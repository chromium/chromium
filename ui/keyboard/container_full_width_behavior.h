// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_CONTAINER_FULL_WIDTH_BEHAVIOR_H_
#define UI_KEYBOARD_CONTAINER_FULL_WIDTH_BEHAVIOR_H_

#include "ui/gfx/geometry/rect.h"
#include "ui/keyboard/container_behavior.h"

namespace keyboard {

// Relative distance from the parent window, from which show animation starts
// or hide animation finishes.
constexpr int kFullWidthKeyboardAnimationDistance = 30;

class KEYBOARD_EXPORT ContainerFullWidthBehavior : public ContainerBehavior {
 public:
  explicit ContainerFullWidthBehavior(Delegate* delegate);
  ~ContainerFullWidthBehavior() override;

  // ContainerBehavior overrides
  void DoHidingAnimation(
      aura::Window* window,
      ::wm::ScopedHidingAnimationSettings* animation_settings) override;
  void DoShowingAnimation(
      aura::Window* window,
      ui::ScopedLayerAnimationSettings* animation_settings) override;
  void InitializeShowAnimationStartingState(aura::Window* container) override;
  gfx::Rect AdjustSetBoundsRequest(
      const gfx::Rect& display_bounds,
      const gfx::Rect& requested_bounds_in_screen_coords) override;
  bool IsOverscrollAllowed() const override;
  bool IsDragHandle(const gfx::Vector2d& offset,
                    const gfx::Size& keyboard_size) const override;
  void SavePosition(const gfx::Rect& keyboard_bounds,
                    const gfx::Size& screen_size) override;
  bool HandlePointerEvent(const ui::LocatedEvent& event,
                          const display::Display& current_display) override;
  void SetCanonicalBounds(aura::Window* container,
                          const gfx::Rect& display_bounds) override;
  ContainerType GetType() const override;
  bool TextBlurHidesKeyboard() const override;
  void SetOccludedBounds(const gfx::Rect& occluded_bounds_in_window) override;
  gfx::Rect GetOccludedBounds(
      const gfx::Rect& visual_bounds_in_screen) const override;
  bool OccludedBoundsAffectWorkspaceLayout() const override;
  bool SetDraggableArea(const gfx::Rect& rect) override;

 private:
  gfx::Rect occluded_bounds_in_window_;
};

}  // namespace keyboard

#endif  // UI_KEYBOARD_CONTAINER_FULL_WIDTH_BEHAVIOR_H_
