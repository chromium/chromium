// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/xdg_surface.h"

#include <xdg-shell-client-protocol.h>

#include "base/logging.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

XdgSurface::XdgSurface(WaylandWindow* wayland_window,
                       WaylandConnection* connection)
    : wayland_window_(wayland_window), connection_(connection) {}

XdgSurface::~XdgSurface() {
  is_configured_ = false;
  connection_->window_manager()->NotifyWindowConfigured(wayland_window_);
}

bool XdgSurface::Initialize() {
  CHECK(connection_->shell()) << "No xdg-shell protocol available.";

  xdg_surface_.reset(xdg_wm_base_get_xdg_surface(
      connection_->shell(), wayland_window_->root_surface()->surface()));
  if (!xdg_surface_) {
    LOG(ERROR) << "Failed to create xdg_surface";
    return false;
  }

  static constexpr xdg_surface_listener kXdgSurfaceListener = {
      .configure = &OnConfigure,
  };
  xdg_surface_add_listener(xdg_surface_.get(), &kXdgSurfaceListener, this);

  connection_->Flush();
  return true;
}

void XdgSurface::AckConfigure(uint32_t serial) {
  // We must not ack any serial more than once, so check for that here.
  DCHECK_LE(last_acked_serial_, serial);
  if (serial == last_acked_serial_) {
    return;
  }

  last_acked_serial_ = serial;
  DCHECK(xdg_surface_);
  xdg_surface_ack_configure(xdg_surface_.get(), serial);

  is_configured_ = true;
  connection_->window_manager()->NotifyWindowConfigured(wayland_window_);
}

bool XdgSurface::IsConfigured() {
  return is_configured_;
}

void XdgSurface::SetWindowGeometry(const gfx::Rect& bounds) {
  DCHECK(xdg_surface_);
  CHECK(!bounds.IsEmpty()) << "The xdg-shell protocol specification forbids "
                              "empty bounds (zero width or height). bounds="
                           << bounds.ToString();
  xdg_surface_set_window_geometry(xdg_surface_.get(), bounds.x(), bounds.y(),
                                  bounds.width(), bounds.height());
}

// static
void XdgSurface::OnConfigure(void* data,
                             struct xdg_surface* surface,
                             uint32_t serial) {
  auto* self = static_cast<XdgSurface*>(data);
  DCHECK(self);

  // Calls to HandleSurfaceConfigure() might end up hiding the enclosing
  // toplevel window, and deleting this object.
  auto weak_window = self->wayland_window_->AsWeakPtr();
  weak_window->HandleSurfaceConfigure(serial);

  if (!weak_window) {
    return;
  }

  weak_window->OnSurfaceConfigureEvent();
}

}  // namespace ui
