// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/xdg_surface_wrapper_impl.h"

#include <xdg-shell-client-protocol.h>

#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

XDGSurfaceWrapperImpl::XDGSurfaceWrapperImpl(WaylandWindow* wayland_window,
                                             WaylandConnection* connection)
    : wayland_window_(wayland_window), connection_(connection) {}

XDGSurfaceWrapperImpl::~XDGSurfaceWrapperImpl() = default;

bool XDGSurfaceWrapperImpl::Initialize() {
  if (!connection_->shell()) {
    NOTREACHED() << "Wrong shell protocol";
    return false;
  }

  static const xdg_surface_listener xdg_surface_listener = {
      &XDGSurfaceWrapperImpl::Configure,
  };

  xdg_surface_.reset(xdg_wm_base_get_xdg_surface(
      connection_->shell(), wayland_window_->root_surface()->surface()));
  if (!xdg_surface_) {
    LOG(ERROR) << "Failed to create xdg_surface";
    return false;
  }

  xdg_surface_add_listener(xdg_surface_.get(), &xdg_surface_listener, this);
  connection_->ScheduleFlush();
  return true;
}

void XDGSurfaceWrapperImpl::AckConfigure(uint32_t serial) {
  DCHECK(xdg_surface_);
  xdg_surface_ack_configure(xdg_surface_.get(), serial);
  connection_->wayland_window_manager()->NotifyWindowConfigured(
      wayland_window_);
}

void XDGSurfaceWrapperImpl::SetWindowGeometry(const gfx::Rect& bounds) {
  DCHECK(xdg_surface_);
  xdg_surface_set_window_geometry(xdg_surface_.get(), bounds.x(), bounds.y(),
                                  bounds.width(), bounds.height());
}

// static
void XDGSurfaceWrapperImpl::Configure(void* data,
                                      struct xdg_surface* xdg_surface,
                                      uint32_t serial) {
  auto* surface = static_cast<XDGSurfaceWrapperImpl*>(data);
  DCHECK(surface);

  surface->wayland_window_->HandleSurfaceConfigure(serial);
}

xdg_surface* XDGSurfaceWrapperImpl::xdg_surface() const {
  DCHECK(xdg_surface_);
  return xdg_surface_.get();
}

}  // namespace ui
