// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_auxiliary_window.h"

#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_data_drag_controller.h"
#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"

namespace ui {

WaylandAuxiliaryWindow::WaylandAuxiliaryWindow(PlatformWindowDelegate* delegate,
                                               WaylandConnection* connection,
                                               WaylandWindow* parent)
    : WaylandWindow(delegate, connection) {
  set_parent_window(parent);
}

WaylandAuxiliaryWindow::~WaylandAuxiliaryWindow() = default;

void WaylandAuxiliaryWindow::Show(bool inactive) {
  if (subsurface_)
    return;

  CreateSubsurface();
  UpdateBufferScale(false);
  WaylandWindow::Show(inactive);
}

void WaylandAuxiliaryWindow::Hide() {
  if (!subsurface_)
    return;
  WaylandWindow::Hide();

  subsurface_.reset();

  // Detach buffer from surface in order to completely shutdown menus and
  // tooltips, and release resources.
  connection()->buffer_manager_host()->ResetSurfaceContents(root_surface());
}

bool WaylandAuxiliaryWindow::IsVisible() const {
  return !!subsurface_;
}

void WaylandAuxiliaryWindow::SetBounds(const gfx::Rect& bounds) {
  auto old_bounds = GetBounds();
  WaylandWindow::SetBounds(bounds);

  if (old_bounds == bounds || !parent_window() || !subsurface_)
    return;

  auto subsurface_bounds_dip =
      wl::TranslateWindowBoundsToParentDIP(this, parent_window());
  wl_subsurface_set_position(subsurface_.get(), subsurface_bounds_dip.x(),
                             subsurface_bounds_dip.y());
  root_surface()->Commit();
  connection()->ScheduleFlush();
}

void WaylandAuxiliaryWindow::CreateSubsurface() {
  // wl_subsurface can be used for either tooltips or drag arrow windows.
  // If we are in a drag process, the current parent is the entered window, so
  // reparent the surface unconditionally.
  if (connection()->IsDragInProgress())
    set_parent_window(connection()->data_drag_controller()->entered_window());

  if (!parent_window()) {
    LOG(WARNING) << "Parent was not set for the auxiliary window; guessing it!";
    set_parent_window(
        connection()->wayland_window_manager()->GetCurrentFocusedWindow());
  }

  DCHECK(parent_window());

  // We need to make sure that buffer scale matches the parent window.
  UpdateBufferScale(true);

  subsurface_ =
      root_surface()->CreateSubsurface(parent_window()->root_surface());

  auto subsurface_bounds_dip =
      wl::TranslateWindowBoundsToParentDIP(this, parent_window());

  DCHECK(subsurface_);
  // Convert position to DIP.
  wl_subsurface_set_position(subsurface_.get(), subsurface_bounds_dip.x(),
                             subsurface_bounds_dip.y());
  wl_subsurface_set_desync(subsurface_.get());
  parent_window()->root_surface()->Commit();
  connection()->ScheduleFlush();

  // Notify the observers the window has been configured. Please note that
  // subsurface doesn't send ack configure events. Thus, notify the observers as
  // soon as the subsurface is created.
  connection()->wayland_window_manager()->NotifyWindowConfigured(this);
}

bool WaylandAuxiliaryWindow::OnInitialize(
    PlatformWindowInitProperties properties) {
  return true;
}

}  // namespace ui
