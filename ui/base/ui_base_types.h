// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_UI_BASE_TYPES_H_
#define UI_BASE_UI_BASE_TYPES_H_

#include <cstdint>

namespace ui {

// Specifies which edges of the window are tiled.
//
// Wayland can notify the application if certain edge of the window is
// "tiled": https://wayland.app/protocols/xdg-shell#xdg_toplevel:enum:state.
// Chromium should not draw frame decorations for the tiled edges.
struct WindowTiledEdges {
  bool left{false};
  bool right{false};
  bool top{false};
  bool bottom{false};

  bool operator==(const WindowTiledEdges& other) const {
    return left == other.left && right == other.right && top == other.top &&
           bottom == other.bottom;
  }

  bool operator!=(const WindowTiledEdges& other) const {
    return left != other.left || right != other.right || top != other.top ||
           bottom != other.bottom;
  }
};

// MdTextButtons have various button styles that can change the button's
// relative prominence/priority. The relative priority (least to greatest) is
// as follows:
// kText -> kDefault -> kTonal -> kProminent
// The default styles are described as below.
// kDefault: White background with blue text and a solid outline.
// kProminent: Blue background with white text.
// kTonal: Cyan background with black text.
// kText: White background with blue text but no outline.
enum class ButtonStyle {
  kText,
  kDefault,
  kTonal,
  kProminent,
};

// The class of window and its overall z-order. Only the Mac provides this
// level of z-order granularity. For other platforms, which only provide a
// distinction between "normal" and "always on top" windows, any of the values
// here that aren't `kNormal` are treated equally as "always on top".
// TODO(crbug.com/40237029): For non-desktop widgets on Linux and Windows,
// this z-order currently does not have any effect.
enum class ZOrderLevel {
  // The default level for windows.
  kNormal = 0,

  // A "floating" window z-ordered above other normal windows.
  //
  // Note this is the traditional _desktop_ concept of a "floating window".
  // Android has a concept of "freeform window mode" in which apps are presented
  // in separate "floating" windows that can be moved and resized by the user.
  // That's not what this is.
  kFloatingWindow,

  // UI elements are used to annotate positions on the screen, and thus must
  // appear above floating windows.
  kFloatingUIElement,

  // There have been horrific security decisions that have been made on the web
  // platform that are now expected behavior and cannot easily be changed. The
  // only way to mitigate problems with these decisions is to inform the user by
  // presenting them with a message that they are in a state that they might not
  // expect, and this message must be presented in a UI that cannot be
  // interfered with or covered up. Thus this level for Security UI that must be
  // Z-ordered in front of everything else. Note that this is useful in
  // situations where window modality (as in ModalType) cannot or should not be
  // used.
  kSecuritySurface,
};

// TODO(varunjain): Remove MENU_SOURCE_NONE (crbug.com/250964)
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.ui.base
// These are used in histograms, do not remove/renumber entries. Only add at the
// end just before MENU_SOURCE_TYPE_LAST. Also remember to update the
// MenuSourceType enum listing in tools/metrics/histograms/enums.xml.
// Lastly, any new type here needs to be synced with ui_base_types.mojom.
enum MenuSourceType {
  MENU_SOURCE_NONE = 0,
  MENU_SOURCE_MOUSE = 1,
  MENU_SOURCE_KEYBOARD = 2,
  MENU_SOURCE_TOUCH = 3,
  MENU_SOURCE_TOUCH_EDIT_MENU = 4,
  MENU_SOURCE_LONG_PRESS = 5,
  MENU_SOURCE_LONG_TAP = 6,
  MENU_SOURCE_TOUCH_HANDLE = 7,
  MENU_SOURCE_STYLUS = 8,
  MENU_SOURCE_ADJUST_SELECTION = 9,
  MENU_SOURCE_ADJUST_SELECTION_RESET = 10,
  MENU_SOURCE_TYPE_LAST = MENU_SOURCE_ADJUST_SELECTION_RESET
};

// Where an owned anchored window should be anchored to. Used by such backends
// as Wayland, which doesn't provide clients with on screen coordinates, but
// rather forces them to position children windows relative to toplevel windows.
// They use anchor bounds, anchor position, gravity and constraints to
// reposition such windows if the originally intended position caused the
// surface to be constrained.
enum class OwnedWindowAnchorPosition {
  kNone,
  kTop,
  kBottom,
  kLeft,
  kRight,
  kTopLeft,
  kBottomLeft,
  kTopRight,
  kBottomRight,
};

// What direction an owned window should be positioned relatively to its anchor.
enum class OwnedWindowAnchorGravity {
  kNone,
  kTop,
  kBottom,
  kLeft,
  kRight,
  kTopLeft,
  kBottomLeft,
  kTopRight,
  kBottomRight,
};

// How an owned window can be resized/repositioned by a system compositor.
enum class OwnedWindowConstraintAdjustment : uint32_t {
  kAdjustmentNone = 0,
  kAdjustmentSlideX = 1 << 0,
  kAdjustmentSlideY = 1 << 1,
  kAdjustmentFlipX = 1 << 2,
  kAdjustmentFlipY = 1 << 3,
  kAdjustmentResizeX = 1 << 4,
  kAdjustmentRezizeY = 1 << 5,
};

}  // namespace ui

#endif  // UI_BASE_UI_BASE_TYPES_H_
