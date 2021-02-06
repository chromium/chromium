// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_SHELL_POPUP_WRAPPER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_SHELL_POPUP_WRAPPER_H_

#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class WaylandConnection;
class WaylandWindow;

enum class MenuType {
  TYPE_RIGHT_CLICK,
  TYPE_3DOT_PARENT_MENU,
  TYPE_3DOT_CHILD_MENU,
  TYPE_UNKNOWN,
};

enum class WlAnchor {
  None,
  Top,
  Bottom,
  Left,
  Right,
  TopLeft,
  BottomLeft,
  TopRight,
  BottomRight,
};

enum class WlGravity {
  None,
  Top,
  Bottom,
  Left,
  Right,
  TopLeft,
  BottomLeft,
  TopRight,
  BottomRight,
};

enum class WlConstraintAdjustment : uint32_t {
  None = 0,
  SlideX = 1,
  SlideY = 2,
  FlipX = 4,
  FlipY = 8,
  ResizeX = 16,
  ResizeY = 32,
};

inline WlConstraintAdjustment operator|(WlConstraintAdjustment a,
                                        WlConstraintAdjustment b) {
  return static_cast<WlConstraintAdjustment>(static_cast<uint32_t>(a) |
                                             static_cast<uint32_t>(b));
}

inline WlConstraintAdjustment operator&(WlConstraintAdjustment a,
                                        WlConstraintAdjustment b) {
  return static_cast<WlConstraintAdjustment>(static_cast<uint32_t>(a) &
                                             static_cast<uint32_t>(b));
}

// A wrapper around different versions of xdg popups.
class ShellPopupWrapper {
 public:
  virtual ~ShellPopupWrapper() {}

  // Initializes the popup surface.
  virtual bool Initialize(WaylandConnection* connection,
                          const gfx::Rect& bounds) = 0;

  // Sends acknowledge configure event back to wayland.
  virtual void AckConfigure(uint32_t serial) = 0;

  MenuType GetMenuTypeForPositioner(WaylandConnection* connection,
                                    WaylandWindow* parent_window) const;
  bool CanGrabPopup(WaylandConnection* connection) const;
};

gfx::Rect GetAnchorRect(MenuType menu_type,
                        const gfx::Rect& menu_bounds,
                        const gfx::Rect& parent_window_bounds);
WlAnchor GetAnchor(MenuType menu_type, const gfx::Rect& bounds);
WlGravity GetGravity(MenuType menu_type, const gfx::Rect& bounds);
WlConstraintAdjustment GetConstraintAdjustment(MenuType menu_type);

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_SHELL_POPUP_WRAPPER_H_
