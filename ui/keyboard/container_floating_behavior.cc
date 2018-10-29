// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/container_floating_behavior.h"

#include "ui/aura/window.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/keyboard/display_util.h"
#include "ui/keyboard/drag_descriptor.h"
#include "ui/wm/core/window_animations.h"

namespace keyboard {

// Length of the animation to show and hide the keyboard.
constexpr int kAnimationDurationMs = 200;

// Distance the keyboard moves during the animation
constexpr int kAnimationDistance = 30;

ContainerFloatingBehavior::ContainerFloatingBehavior(Delegate* delegate)
    : ContainerBehavior(delegate) {}

ContainerFloatingBehavior::~ContainerFloatingBehavior() = default;

ContainerType ContainerFloatingBehavior::GetType() const {
  return ContainerType::FLOATING;
}

void ContainerFloatingBehavior::DoHidingAnimation(
    aura::Window* container,
    ::wm::ScopedHidingAnimationSettings* animation_settings) {
  animation_settings->layer_animation_settings()->SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kAnimationDurationMs));
  gfx::Transform transform;
  transform.Translate(0, kAnimationDistance);
  container->SetTransform(transform);
  container->layer()->SetOpacity(0.f);
}

void ContainerFloatingBehavior::DoShowingAnimation(
    aura::Window* container,
    ui::ScopedLayerAnimationSettings* animation_settings) {
  animation_settings->SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);
  animation_settings->SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kAnimationDurationMs));

  container->SetTransform(gfx::Transform());
  container->layer()->SetOpacity(1.0);
}

void ContainerFloatingBehavior::InitializeShowAnimationStartingState(
    aura::Window* container) {
  aura::Window* root_window = container->GetRootWindow();

  SetCanonicalBounds(container, root_window->bounds());

  gfx::Transform transform;
  transform.Translate(0, kAnimationDistance);
  container->SetTransform(transform);
  container->layer()->SetOpacity(kAnimationStartOrAfterHideOpacity);
}

gfx::Rect ContainerFloatingBehavior::AdjustSetBoundsRequest(
    const gfx::Rect& display_bounds,
    const gfx::Rect& requested_bounds_in_screen) {
  gfx::Rect keyboard_bounds_in_screen = requested_bounds_in_screen;

  if (!default_position_in_screen_) {
    // If the keyboard hasn't been shown yet, ignore the request and use
    // default.
    gfx::Point default_location = GetPositionForShowingKeyboard(
        keyboard_bounds_in_screen.size(), display_bounds);
    keyboard_bounds_in_screen =
        gfx::Rect(default_location, keyboard_bounds_in_screen.size());
  } else {
    // Otherwise, simply make sure that the new bounds are not off the edge of
    // the screen.
    keyboard_bounds_in_screen = ContainKeyboardToScreenBounds(
        keyboard_bounds_in_screen, display_bounds);
    SavePosition(keyboard_bounds_in_screen, display_bounds.size());
  }

  return keyboard_bounds_in_screen;
}

void ContainerFloatingBehavior::SavePosition(
    const gfx::Rect& keyboard_bounds_in_screen,
    const gfx::Size& screen_size) {
  int left_distance = keyboard_bounds_in_screen.x();
  int right_distance = screen_size.width() - keyboard_bounds_in_screen.right();
  int top_distance = keyboard_bounds_in_screen.y();
  int bottom_distance =
      screen_size.height() - keyboard_bounds_in_screen.bottom();

  double available_width = left_distance + right_distance;
  double available_height = top_distance + bottom_distance;

  if (!default_position_in_screen_) {
    default_position_in_screen_ = std::make_unique<KeyboardPosition>();
  }

  default_position_in_screen_->left_padding_allotment_ratio =
      left_distance / available_width;
  default_position_in_screen_->top_padding_allotment_ratio =
      top_distance / available_height;
}

