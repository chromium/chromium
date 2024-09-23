// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_buffer_factory.h"

#include <memory>

#include "ui/ozone/platform/wayland/host/wayland_drm.h"
#include "ui/ozone/platform/wayland/host/wayland_zwp_linux_dmabuf.h"

namespace ui {

WaylandBufferFactory::WaylandBufferFactory() = default;

WaylandBufferFactory::~WaylandBufferFactory() = default;

void WaylandBufferFactory::CreateDmabufBuffer(
    const base::ScopedFD& fd,
    const gfx::Size& size,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    const std::vector<uint64_t>& modifiers,
    uint32_t format,
    uint32_t planes_count,
    wl::OnRequestBufferCallback callback) const {
  DCHECK(SupportsDmabuf());
  if (wayland_zwp_dmabuf_) {
    wayland_zwp_dmabuf_->CreateBuffer(fd, size, strides, offsets, modifiers,
                                      format, planes_count,
                                      std::move(callback));
  } else if (wayland_drm_) {
    wayland_drm_->CreateBuffer(fd, size, strides, offsets, modifiers, format,
                               planes_count, std::move(callback));
  } else {
    // This method must never be called if neither zwp_linux_dmabuf or wl_drm
    // are supported.
    NOTREACHED_IN_MIGRATION();
  }
}

wl::Object<struct wl_buffer> WaylandBufferFactory::CreateShmBuffer(
    const base::ScopedFD& fd,
    size_t length,
    const gfx::Size& size,
    bool with_alpha_channel) const {
  if (!wayland_shm_) [[unlikely]] {
    return {};
  }
  return wayland_shm_->CreateBuffer(fd, length, size, with_alpha_channel);
}

wl::BufferFormatsWithModifiersMap
WaylandBufferFactory::GetSupportedBufferFormats() const {
#if defined(WAYLAND_GBM)
  if (wayland_zwp_dmabuf_)
    return wayland_zwp_dmabuf_->supported_buffer_formats();
  else if (wayland_drm_)
    return wayland_drm_->supported_buffer_formats();
#endif
  return {};
}

bool WaylandBufferFactory::SupportsDmabuf() const {
#if defined(WAYLAND_GBM)
  return !!wayland_zwp_dmabuf_ ||
         (wayland_drm_ && wayland_drm_->SupportsDrmPrime());
#else
  return false;
#endif
}

bool WaylandBufferFactory::CanCreateDmabufImmed() const {
#if defined(WAYLAND_GBM)
  if (wayland_zwp_dmabuf_)
    return wayland_zwp_dmabuf_->CanCreateBufferImmed();
  else if (wayland_drm_)
    return wayland_drm_->CanCreateBufferImmed();
#endif
  return false;
}

}  // namespace ui
