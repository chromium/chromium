// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wayland-drm-client-protocol.h>

#include <fcntl.h>
#include <xf86drm.h>

#include "base/files/scoped_file.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/ozone/common/linux/drm_util_linux.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_drm.h"

namespace ui {

WaylandDrm::WaylandDrm(wl_drm* drm, WaylandConnection* connection)
    : wl_drm_(drm), connection_(connection) {
  static const wl_drm_listener kDrmListener = {
      &WaylandDrm::Device,
      &WaylandDrm::Format,
      &WaylandDrm::Authenticated,
      &WaylandDrm::Capabilities,
  };
  wl_drm_add_listener(wl_drm_.get(), &kDrmListener, this);
  connection_->ScheduleFlush();

  // A roundtrip after binding guarantees that the client has received all
  // supported formats and capabilities of the device.
  wl_display_roundtrip(connection_->display());
}

WaylandDrm::~WaylandDrm() = default;

bool WaylandDrm::SupportsDrmPrime() const {
  return authenticated_ && !!wl_drm_;
}

void WaylandDrm::CreateBuffer(base::ScopedFD fd,
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
  uint32_t stride[3] = {0};
  uint32_t offset[3] = {0};
  for (size_t i = 0; i < planes_count; i++) {
    stride[i] = strides[i];
    offset[i] = offset[i];
  }

  wl::Object<wl_buffer> buffer(wl_drm_create_prime_buffer(
      wl_drm_.get(), fd.get(), size.width(), size.height(), format, offset[0],
      stride[0], offset[1], stride[1], offset[2], stride[2]));
  connection_->ScheduleFlush();

  std::move(callback).Run(std::move(buffer));
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

  drm_magic_t magic;
  memset(&magic, 0, sizeof(magic));
  if (drmGetMagic(drm_fd.get(), &magic)) {
    HandleDrmFailure("Failed to get drm magic");
    return;
  }

  wl_drm_authenticate(wl_drm_.get(), magic);
  connection_->ScheduleFlush();

  // Do the roundtrip to make sure the server processes this request and
  // authenticates us.
  wl_display_roundtrip(connection_->display());
}

void WaylandDrm::DrmDeviceAuthenticated(struct wl_drm* wl_drm) {
  DCHECK(wl_drm_ && wl_drm_.get() == wl_drm);
  authenticated_ = true;
}

void WaylandDrm::HandleCapabilities(uint32_t value) {
  if ((value & WL_DRM_CAPABILITY_PRIME) == 0)
    HandleDrmFailure("Drm prime capability is not supported");
}

// static
void WaylandDrm::Device(void* data, struct wl_drm* wl_drm, const char* path) {
  auto* wayland_drm = static_cast<WaylandDrm*>(data);
  DCHECK(wayland_drm && wayland_drm->wl_drm_.get() == wl_drm);
  wayland_drm->Authenticate(path);
}

// static
void WaylandDrm::Format(void* data, struct wl_drm* wl_drm, uint32_t format) {
  auto* wayland_drm = static_cast<WaylandDrm*>(data);
  DCHECK(wayland_drm && wayland_drm->wl_drm_.get() == wl_drm);
  wayland_drm->AddSupportedFourCCFormat(format);
}

// static
void WaylandDrm::Authenticated(void* data, struct wl_drm* wl_drm) {
  auto* wayland_drm = static_cast<WaylandDrm*>(data);
  DCHECK(wayland_drm);
  wayland_drm->DrmDeviceAuthenticated(wl_drm);
}

// static
void WaylandDrm::Capabilities(void* data,
                              struct wl_drm* wl_drm,
                              uint32_t value) {
  auto* wayland_drm = static_cast<WaylandDrm*>(data);
  DCHECK(wayland_drm && wayland_drm->wl_drm_.get() == wl_drm);
  wayland_drm->HandleCapabilities(value);
}

}  // namespace ui
