// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/xdg_surface_wrapper_v5.h"

#include <xdg-shell-unstable-v5-client-protocol.h>

#include "base/strings/utf_string_conversions.h"
#include "ui/base/hit_test.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_util.h"
#include "ui/ozone/platform/wayland/wayland_window.h"

namespace ui {

XDGSurfaceWrapperV5::XDGSurfaceWrapperV5(WaylandWindow* wayland_window)
    : wayland_window_(wayland_window) {}

XDGSurfaceWrapperV5::~XDGSurfaceWrapperV5() {}

bool XDGSurfaceWrapperV5::Initialize(WaylandConnection* connection,
                                     wl_surface* surface,
                                     bool with_toplevel) {
  static const xdg_surface_listener xdg_surface_listener = {
      &XDGSurfaceWrapperV5::Configure, &XDGSurfaceWrapperV5::Close,
  };
  xdg_surface_.reset(xdg_shell_get_xdg_surface(connection->shell(), surface));
  if (!xdg_surface_) {
    LOG(ERROR) << "Failed to create xdg_surface";
    return false;
  }
  xdg_surface_add_listener(xdg_surface_.get(), &xdg_surface_listener, this);
  return true;
}

void XDGSurfaceWrapperV5::SetMaximized() {
  xdg_surface_set_maximized(xdg_surface_.get());
}

void XDGSurfaceWrapperV5::UnSetMaximized() {
  xdg_surface_unset_maximized(xdg_surface_.get());
}

void XDGSurfaceWrapperV5::SetFullscreen() {
  xdg_surface_set_fullscreen(xdg_surface_.get(), nullptr);
}

void XDGSurfaceWrapperV5::UnSetFullscreen() {
  xdg_surface_unset_fullscreen(xdg_surface_.get());
}

void XDGSurfaceWrapperV5::SetMinimized() {
  xdg_surface_set_minimized(xdg_surface_.get());
}

void XDGSurfaceWrapperV5::SurfaceMove(WaylandConnection* connection) {
  xdg_surface_move(xdg_surface_.get(), connection->seat(),
                   connection->serial());
}

void XDGSurfaceWrapperV5::SurfaceResize(WaylandConnection* connection,
                                        uint32_t hittest) {
  xdg_surface_resize(xdg_surface_.get(), connection->seat(),
                     connection->serial(),
                     wl::IdentifyDirection(*connection, hittest));
}

void XDGSurfaceWrapperV5::SetTitle(const base::string16& title) {
  xdg_surface_set_title(xdg_surface_.get(), base::UTF16ToUTF8(title).c_str());
}

void XDGSurfaceWrapperV5::AckConfigure() {
  xdg_surface_ack_configure(xdg_surface_.get(), pending_configure_serial_);
}

void XDGSurfaceWrapperV5::SetWindowGeometry(const gfx::Rect& bounds) {
  xdg_surface_set_window_geometry(xdg_surface_.get(), bounds.x(), bounds.y(),
                                  bounds.width(), bounds.height());
}

// static
void XDGSurfaceWrapperV5::Configure(void* data,
                                    xdg_surface* obj,
                                    int32_t width,
                                    int32_t height,
                                    wl_array* states,
                                    uint32_t serial) {
  XDGSurfaceWrapperV5* surface = static_cast<XDGSurfaceWrapperV5*>(data);

  bool is_maximized =
      CheckIfWlArrayHasValue(states, XDG_SURFACE_STATE_MAXIMIZED);
  bool is_fullscreen =
      CheckIfWlArrayHasValue(states, XDG_SURFACE_STATE_FULLSCREEN);
  bool is_activated =
      CheckIfWlArrayHasValue(states, XDG_SURFACE_STATE_ACTIVATED);

  surface->pending_configure_serial_ = serial;
  surface->wayland_window_->HandleSurfaceConfigure(width, height, is_maximized,
                                                   is_fullscreen, is_activated);
}

// static
void XDGSurfaceWrapperV5::Close(void* data, xdg_surface* obj) {
  NOTIMPLEMENTED();
}

}  // namespace ui
