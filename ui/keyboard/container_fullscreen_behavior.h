// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_CONTAINER_FULLSCREEN_BEHAVIOR_H_
#define UI_KEYBOARD_CONTAINER_FULLSCREEN_BEHAVIOR_H_

#include "ui/keyboard/container_full_width_behavior.h"
#include "ui/keyboard/keyboard_export.h"

namespace keyboard {

class KEYBOARD_EXPORT ContainerFullscreenBehavior
    : public ContainerFullWidthBehavior {
 public:
  explicit ContainerFullscreenBehavior(Delegate* controller);
  ~ContainerFullscreenBehavior() override;

  // ContainerFullWidthBehavior overrides
  gfx::Rect AdjustSetBoundsRequest(
      const gfx::Rect& display_bounds,
      const gfx::Rect& requested_bounds_in_screen_coords) override;
  void SetCanonicalBounds(aura::Window* container,
                          const gfx::Rect& display_bounds) override;
  ContainerType GetType() const override;
  void SetOccludedBounds(const gfx::Rect& occluded_bounds_in_window) override;
  gfx::Rect GetOccludedBounds(
      const gfx::Rect& visual_bounds_in_screen) const override;

 private:
  // The occluded bounds for fullscreen behavior is determined on the IME
  // extension side, so it has to be passed here via the extension API.
  gfx::Rect occluded_bounds_;
};

}  // namespace keyboard

#endif  // UI_KEYBOARD_CONTAINER_FULLSCREEN_BEHAVIOR_H_
