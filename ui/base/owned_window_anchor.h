// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_OWNED_WINDOW_ANCHOR_H_
#define UI_BASE_OWNED_WINDOW_ANCHOR_H_

#include <cstdint>
#include <type_traits>

#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

// Structure that describes anchor for an owned window. Owned windows are the
// windows that must be owned by their context (for example, menus, tooltips),
// and they must be positioned in such a way that a system compositor can
// reposition them using provided anchor. Ozone/Wayland is an example user of
// this.
struct OwnedWindowAnchor {
  gfx::Rect anchor_rect;
  OwnedWindowAnchorPosition anchor_position = OwnedWindowAnchorPosition::kNone;
  OwnedWindowAnchorGravity anchor_gravity = OwnedWindowAnchorGravity::kNone;
  OwnedWindowConstraintAdjustment constraint_adjustment =
      OwnedWindowConstraintAdjustment::kAdjustmentNone;
};

}  // namespace ui

inline constexpr ui::OwnedWindowConstraintAdjustment operator|(
    ui::OwnedWindowConstraintAdjustment l,
    ui::OwnedWindowConstraintAdjustment r) {
  using T = std::underlying_type_t<ui::OwnedWindowConstraintAdjustment>;
  return static_cast<ui::OwnedWindowConstraintAdjustment>(static_cast<T>(l) |
                                                          static_cast<T>(r));
}

inline constexpr ui::OwnedWindowConstraintAdjustment operator&(
    ui::OwnedWindowConstraintAdjustment l,
    ui::OwnedWindowConstraintAdjustment r) {
  using T = std::underlying_type_t<ui::OwnedWindowConstraintAdjustment>;
  return static_cast<ui::OwnedWindowConstraintAdjustment>(static_cast<T>(l) &
                                                          static_cast<T>(r));
}

#endif  // UI_BASE_OWNED_WINDOW_ANCHOR_H_