gfx::Rect ContainerFloatingBehavior::ContainKeyboardToScreenBounds(
    const gfx::Rect& keyboard_bounds_in_screen,
    const gfx::Rect& display_bounds) const {
  int left = keyboard_bounds_in_screen.x();
  int top = keyboard_bounds_in_screen.y();
  int right = keyboard_bounds_in_screen.right();
  int bottom = keyboard_bounds_in_screen.bottom();

  // Prevent keyboard from appearing off screen or overlapping with the edge.
  if (left < display_bounds.x()) {
    left = display_bounds.x();
    right = left + keyboard_bounds_in_screen.width();
  }
  if (right >= display_bounds.right()) {
    right = display_bounds.right();
    left = right - keyboard_bounds_in_screen.width();
  }
  if (top < display_bounds.y()) {
    top = display_bounds.y();
    bottom = top + keyboard_bounds_in_screen.height();
  }
  if (bottom >= display_bounds.bottom()) {
    bottom = display_bounds.bottom();
    top = bottom - keyboard_bounds_in_screen.height();
  }

  return gfx::Rect(left, top, right - left, bottom - top);
}

bool ContainerFloatingBehavior::IsOverscrollAllowed() const {
  return false;
}

gfx::Point ContainerFloatingBehavior::GetPositionForShowingKeyboard(
    const gfx::Size& keyboard_size,
    const gfx::Rect& display_bounds) const {
  // Start with the last saved position
  gfx::Point top_left_offset;
  KeyboardPosition* position = default_position_in_screen_.get();
  if (position == nullptr) {
    // If there is none, center the keyboard along the bottom of the screen.
    top_left_offset.set_x(display_bounds.width() - keyboard_size.width() -
                          kDefaultDistanceFromScreenRight);
    top_left_offset.set_y(display_bounds.height() - keyboard_size.height() -
                          kDefaultDistanceFromScreenBottom);
  } else {
    double left = (display_bounds.width() - keyboard_size.width()) *
                  position->left_padding_allotment_ratio;
    double top = (display_bounds.height() - keyboard_size.height()) *
                 position->top_padding_allotment_ratio;
    top_left_offset.set_x((int)left);
    top_left_offset.set_y((int)top);
  }

  // Make sure that this location is valid according to the current size of the
  // screen.
  gfx::Rect keyboard_bounds =
      gfx::Rect(top_left_offset.x() + display_bounds.x(),
                top_left_offset.y() + display_bounds.y(), keyboard_size.width(),
                keyboard_size.height());

  gfx::Rect valid_keyboard_bounds =
      ContainKeyboardToScreenBounds(keyboard_bounds, display_bounds);

  return valid_keyboard_bounds.origin();
}

bool ContainerFloatingBehavior::IsDragHandle(
    const gfx::Vector2d& offset,
    const gfx::Size& keyboard_size) const {
  return draggable_area_.Contains(offset.x(), offset.y());
}

