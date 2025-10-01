// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zwp_linux_dmabuf.h"

#include <drm_fourcc.h>
#include <fcntl.h>
#include <linux-dmabuf-unstable-v1-client-protocol.h>
#include <sys/mman.h>
#include <xf86drm.h>

#include <algorithm>
#include <cstring>

#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/timer/elapsed_timer.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_factory.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

namespace {
constexpr uint32_t kMinVersion = 1;
constexpr uint32_t kMaxVersion = 4;

struct DrmDeviceDeleter {
  void operator()(drmDevice* device) { drmFreeDevice(&device); }
};
using ScopedDrmDevice = std::unique_ptr<drmDevice, DrmDeviceDeleter>;

}  // namespace

// static
constexpr char WaylandZwpLinuxDmabuf::kInterfaceName[];

// static
void WaylandZwpLinuxDmabuf::Instantiate(WaylandConnection* connection,
                                        wl_registry* registry,
                                        uint32_t name,
                                        const std::string& interface,
                                        uint32_t version) {
  CHECK_EQ(interface, kInterfaceName) << "Expected \"" << kInterfaceName
                                      << "\" but got \"" << interface << "\"";
  auto* buffer_factory = connection->buffer_factory();
  if (buffer_factory->wayland_zwp_dmabuf_ ||
      !wl::CanBind(interface, version, kMinVersion, kMaxVersion)) {
    return;
  }

  auto zwp_linux_dmabuf = wl::Bind<zwp_linux_dmabuf_v1>(
      registry, name, std::min(version, kMaxVersion));
  if (!zwp_linux_dmabuf) {
    LOG(ERROR) << "Failed to bind zwp_linux_dmabuf_v1";
    return;
  }
  buffer_factory->wayland_zwp_dmabuf_ = std::make_unique<WaylandZwpLinuxDmabuf>(
      zwp_linux_dmabuf.release(), connection);
}

WaylandZwpLinuxDmabuf::WaylandZwpLinuxDmabuf(
    zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
    WaylandConnection* connection)
    : zwp_linux_dmabuf_(zwp_linux_dmabuf), connection_(connection) {
  static constexpr zwp_linux_dmabuf_v1_listener kDmabufListener = {
      .format = &OnFormat,
      .modifier = &OnModifiers,
  };
  zwp_linux_dmabuf_v1_add_listener(zwp_linux_dmabuf_.get(), &kDmabufListener,
                                   this);

  if (wl::get_version_of_object(zwp_linux_dmabuf_.get()) >=
      ZWP_LINUX_DMABUF_V1_GET_DEFAULT_FEEDBACK_SINCE_VERSION) {
    zwp_linux_dmabuf_feedback_.reset(
        zwp_linux_dmabuf_v1_get_default_feedback(zwp_linux_dmabuf_.get()));
    static constexpr zwp_linux_dmabuf_feedback_v1_listener
        kDmabufFeedbackListener = {
            .done = &OnDone,
            .format_table = &OnFormatTable,
            .main_device = &OnMainDevice,
            .tranche_done = &OnTrancheDone,
            .tranche_target_device = &OnTrancheTargetDevice,
            .tranche_formats = &OnTrancheFormats,
            .tranche_flags = &OnTrancheFlags,
        };
    zwp_linux_dmabuf_feedback_v1_add_listener(zwp_linux_dmabuf_feedback_.get(),
                                              &kDmabufFeedbackListener, this);
  }

  // A roundtrip after binding guarantees that the client has received all
  // supported formats.
  base::ElapsedTimer timer;
  do {
    connection_->RoundTripQueue();
  } while (zwp_linux_dmabuf_feedback_.get() &&
           timer.Elapsed() < base::Milliseconds(500));
}

WaylandZwpLinuxDmabuf::~WaylandZwpLinuxDmabuf() = default;

WaylandZwpLinuxDmabuf::Tranche::Tranche() = default;

WaylandZwpLinuxDmabuf::Tranche::~Tranche() = default;

