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
                                               WaylandConnection* connection)
    : WaylandWindow(delegate, connection) {}

WaylandAuxiliaryWindow::~WaylandAuxiliaryWindow() = default;

void WaylandAuxiliaryWindow::Show(bool inactive) {
  if (subsurface_)
    return;

  CreateSubsurface();
  UpdateBufferScale(false);
}

void WaylandAuxiliaryWindow::Hide() {
  if (!subsurface_)
    return;

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

  if (old_bounds == bounds || !parent_window())
    return;

  auto subsurface_bounds_dip =
      wl::TranslateWindowBoundsToParentDIP(this, parent_window());
  wl_subsurface_set_position(subsurface_.get(), subsurface_bounds_dip.x(),
                             subsurface_bounds_dip.y());
  root_surface()->Commit();
  connection()->ScheduleFlush();
}

void WaylandAuxiliaryWindow::CreateSubsurface() {
  auto* parent = parent_window();
  if (!parent) {
    // wl_subsurface can be used for several purposes: tooltips and drag arrow
    // windows. If we are in a drag process, use the entered window. Otherwise,
    // it must be a tooltip.
    if (connection()->IsDragInProgress()) {
      parent = connection()->data_drag_controller()->entered_window();
      set_parent_window(parent);
    } else {
      // If Aura does not not provide a reference parent window, needed by
      // Wayland, we get the current focused window to place and show the
      // tooltips.
      parent =
          connection()->wayland_window_manager()->GetCurrentFocusedWindow();
    }
  }

  // Tooltip and drag arrow creation is an async operation. By the time Aura
  // actually creates them, it is possible that the user has already moved the
  // mouse/pointer out of the window that triggered the tooltip, or user is no
  // longer in a drag/drop process. In this case, parent is nullptr.
  if (!parent)
    return;

  subsurface_ = root_surface()->CreateSubsurface(parent->root_surface());

  auto subsurface_bounds_dip =
      wl::TranslateWindowBoundsToParentDIP(this, parent);

  DCHECK(subsurface_);
  // Convert position to DIP.
  wl_subsurface_set_position(subsurface_.get(), subsurface_bounds_dip.x(),
                             subsurface_bounds_dip.y());
  wl_subsurface_set_desync(subsurface_.get());
  parent->root_surface()->Commit();
  connection()->ScheduleFlush();

  // Notify the observers the window has been configured. Please note that
  // subsurface doesn't send ack configure events. Thus, notify the observers as
  // soon as the subsurface is created.
  connection()->wayland_window_manager()->NotifyWindowConfigured(this);
}

bool WaylandAuxiliaryWindow::OnInitialize(
    PlatformWindowInitProperties properties) {
  DCHECK(!parent_window());

  // If we do not have parent window provided, we must always use a focused
  // window or a window that entered drag whenever the subsurface is created.
  if (properties.parent_widget == gfx::kNullAcceleratedWidget)
    return true;

  set_parent_window(
      connection()->wayland_window_manager()->FindParentForNewWindow(
          properties.parent_widget));
  return true;
}

}  // namespace ui
