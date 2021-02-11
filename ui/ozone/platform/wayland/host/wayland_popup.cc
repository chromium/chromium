// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_popup.h"

#include <aura-shell-client-protocol.h>

#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/shell_object_factory.h"
#include "ui/ozone/platform/wayland/host/shell_popup_wrapper.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_zaura_shell.h"

namespace ui {

WaylandPopup::WaylandPopup(PlatformWindowDelegate* delegate,
                           WaylandConnection* connection)
    : WaylandWindow(delegate, connection) {}

WaylandPopup::~WaylandPopup() = default;

bool WaylandPopup::CreateShellPopup() {
  DCHECK(parent_window() && !shell_popup_);

  auto subsurface_bounds_dip =
      wl::TranslateWindowBoundsToParentDIP(this, parent_window());

  ShellObjectFactory factory;
  shell_popup_ = factory.CreateShellPopupWrapper(connection(), this,
                                                 subsurface_bounds_dip);
  if (!shell_popup_) {
    LOG(ERROR) << "Failed to create Wayland shell popup";
    return false;
  }

  parent_window()->set_child_window(this);
  InitializeAuraShellSurface();
  return true;
}

void WaylandPopup::InitializeAuraShellSurface() {
  DCHECK(shell_popup_);
  if (!connection()->zaura_shell() || aura_surface_)
    return;
  aura_surface_.reset(zaura_shell_get_aura_surface(
      connection()->zaura_shell()->wl_object(), root_surface()->surface()));
  if (shadow_type_ == PlatformWindowShadowType::kDrop) {
    zaura_surface_set_frame(aura_surface_.get(),
                            ZAURA_SURFACE_FRAME_TYPE_SHADOW);
  }
}

void WaylandPopup::Show(bool inactive) {
  if (shell_popup_)
    return;

  if (!CreateShellPopup()) {
    Close();
    return;
  }

  UpdateBufferScale(false);
  connection()->ScheduleFlush();
  WaylandWindow::Show(inactive);
}

void WaylandPopup::Hide() {
  if (!shell_popup_)
    return;

  if (child_window())
    child_window()->Hide();

  if (shell_popup_) {
    parent_window()->set_child_window(nullptr);
    shell_popup_.reset();
  }

  // Detach buffer from surface in order to completely shutdown popups and
  // tooltips, and release resources.
  connection()->buffer_manager_host()->ResetSurfaceContents(root_surface());
}

bool WaylandPopup::IsVisible() const {
  return !!shell_popup_;
}

void WaylandPopup::HandlePopupConfigure(const gfx::Rect& bounds_dip) {
  DCHECK(shell_popup());
  DCHECK(parent_window());

  root_surface()->SetBufferScale(parent_window()->buffer_scale(), true);

  gfx::Rect new_bounds_dip = bounds_dip;

  // It's not enough to just set new bounds. If it is a menu window, whose
  // parent is a top level window a.k.a browser window, it can be flipped
  // vertically along y-axis and have negative values set. Chromium cannot
  // understand that and starts to position nested menu windows incorrectly. To
  // fix that, we have to bear in mind that Wayland compositor does not share
  // global coordinates for any surfaces, and Chromium assumes the top level
  // window is always located at 0,0 origin. What is more, child windows must
  // always be positioned relative to parent window local surface coordinates.
  // Thus, if the menu window is flipped along y-axis by Wayland and its origin
  // is above the top level parent window, the origin of the top level window
  // has to be shifted by that value on y-axis so that the origin of the menu
  // becomes x,0, and events can be handled normally.
  if (!wl::IsMenuType(parent_window()->type())) {
    gfx::Rect parent_bounds = parent_window()->GetBounds();
    // The menu window is flipped along y-axis and have x,-y origin. Shift the
    // parent top level window instead.
    if (new_bounds_dip.y() < 0) {
      // Move parent bounds along y-axis.
      parent_bounds.set_y(-(new_bounds_dip.y() * buffer_scale()));
      new_bounds_dip.set_y(0);
    } else {
      // If the menu window is located at correct origin from the browser point
      // of view, return the top level window back to 0,0.
      parent_bounds.set_y(0);
    }
    parent_window()->SetBounds(parent_bounds);
  } else {
    // The nested menu windows are located relative to the parent menu windows.
    // Thus, the location must be translated to be relative to the top level
    // window, which automatically becomes the same as relative to an origin of
    // a display.
    new_bounds_dip = gfx::ScaleToRoundedRect(
        wl::TranslateBoundsToTopLevelCoordinates(
            gfx::ScaleToRoundedRect(new_bounds_dip, buffer_scale()),
            parent_window()->GetBounds()),
        1.0 / buffer_scale());
    DCHECK(new_bounds_dip.y() >= 0);
  }

  SetBoundsDip(new_bounds_dip);
}

void WaylandPopup::HandleSurfaceConfigure(uint32_t serial) {
  shell_popup()->AckConfigure(serial);
}

void WaylandPopup::OnCloseRequest() {
  // Before calling OnCloseRequest, the |shell_popup_| must become hidden and
  // only then call OnCloseRequest().
  DCHECK(!shell_popup_);
  WaylandWindow::OnCloseRequest();
}

bool WaylandPopup::OnInitialize(PlatformWindowInitProperties properties) {
  DCHECK(wl::IsMenuType(type()));
  DCHECK(parent_window());
  root_surface()->SetBufferScale(parent_window()->buffer_scale(), false);
  set_ui_scale(parent_window()->ui_scale());
  shadow_type_ = properties.shadow_type;
  return true;
}

}  // namespace ui
