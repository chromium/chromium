// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <wayland-drm-client-protocol.h>

#include <fcntl.h>
#include <xf86drm.h>

#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_factory.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_drm.h"
#include "wayland-util.h"

namespace ui {

namespace {
constexpr uint32_t kMinVersion = 2;
}

// static
constexpr char WaylandDrm::kInterfaceName[];

// static
void WaylandDrm::Instantiate(WaylandConnection* connection,
                             wl_registry* registry,
                             uint32_t name,
                             const std::string& interface,
                             uint32_t version) {
  CHECK_EQ(interface, kInterfaceName) << "Expected \"" << kInterfaceName
                                      << "\" but got \"" << interface << "\"";

  auto* buffer_factory = connection->buffer_factory();
  if (buffer_factory->wayland_drm_ ||
      !wl::CanBind(interface, version, kMinVersion, kMinVersion)) {
    return;
  }

  auto wl_obj = wl::Bind<wl_drm>(registry, name, version);
  if (!wl_obj) {
    LOG(ERROR) << "Failed to bind wl_drm";
    return;
  }
  buffer_factory->wayland_drm_ =
      std::make_unique<WaylandDrm>(wl_obj.release(), connection);
}

WaylandDrm::WaylandDrm(wl_drm* drm, WaylandConnection* connection)
    : wl_drm_(drm), connection_(connection) {
  static constexpr wl_drm_listener kDrmListener = {
      .device = &OnDevice,
      .format = &OnFormat,
      .authenticated = &OnAuthenticated,
      .capabilities = &OnCapabilities,
  };
  wl_drm_add_listener(wl_drm_.get(), &kDrmListener, this);
  connection_->Flush();

  // A roundtrip after binding guarantees that the client has received all
  // supported formats and capabilities of the device.
  connection_->RoundTripQueue();
}

WaylandDrm::~WaylandDrm() = default;

bool WaylandDrm::SupportsDrmPrime() const {
  return authenticated_ && !!wl_drm_;
}

void WaylandDrm::CreateBuffer(const base::ScopedFD& fd,
                              const gfx::Size& size,
                              const std::vector<uint32_t>& strides,
                              const std::vector<uint32_t>& offsets,
                              const std::vector<uint64_t>& modifiers,
                              uint32_t format,
                              uint32_t planes_count,
                              wl::OnRequestBufferCallback callback) {
  // If the |planes_count| less than the maximum sizes of these arrays and the
  // number of offsets and strides that |wl_drm| can receive, just initialize
  // them to 0, which is totally ok.
  std::array<uint32_t, 3> stride = {0};
  std::array<uint32_t, 3> offset = {0};
  for (size_t i = 0; i < planes_count; i++) {
    stride[i] = strides[i];
    offset[i] = offset[i];
  }

  wl::Object<wl_buffer> buffer(wl_drm_create_prime_buffer(
      wl_drm_.get(), fd.get(), size.width(), size.height(), format, offset[0],
      stride[0], offset[1], stride[1], offset[2], stride[2]));
  connection_->Flush();

  std::move(callback).Run(std::move(buffer));
}

bool WaylandDrm::CanCreateBufferImmed() const {
  // Unlike the WaylandZwpLinuxDmabuf, the WaylandDrm always creates wl_buffers
  // immediately.
  return true;
}

void WaylandDrm::HandleDrmFailure(const std::string& error) {
  LOG(WARNING) << error;
  wl_drm_.reset();
}

void WaylandDrm::AddSupportedFourCCFormat(uint32_t fourcc_format) {
  // Return on unsupported fourcc formats.
  if (!IsValidBufferFormat(fourcc_format))
    return;

  gfx::BufferFormat format = GetBufferFormatFromFourCCFormat(fourcc_format);
  // Modifiers are not supported by the |wl_drm|, but for consistency with the
  // WaylandZwpLinuxDmabuf we use the same map type, which is passed to the
  // WaylandBufferManagerGpu later during initialization stage of the GPU
  // process.
  std::vector<uint64_t> modifiers;
  supported_buffer_formats_.emplace(format, std::move(modifiers));
}

void WaylandDrm::Authenticate(const char* drm_device_path) {
  if (!wl_drm_)
    return;

  DCHECK(drm_device_path);
  base::ScopedFD drm_fd(open(drm_device_path, O_RDWR));
  if (!drm_fd.is_valid()) {
    HandleDrmFailure("Drm open failed: " + std::string(drm_device_path));
    return;
  }

  if (drmGetNodeTypeFromFd(drm_fd.get()) != DRM_NODE_PRIMARY) {
    DrmDeviceAuthenticated(wl_drm_.get());
    return;
  }

  drm_magic_t magic;
  memset(&magic, 0, sizeof(magic));
  if (drmGetMagic(drm_fd.get(), &magic)) {
    HandleDrmFailure("Failed to get drm magic");
    return;
  }

  wl_drm_authenticate(wl_drm_.get(), magic);
  connection_->Flush();

  // Do the roundtrip to make sure the server processes this request and
  // authenticates us.
  connection_->RoundTripQueue();
}

void WaylandDrm::DrmDeviceAuthenticated(wl_drm* drm) {
  DCHECK(wl_drm_ && wl_drm_.get() == drm);
  authenticated_ = true;
}

void WaylandDrm::HandleCapabilities(uint32_t value) {
  if ((value & WL_DRM_CAPABILITY_PRIME) == 0) {
    HandleDrmFailure("Drm prime capability is not supported");
  }
}

// static
void WaylandDrm::OnDevice(void* data, wl_drm* drm, const char* path) {
  auto* self = static_cast<WaylandDrm*>(data);
  DCHECK(self && self->wl_drm_.get() == drm);
  self->Authenticate(path);
}

// static
void WaylandDrm::OnFormat(void* data, wl_drm* drm, uint32_t format) {
  auto* self = static_cast<WaylandDrm*>(data);
  DCHECK(self && self->wl_drm_.get() == drm);
  self->AddSupportedFourCCFormat(format);
}

// static
void WaylandDrm::OnAuthenticated(void* data, wl_drm* drm) {
  auto* self = static_cast<WaylandDrm*>(data);
  DCHECK(self);
  self->DrmDeviceAuthenticated(drm);
}

// static
void WaylandDrm::OnCapabilities(void* data, wl_drm* drm, uint32_t value) {
  auto* self = static_cast<WaylandDrm*>(data);
  DCHECK(self && self->wl_drm_.get() == drm);
  self->HandleCapabilities(value);
}

}  // namespace ui
