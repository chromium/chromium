// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_CONTAINER_FLOATING_BEHAVIOR_H_
#define UI_KEYBOARD_CONTAINER_FLOATING_BEHAVIOR_H_

#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/keyboard/container_behavior.h"
#include "ui/keyboard/drag_descriptor.h"
#include "ui/keyboard/keyboard_export.h"

namespace keyboard {

// Margins from the bottom right corner of the screen for the default location
// of the keyboard.
constexpr int kDefaultDistanceFromScreenBottom = 20;
constexpr int kDefaultDistanceFromScreenRight = 20;

class KEYBOARD_EXPORT ContainerFloatingBehavior : public ContainerBehavior {
 public:
  explicit ContainerFloatingBehavior(Delegate* delegate);
  ~ContainerFloatingBehavior() override;

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
  void SavePosition(const gfx::Rect& keyboard_bounds_in_screen,
                    const gfx::Size& screen_size) override;
  bool HandlePointerEvent(const ui::LocatedEvent& event,
                          const display::Display& current_display) override;
  void SetCanonicalBounds(aura::Window* container,
                          const gfx::Rect& display_bounds) override;
  ContainerType GetType() const override;
  bool TextBlurHidesKeyboard() const override;
  gfx::Rect GetOccludedBounds(
      const gfx::Rect& visual_bounds_in_screen) const override;
  bool OccludedBoundsAffectWorkspaceLayout() const override;
  bool SetDraggableArea(const gfx::Rect& rect) override;

  // Calculate the position of the keyboard for when it is being shown.
  gfx::Point GetPositionForShowingKeyboard(
      const gfx::Size& keyboard_size,
      const gfx::Rect& display_bounds) const;

 private:
  struct KeyboardPosition {
    double left_padding_allotment_ratio;
    double top_padding_allotment_ratio;
  };

  // Ensures that the keyboard is neither off the screen nor overlapping an
  // edge.
  gfx::Rect ContainKeyboardToScreenBounds(
      const gfx::Rect& keyboard_bounds_in_screen,
      const gfx::Rect& display_bounds) const;

  // Saves the current keyboard location for use the next time it is displayed.
  void UpdateLastPoint(const gfx::Point& position);

  // TODO(blakeo): cache the default_position_ on a per-display basis.
  std::unique_ptr<KeyboardPosition> default_position_in_screen_;

  // Current state of a cursor drag to move the keyboard, if one exists.
  // Otherwise nullptr.
  std::unique_ptr<const DragDescriptor> drag_descriptor_;

  gfx::Rect draggable_area_ = gfx::Rect();
};

}  // namespace keyboard

#endif  // UI_KEYBOARD_CONTAINER_FLOATING_BEHAVIOR_H_
