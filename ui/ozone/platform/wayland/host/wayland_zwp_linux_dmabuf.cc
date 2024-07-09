// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zwp_linux_dmabuf.h"

#include <drm_fourcc.h>
#include <linux-dmabuf-unstable-v1-client-protocol.h>

#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_factory.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

namespace {
constexpr uint32_t kMinVersion = 1;
constexpr uint32_t kMaxVersion = 3;
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

  // A roundtrip after binding guarantees that the client has received all
  // supported formats.
  connection_->RoundTripQueue();
}

WaylandZwpLinuxDmabuf::~WaylandZwpLinuxDmabuf() = default;

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
  if (!IsValidBufferFormat(fourcc_format))
    return;

  uint64_t format_modifier = modifier.value_or(DRM_FORMAT_MOD_INVALID);

  // If the buffer format has already been stored, it must be another supported
  // modifier sent by the Wayland compositor.
  gfx::BufferFormat format = GetBufferFormatFromFourCCFormat(fourcc_format);
  auto it = supported_buffer_formats_with_modifiers_.find(format);
  if (it != supported_buffer_formats_with_modifiers_.end()) {
    if (format_modifier != DRM_FORMAT_MOD_INVALID)
      it->second.emplace_back(format_modifier);
    return;
  } else {
    std::vector<uint64_t> modifiers;
    if (format_modifier != DRM_FORMAT_MOD_INVALID)
      modifiers.emplace_back(format_modifier);
    supported_buffer_formats_with_modifiers_.emplace(format,
                                                     std::move(modifiers));
  }
}

void WaylandZwpLinuxDmabuf::NotifyRequestCreateBufferDone(
    zwp_linux_buffer_params_v1* params,
    wl_buffer* new_buffer) {
  auto it = base::ranges::find(pending_params_, params, [](const auto& item) {
    return item.first.get();
  });
  CHECK(it != pending_params_.end(), base::NotFatalUntil::M130);
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
    self->AddSupportedFourCCFormatAndModifier(format, {modifier});
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

}  // namespace ui