void WaylandZwpLinuxDmabuf::CreateBuffer(const base::ScopedFD& fd,
                                         const gfx::Size& size,
                                         const std::vector<uint32_t>& strides,
                                         const std::vector<uint32_t>& offsets,
                                         const std::vector<uint64_t>& modifiers,
                                         uint32_t format,
                                         uint32_t planes_count,
                                         wl::OnRequestBufferCallback callback) {
  // Params will be destroyed immediately if create_immed is available.
  // Otherwise, they will be destroyed after Wayland notifies a new buffer is
  // created or failed to be created.
  wl::Object<zwp_linux_buffer_params_v1> params(
      zwp_linux_dmabuf_v1_create_params(zwp_linux_dmabuf_.get()));

  for (size_t i = 0; i < planes_count; i++) {
    zwp_linux_buffer_params_v1_add(params.get(), fd.get(), i /* plane id */,
                                   offsets[i], strides[i], modifiers[i] >> 32,
                                   modifiers[i] & UINT32_MAX);
  }

  // It's possible to avoid waiting until the buffer is created and have it
  // immediately. This method is only available since the protocol version 2.
  if (CanCreateBufferImmed()) {
    wl::Object<wl_buffer> buffer(zwp_linux_buffer_params_v1_create_immed(
        params.get(), size.width(), size.height(), format, 0));
    std::move(callback).Run(std::move(buffer));
  } else {
    static constexpr zwp_linux_buffer_params_v1_listener
        kLinuxBufferParamsListener = {.created = &OnCreated,
                                      .failed = &OnFailed};
    zwp_linux_buffer_params_v1_add_listener(params.get(),
                                            &kLinuxBufferParamsListener, this);
    zwp_linux_buffer_params_v1_create(params.get(), size.width(), size.height(),
                                      format, 0);

    // Store the |params| with the corresponding |callback| to identify newly
    // created buffer and notify the client about it via the |callback|.
    pending_params_.emplace(std::move(params), std::move(callback));
  }
  connection_->Flush();
}

bool WaylandZwpLinuxDmabuf::CanCreateBufferImmed() const {
  return wl::get_version_of_object(zwp_linux_dmabuf_.get()) >=
         ZWP_LINUX_BUFFER_PARAMS_V1_CREATE_IMMED_SINCE_VERSION;
}

void WaylandZwpLinuxDmabuf::AddSupportedFourCCFormatAndModifier(
    uint32_t fourcc_format,
    std::optional<uint64_t> modifier) {
  // Return on not supported fourcc formats.
  if (!IsValidBufferFormat(fourcc_format)) {
    return;
  }

  // If the buffer format has already been stored, it must be another supported
  // modifier sent by the Wayland compositor.
  gfx::BufferFormat format = GetBufferFormatFromFourCCFormat(fourcc_format);
  auto it = supported_buffer_formats_with_modifiers_.find(format);
  if (it == supported_buffer_formats_with_modifiers_.end()) {
    std::vector<uint64_t> modifiers;
    if (modifier.has_value()) {
      modifiers.emplace_back(modifier.value());
    }
    supported_buffer_formats_with_modifiers_.emplace(format,
                                                     std::move(modifiers));
  } else if (modifier.has_value()) {
    it->second.emplace_back(modifier.value());
  }
}

void WaylandZwpLinuxDmabuf::NotifyRequestCreateBufferDone(
    zwp_linux_buffer_params_v1* params,
    wl_buffer* new_buffer) {
  auto it = std::ranges::find(pending_params_, params, [](const auto& item) {
    return item.first.get();
  });
  CHECK(it != pending_params_.end());
  std::move(it->second).Run(wl::Object<wl_buffer>(new_buffer));
  pending_params_.erase(it);
  connection_->Flush();
}