bool ContainerFloatingBehavior::HandlePointerEvent(
    const ui::LocatedEvent& event,
    const display::Display& current_display) {
  auto kb_offset = gfx::Vector2d(event.x(), event.y());

  const gfx::Rect& keyboard_bounds_in_screen = delegate_->GetBoundsInScreen();

  // Don't handle events if this runs in a partially initialized state.
  if (keyboard_bounds_in_screen.height() <= 0)
    return false;

  ui::PointerId pointer_id = -1;
  if (event.IsTouchEvent()) {
    const ui::TouchEvent* te = event.AsTouchEvent();
    pointer_id = te->pointer_details().id;
  }

  const ui::EventType type = event.type();
  switch (type) {
    case ui::ET_TOUCH_PRESSED:
    case ui::ET_MOUSE_PRESSED:
      if (!IsDragHandle(kb_offset, keyboard_bounds_in_screen.size())) {
        drag_descriptor_ = nullptr;
      } else if (type == ui::ET_MOUSE_PRESSED &&
                 !((const ui::MouseEvent*)&event)->IsOnlyLeftMouseButton()) {
        // Mouse events are limited to just the left mouse button.
        drag_descriptor_ = nullptr;
      } else if (!drag_descriptor_) {
        // If there is no active drag descriptor, start a new one.
        bool drag_started_by_touch = (type == ui::ET_TOUCH_PRESSED);
        drag_descriptor_.reset(
            new DragDescriptor{keyboard_bounds_in_screen.origin(), kb_offset,
                               drag_started_by_touch, pointer_id});
      }
      break;

    case ui::ET_MOUSE_DRAGGED:
    case ui::ET_TOUCH_MOVED:
      if (!drag_descriptor_) {
        // do nothing
      } else if (drag_descriptor_->is_touch_drag !=
                 (type == ui::ET_TOUCH_MOVED)) {
        // If the event isn't of the same type that started the drag, end the
        // drag to prevent confusion.
        drag_descriptor_ = nullptr;
      } else if (drag_descriptor_->pointer_id != pointer_id) {
        // do nothing.
      } else {
        // Drag continues.
        // If there is an active drag, use it to determine the new location
        // of the keyboard.
        const gfx::Point original_click_location =
            drag_descriptor_->original_keyboard_location +
            drag_descriptor_->original_click_offset;
        const gfx::Point current_drag_location =
            keyboard_bounds_in_screen.origin() + kb_offset;
        const gfx::Vector2d cumulative_drag_offset =
            current_drag_location - original_click_location;
        const gfx::Point new_keyboard_location =
            drag_descriptor_->original_keyboard_location +
            cumulative_drag_offset;
        gfx::Rect new_bounds_in_local =
            gfx::Rect(new_keyboard_location, keyboard_bounds_in_screen.size());

        DisplayUtil display_util;
        const display::Display& new_display =
            display_util.FindAdjacentDisplayIfPointIsNearMargin(
                current_display, current_drag_location);

        if (current_display.id() == new_display.id()) {
          delegate_->MoveKeyboardWindow(new_bounds_in_local);
        } else {
          // Since the keyboard has jumped across screens, cancel the current
          // drag descriptor as though the user has lifted their finger.
          drag_descriptor_ = nullptr;

          gfx::Rect new_bounds_in_screen =
              new_bounds_in_local +
              current_display.bounds().origin().OffsetFromOrigin();
          gfx::Rect contained_new_bounds_in_screen =
              ContainKeyboardToScreenBounds(new_bounds_in_screen,
                                            new_display.bounds());

          // Enqueue a transition to the adjacent display.
          new_bounds_in_local =
              contained_new_bounds_in_screen -
              new_display.bounds().origin().OffsetFromOrigin();
          delegate_->MoveKeyboardWindowToDisplay(new_display,
                                                 new_bounds_in_local);
        }
        SavePosition(delegate_->GetBoundsInScreen(), new_display.size());
        return true;
      }
      break;

    default:
      drag_descriptor_ = nullptr;
      break;
  }
  return false;
}

void ContainerFloatingBehavior::SetCanonicalBounds(
    aura::Window* container,
    const gfx::Rect& display_bounds) {
  gfx::Point keyboard_location =
      GetPositionForShowingKeyboard(container->bounds().size(), display_bounds);
  gfx::Rect keyboard_bounds_in_screen =
      gfx::Rect(keyboard_location, container->bounds().size());
  SavePosition(keyboard_bounds_in_screen, display_bounds.size());
  container->SetBounds(keyboard_bounds_in_screen);
}

bool ContainerFloatingBehavior::TextBlurHidesKeyboard() const {
  return true;
}

gfx::Rect ContainerFloatingBehavior::GetOccludedBounds(
    const gfx::Rect& visual_bounds_in_screen) const {
  return {};
}

bool ContainerFloatingBehavior::OccludedBoundsAffectWorkspaceLayout() const {
  return false;
}

bool ContainerFloatingBehavior::SetDraggableArea(const gfx::Rect& rect) {
  draggable_area_ = rect;
  return true;
}

}  //  namespace keyboard
