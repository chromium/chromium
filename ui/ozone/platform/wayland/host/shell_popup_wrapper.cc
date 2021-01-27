// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/shell_popup_wrapper.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"

namespace ui {

constexpr uint32_t kAnchorDefaultWidth = 1;
constexpr uint32_t kAnchorDefaultHeight = 1;
constexpr uint32_t kAnchorHeightParentMenu = 30;

gfx::Rect GetAnchorRect(MenuType menu_type,
                        const gfx::Rect& menu_bounds,
                        const gfx::Rect& parent_window_bounds) {
  gfx::Rect anchor_rect;
  switch (menu_type) {
    case MenuType::TYPE_RIGHT_CLICK:
      // Place anchor for right click menus normally.
      anchor_rect = gfx::Rect(menu_bounds.x(), menu_bounds.y(),
                              kAnchorDefaultWidth, kAnchorDefaultHeight);
      break;
    case MenuType::TYPE_3DOT_PARENT_MENU:
      // The anchor for parent menu windows is positioned slightly above the
      // specified bounds to ensure flipped window along y-axis won't hide 3-dot
      // menu button.
      anchor_rect = gfx::Rect(menu_bounds.x() - kAnchorDefaultWidth,
                              menu_bounds.y() - kAnchorHeightParentMenu,
                              kAnchorDefaultWidth, kAnchorHeightParentMenu);
      break;
    case MenuType::TYPE_3DOT_CHILD_MENU:
      // The child menu's anchor must meet the following requirements: at some
      // point, the Wayland compositor can flip it along x-axis. To make sure
      // it's positioned correctly, place it closer to the beginning of the
      // parent menu shifted by the same value along x-axis. The width of anchor
      // must correspond the width between two points - specified origin by the
      // Chromium and calculated point shifted by the same value along x-axis
      // from the beginning of the parent menu width.
      //
      // We also have to bear in mind that Chromium may decide to flip the
      // position of the menu window along the x-axis and show it on the other
      // side of the parent menu window (normally, the Wayland compositor does
      // it). Thus, check which side the child menu window is going to be
      // presented on and create right anchor.
      if (menu_bounds.x() >= 0) {
        auto anchor_width =
            parent_window_bounds.width() -
            (parent_window_bounds.width() - menu_bounds.x()) * 2;
        if (anchor_width <= 0) {
          anchor_rect = gfx::Rect(menu_bounds.x(), menu_bounds.y(),
                                  kAnchorDefaultWidth, kAnchorDefaultHeight);
        } else {
          anchor_rect =
              gfx::Rect(parent_window_bounds.width() - menu_bounds.x(),
                        menu_bounds.y(), anchor_width, kAnchorDefaultHeight);
        }
      } else {
        DCHECK_LE(menu_bounds.x(), 0);
        auto position = menu_bounds.width() + menu_bounds.x();
        DCHECK(position > 0 && position < parent_window_bounds.width());
        auto anchor_width = parent_window_bounds.width() - position * 2;
        if (anchor_width <= 0) {
          anchor_rect = gfx::Rect(position, menu_bounds.y(),
                                  kAnchorDefaultWidth, kAnchorDefaultHeight);
        } else {
          anchor_rect = gfx::Rect(position, menu_bounds.y(), anchor_width,
                                  kAnchorDefaultHeight);
        }
      }
      break;
    case MenuType::TYPE_UNKNOWN:
      NOTREACHED() << "Unsupported menu type";
      break;
  }

  return anchor_rect;
}

WlAnchor GetAnchor(MenuType menu_type, const gfx::Rect& bounds) {
  WlAnchor anchor = WlAnchor::None;
  switch (menu_type) {
    case MenuType::TYPE_RIGHT_CLICK:
      anchor = WlAnchor::TopLeft;
      break;
    case MenuType::TYPE_3DOT_PARENT_MENU:
      anchor = WlAnchor::BottomRight;
      break;
    case MenuType::TYPE_3DOT_CHILD_MENU:
      // Chromium may want to manually position a child menu on the left side of
      // its parent menu. Thus, react accordingly. Positive x means the child is
      // located on the right side of the parent and negative - on the left
      // side.
      if (bounds.x() >= 0)
        anchor = WlAnchor::TopRight;
      else
        anchor = WlAnchor::TopLeft;
      break;
    case MenuType::TYPE_UNKNOWN:
      NOTREACHED() << "Unsupported menu type";
      break;
  }

  return anchor;
}

WlGravity GetGravity(MenuType menu_type, const gfx::Rect& bounds) {
  WlGravity gravity = WlGravity::None;
  switch (menu_type) {
    case MenuType::TYPE_RIGHT_CLICK:
      gravity = WlGravity::BottomRight;
      break;
    case MenuType::TYPE_3DOT_PARENT_MENU:
      gravity = WlGravity::BottomRight;
      break;
    case MenuType::TYPE_3DOT_CHILD_MENU:
      // Chromium may want to manually position a child menu on the left side of
      // its parent menu. Thus, react accordingly. Positive x means the child is
      // located on the right side of the parent and negative - on the left
      // side.
      if (bounds.x() >= 0)
        gravity = WlGravity::BottomRight;
      else
        gravity = WlGravity::BottomLeft;
      break;
    case MenuType::TYPE_UNKNOWN:
      NOTREACHED() << "Unsupported menu type";
      break;
  }

  return gravity;
}

WlConstraintAdjustment GetConstraintAdjustment(MenuType menu_type) {
  WlConstraintAdjustment constraint = WlConstraintAdjustment::None;

  switch (menu_type) {
    case MenuType::TYPE_RIGHT_CLICK:
      constraint =
          WlConstraintAdjustment::SlideX | WlConstraintAdjustment::SlideY |
          WlConstraintAdjustment::FlipY | WlConstraintAdjustment::ResizeY;
      break;
    case MenuType::TYPE_3DOT_PARENT_MENU:
      constraint = WlConstraintAdjustment::SlideX |
                   WlConstraintAdjustment::FlipY |
                   WlConstraintAdjustment::ResizeY;
      break;
    case MenuType::TYPE_3DOT_CHILD_MENU:
      constraint = WlConstraintAdjustment::SlideY |
                   WlConstraintAdjustment::FlipX |
                   WlConstraintAdjustment::ResizeY;
      break;
    case MenuType::TYPE_UNKNOWN:
      NOTREACHED() << "Unsupported menu type";
      break;
  }

  return constraint;
}

MenuType ShellPopupWrapper::GetMenuTypeForPositioner(
    WaylandConnection* connection,
    WaylandWindow* parent_window) const {
  bool is_right_click_menu =
      connection->event_source()->last_pointer_button_pressed() &
      EF_RIGHT_MOUSE_BUTTON;

  // Different types of menu require different anchors, constraint adjustments,
  // gravity and etc.
  if (is_right_click_menu)
    return MenuType::TYPE_RIGHT_CLICK;
  else if (!wl::IsMenuType(parent_window->type()))
    return MenuType::TYPE_3DOT_PARENT_MENU;
  else
    return MenuType::TYPE_3DOT_CHILD_MENU;
}

bool ShellPopupWrapper::CanGrabPopup(WaylandConnection* connection) const {
  // When drag process starts, as described the protocol -
  // https://goo.gl/1Mskq3, the client must have an active implicit grab. If
  // we try to create a popup and grab it, it will be immediately dismissed.
  // Thus, do not take explicit grab during drag process.
  if (connection->IsDragInProgress() || !connection->seat())
    return false;

  // According to the definition of the xdg protocol, the grab request must be
  // used in response to some sort of user action like a button press, key
  // press, or touch down event.
  EventType last_event_type = connection->event_serial().event_type;
  return last_event_type == ET_TOUCH_PRESSED ||
         last_event_type == ET_KEY_PRESSED ||
         last_event_type == ET_MOUSE_PRESSED;
}

}  // namespace ui