// static
void WaylandZwpLinuxDmabuf::OnModifiers(void* data,
                                        zwp_linux_dmabuf_v1* linux_dmabuf,
                                        uint32_t format,
                                        uint32_t modifier_hi,
                                        uint32_t modifier_lo) {
  if (auto* self = static_cast<WaylandZwpLinuxDmabuf*>(data)) {
    uint64_t modifier = static_cast<uint64_t>(modifier_hi) << 32 | modifier_lo;
    self->AddSupportedFourCCFormatAndModifier(format, modifier);
  }
}

// static
void WaylandZwpLinuxDmabuf::OnFormat(void* data,
                                     zwp_linux_dmabuf_v1* linux_dmabuf,
                                     uint32_t format) {
  if (auto* self = static_cast<WaylandZwpLinuxDmabuf*>(data)) {
    self->AddSupportedFourCCFormatAndModifier(format, std::nullopt);
  }
}

// static
void WaylandZwpLinuxDmabuf::OnCreated(void* data,
                                      zwp_linux_buffer_params_v1* params,
                                      wl_buffer* new_buffer) {
  if (auto* self = static_cast<WaylandZwpLinuxDmabuf*>(data)) {
    self->NotifyRequestCreateBufferDone(params, new_buffer);
  }
}

// static
void WaylandZwpLinuxDmabuf::OnFailed(void* data,
                                     zwp_linux_buffer_params_v1* params) {
  if (auto* self = static_cast<WaylandZwpLinuxDmabuf*>(data)) {
    self->NotifyRequestCreateBufferDone(params, nullptr);
  }
}

void WaylandZwpLinuxDmabuf::OnDone(void* data,
                                   zwp_linux_dmabuf_feedback_v1* feedback) {
  if (auto* self = static_cast<WaylandZwpLinuxDmabuf*>(data)) {
    self->zwp_linux_dmabuf_feedback_.reset();
  }
}

void WaylandZwpLinuxDmabuf::OnFormatTable(
    void* data,
    zwp_linux_dmabuf_feedback_v1* feedback,
    int32_t fd,
    uint32_t size) {
  auto* self = static_cast<WaylandZwpLinuxDmabuf*>(data);
  if (!self) {
    return;
  }

  // If the compositor wants to change the available formats later, it must
  // send a new format table and resend all feedback parameters. Clean up any
  // existing table and formats to make room for the new ones.
  self->format_table_.clear();
  self->supported_buffer_formats_with_modifiers_.clear();

  void* format_ptr = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (format_ptr == MAP_FAILED) {
    LOG(ERROR) << "Failed to map zwp_linux_dmabuf_feedback_v1 format table";
    return;
  }

  CHECK_EQ(size % 16, 0UL);
  // SAFETY: The display server provides the format table as a memory-mappable
  // file that is meant to be mapped in read-only private mode. The display
  // server is not allowed to mutate the table, and must send a new one. The
  // CHECK_EQ above ensures size is a multiple of 16 bytes which we will
  // address below as sets of 4 uint32's.
  uint32_t* formats = UNSAFE_BUFFERS(static_cast<uint32_t*>(format_ptr));
  uint32_t count = size / 4;

  // The format table is a tightly packed array of native-endianness
  // format/modifier pairs:
  //
  //    [ 32-bit format | 32-bit padding | 64-bit modifier ]
  //
  for (uint32_t idx = 0; idx < count; idx += 4) {
    FormatModifierPair pair;

    // SAFETY: We have tested the length above to ensure that there will be 16
    // bytes (4 uint32_t's) available to read, and the protocol defines these
    // as native-endianness, most easily read through memcpy.
    UNSAFE_BUFFERS({
      pair.format = formats[idx];
      memcpy(&pair.modifier, &formats[idx + 2], sizeof(uint64_t));
    });
    self->format_table_.push_back(pair);
  }

  munmap(format_ptr, size);
}

