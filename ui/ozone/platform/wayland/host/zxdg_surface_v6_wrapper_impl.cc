// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/zxdg_surface_v6_wrapper_impl.h"

#include <xdg-shell-unstable-v6-client-protocol.h>

#include "base/logging.h"
#include "base/notreached.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

ZXDGSurfaceV6WrapperImpl::ZXDGSurfaceV6WrapperImpl(
    WaylandWindow* wayland_window,
    WaylandConnection* connection)
    : wayland_window_(wayland_window), connection_(connection) {}

ZXDGSurfaceV6WrapperImpl::~ZXDGSurfaceV6WrapperImpl() {
  is_configured_ = false;
  connection_->wayland_window_manager()->NotifyWindowConfigured(
      wayland_window_);
}

bool ZXDGSurfaceV6WrapperImpl::Initialize() {
  if (!connection_->shell_v6()) {
    NOTREACHED() << "Wrong shell protocol";
    return false;
  }

  static constexpr zxdg_surface_v6_listener zxdg_surface_v6_listener = {
      &Configure,
  };

  zxdg_surface_v6_.reset(zxdg_shell_v6_get_xdg_surface(
      connection_->shell_v6(), wayland_window_->root_surface()->surface()));
  if (!zxdg_surface_v6_) {
    LOG(ERROR) << "Failed to create zxdg_surface";
    return false;
  }

  zxdg_surface_v6_add_listener(zxdg_surface_v6_.get(),
                               &zxdg_surface_v6_listener, this);
  connection_->ScheduleFlush();
  return true;
}

void ZXDGSurfaceV6WrapperImpl::AckConfigure(uint32_t serial) {
  DCHECK(zxdg_surface_v6_);
  zxdg_surface_v6_ack_configure(zxdg_surface_v6_.get(), serial);
  is_configured_ = true;
  connection_->wayland_window_manager()->NotifyWindowConfigured(
      wayland_window_);
}

bool ZXDGSurfaceV6WrapperImpl::IsConfigured() {
  return is_configured_;
}

void ZXDGSurfaceV6WrapperImpl::SetWindowGeometry(const gfx::Rect& bounds) {
  DCHECK(zxdg_surface_v6_);
  zxdg_surface_v6_set_window_geometry(zxdg_surface_v6_.get(), bounds.x(),
                                      bounds.y(), bounds.width(),
                                      bounds.height());
}

// static
void ZXDGSurfaceV6WrapperImpl::Configure(
    void* data,
    struct zxdg_surface_v6* zxdg_surface_v6,
    uint32_t serial) {
  auto* surface = static_cast<ZXDGSurfaceV6WrapperImpl*>(data);
  DCHECK(surface);

  // Calls to HandleSurfaceConfigure() might end up hiding the enclosing
  // toplevel window, and deleting this object.
  auto weak_window = surface->wayland_window_->AsWeakPtr();
  weak_window->HandleSurfaceConfigure(serial);

  if (!weak_window)
    return;

  weak_window->OnSurfaceConfigureEvent();
}

zxdg_surface_v6* ZXDGSurfaceV6WrapperImpl::zxdg_surface() const {
  DCHECK(zxdg_surface_v6_);
  return zxdg_surface_v6_.get();
}

}  // namespace ui
