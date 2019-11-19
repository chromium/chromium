// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/xdg_popup_wrapper_v5.h"
#include <xdg-shell-unstable-v5-client-protocol.h>
#include <vector>

#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

XDGPopupWrapperV5::XDGPopupWrapperV5(WaylandWindow* wayland_window)
    : wayland_window_(wayland_window) {}

XDGPopupWrapperV5::~XDGPopupWrapperV5() {
  wl_surface_attach(surface_, NULL, 0, 0);
  wl_surface_commit(surface_);
}

bool XDGPopupWrapperV5::Initialize(WaylandConnection* connection,
                                   wl_surface* surface,
                                   WaylandWindow* parent_window,
                                   const gfx::Rect& bounds) {
  DCHECK(connection && surface && parent_window);
  static const xdg_popup_listener xdg_popup_listener = {
      &XDGPopupWrapperV5::PopupDone,
  };

  DCHECK(!surface_);
  surface_ = surface;
  xdg_popup_.reset(xdg_shell_get_xdg_popup(
      connection->shell(), surface, parent_window->surface(),
      connection->seat(), connection->serial(), bounds.x(), bounds.y()));

  xdg_popup_add_listener(xdg_popup_.get(), &xdg_popup_listener, this);

  // xdg_popup_v5 does not support configure events. Thus, manually call it to
  // propagate final bounds.
  wayland_window_->HandlePopupConfigure(bounds);

  return true;
}

// static
void XDGPopupWrapperV5::PopupDone(void* data, xdg_popup* obj) {
  WaylandWindow* window =
      static_cast<XDGPopupWrapperV5*>(data)->wayland_window_;
  DCHECK(window);
  window->Hide();
  window->OnCloseRequest();
}

}  // namespace ui