void WaylandZwpLinuxDmabuf::OnMainDevice(void* data,
                                         zwp_linux_dmabuf_feedback_v1* feedback,
                                         struct wl_array* device) {
#if defined(WAYLAND_GBM)
  auto* self = static_cast<WaylandZwpLinuxDmabuf*>(data);
  if (!self) {
    return;
  }

  CHECK_EQ(device->size, sizeof(dev_t));
  // SAFETY: wl_array is managed by wayland connection that invokes this
  // listener, and the CHECK above ensures there is 1 element in the wl_array.
  self->main_dev_ = UNSAFE_BUFFERS(reinterpret_cast<dev_t*>(device->data)[0]);
  drmDevicePtr raw_device;
  int ret = drmGetDeviceFromDevId(self->main_dev_, 0, &raw_device);
  if (ret < 0) {
    PLOG(ERROR) << "drmGetDeviceFromDevId() returned an error";
    return;
  }
  ScopedDrmDevice drm_device(raw_device);

  if (!drm_device || !(drm_device->available_nodes & 1 << DRM_NODE_RENDER)) {
    return;
  }
  CHECK(drm_device->nodes);

  // SAFETY: drmDevice.nodes is a DRM_NODE_MAX sized array.
  const char* drm_device_path =
      UNSAFE_BUFFERS((drm_device.get()->nodes[DRM_NODE_RENDER]));
  base::ScopedFD drm_fd(open(drm_device_path, O_RDWR));

  self->connection_->SetRenderNodePath(drm_fd, drm_device_path);

  // Prepare to receive new formats and modifiers
  self->supported_buffer_formats_with_modifiers_.clear();
#endif  // defined(WAYLAND_GBM)
}

void WaylandZwpLinuxDmabuf::OnTrancheDone(
    void* data,
    zwp_linux_dmabuf_feedback_v1* feedback) {
  auto* self = static_cast<WaylandZwpLinuxDmabuf*>(data);
  if (!self) {
    return;
  }

  if (self->pending_tranche_.target_device == self->main_dev_) {
    for (const auto& table_index : self->pending_tranche_.formats) {
      CHECK_LE(table_index, self->format_table_.size());
      auto format_pair = self->format_table_[table_index];
      self->AddSupportedFourCCFormatAndModifier(format_pair.format,
                                                {format_pair.modifier});
    }
  }

  self->pending_tranche_ = {};
}

void WaylandZwpLinuxDmabuf::OnTrancheTargetDevice(
    void* data,
    zwp_linux_dmabuf_feedback_v1* feedback,
    struct wl_array* device) {
  auto* self = static_cast<WaylandZwpLinuxDmabuf*>(data);
  if (!self) {
    return;
  }

  CHECK_EQ(device->size, sizeof(dev_t));
  // SAFETY: wl_array is managed by wayland connection that invokes this
  // listener, and the CHECK above ensures there is 1 element in the wl_array.
  dev_t dev = UNSAFE_BUFFERS(reinterpret_cast<dev_t*>(device->data)[0]);
  self->pending_tranche_.target_device = dev;
}

void WaylandZwpLinuxDmabuf::OnTrancheFormats(
    void* data,
    zwp_linux_dmabuf_feedback_v1* feedback,
    struct wl_array* indices) {
  auto* self = static_cast<WaylandZwpLinuxDmabuf*>(data);
  if (!self) {
    return;
  }

  if (indices->size == 0) {
    return;
  }

  CHECK_EQ(indices->size % sizeof(uint16_t), 0UL);
  // SAFETY: wl_array is managed by wayland connection that invokes this
  // listener, the length of the array is checked above to be positive and the
  // CHECK_EQ ensures that it contains an integer number of uint16_t indices.
  size_t count = indices->size / sizeof(uint16_t);
  uint16_t* is = UNSAFE_BUFFERS(static_cast<uint16_t*>(indices->data));
  for (size_t idx = 0; idx < count; idx++) {
    // SAFETY: We have validated the length to contain an integer number of
    // uint16_t's
    uint16_t table_index = UNSAFE_BUFFERS(is[idx]);
    self->pending_tranche_.formats.push_back(table_index);
  }
}

void WaylandZwpLinuxDmabuf::OnTrancheFlags(
    void* data,
    zwp_linux_dmabuf_feedback_v1* feedback,
    uint32_t flags) {
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace ui
