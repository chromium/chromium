// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/touch_selection/longpress_drag_selector.h"

#include "base/auto_reset.h"
#include "ui/events/velocity_tracker/motion_event.h"

namespace ui {
namespace {

gfx::Vector2dF SafeNormalize(const gfx::Vector2dF& v) {
  return v.IsZero() ? v : ScaleVector2d(v, 1.f / v.Length());
}

}  // namespace

LongPressDragSelector::LongPressDragSelector(
    LongPressDragSelectorClient* client)
    : client_(client),
      state_(INACTIVE),
      has_longpress_drag_start_anchor_(false) {
}

LongPressDragSelector::~LongPressDragSelector() {
}

bool LongPressDragSelector::WillHandleTouchEvent(const MotionEvent& event) {
  switch (event.GetAction()) {
    case MotionEvent::Action::DOWN:
      touch_down_position_.SetPoint(event.GetX(), event.GetY());
      touch_down_time_ = event.GetEventTime();
      has_longpress_drag_start_anchor_ = false;
      SetState(INITIATING_GESTURE_PENDING);
      return false;

    case MotionEvent::Action::UP:
    case MotionEvent::Action::CANCEL:
      SetState(INACTIVE);
      return false;

    case MotionEvent::Action::MOVE:
      break;

    default:
      return false;
  }

  if (state_ != DRAG_PENDING && state_ != DRAGGING)
    return false;

  gfx::PointF position(event.GetX(), event.GetY());
  if (state_ == DRAGGING) {
    gfx::PointF drag_position = position + longpress_drag_selection_offset_;
    client_->OnDragUpdate(*this, drag_position);
    return true;
  }

  // We can't use |touch_down_position_| as the offset anchor, as
  // showing the selection UI may have shifted the motion coordinates.
  if (!has_longpress_drag_start_anchor_) {
    has_longpress_drag_start_anchor_ = true;
    longpress_drag_start_anchor_ = position;
    return true;
  }

  // Allow an additional slop affordance after the longpress occurs.
  gfx::Vector2dF delta = position - longpress_drag_start_anchor_;
  if (client_->IsWithinTapSlop(delta))
    return true;

  gfx::PointF selection_start = client_->GetSelectionStart();
  gfx::PointF selection_end = client_->GetSelectionEnd();
  bool extend_selection_start = false;
  if (std::abs(delta.y()) > std::abs(delta.x())) {
    // If initial motion is up/down, extend the start/end selection bound.
    extend_selection_start = delta.y() < 0;
  } else {
    // Otherwise extend the selection bound toward which we're moving, or
    // the closest bound if motion is already away from both bounds.
    // Note that, for mixed RTL text, or for multiline selections triggered
    // by longpress, this may not pick the most suitable drag target
    gfx::Vector2dF start_delta = selection_start - longpress_drag_start_anchor_;
    gfx::Vector2dF end_delta = selection_end - longpress_drag_start_anchor_;

    // The vectors must be normalized to make dot product comparison meaningful.
    gfx::Vector2dF normalized_start_delta = SafeNormalize(start_delta);
    gfx::Vector2dF normalized_end_delta = SafeNormalize(end_delta);
    double start_dot_product = gfx::DotProduct(normalized_start_delta, delta);
    double end_dot_product = gfx::DotProduct(normalized_end_delta, delta);

    if (start_dot_product >= 0 || end_dot_product >= 0) {
      // The greater the dot product the more similar the direction.
      extend_selection_start = start_dot_product > end_dot_product;
    } else {
      // If we're already moving away from both endpoints, pick the closest.
      extend_selection_start =
          start_delta.LengthSquared() < end_delta.LengthSquared();
    }
  }

  gfx::PointF extent = extend_selection_start ? selection_start : selection_end;
  longpress_drag_selection_offset_ = extent - position;
  client_->OnDragBegin(*this, extent);
  SetState(DRAGGING);
  return true;
}

bool LongPressDragSelector::IsActive() const {
  return state_ == DRAG_PENDING || state_ == DRAGGING;
}

void LongPressDragSelector::OnLongPressEvent(base::TimeTicks event_time,
                                             const gfx::PointF& position) {
  // We have no guarantees that the current gesture stream is aligned with the
  // observed touch stream. We only know that the gesture sequence is downstream
  // from the touch sequence. Using a time/distance heuristic helps ensure that
  // the observed longpress corresponds to the active touch sequence.
  if (state_ == INITIATING_GESTURE_PENDING &&
      // Ensure the down event occurs *before* the longpress event. Use a
      // small time epsilon to account for floating point time conversion.
      (touch_down_time_ < event_time + base::Microseconds(10)) &&
      client_->IsWithinTapSlop(touch_down_position_ - position)) {
    SetState(SELECTION_PENDING);
  }
}

void LongPressDragSelector::OnDoublePressEvent(base::TimeTicks event_time,
                                               const gfx::PointF& position) {
  // Handle a double press the same way as a long press.
  if (state_ == INITIATING_GESTURE_PENDING &&
      // Check event time and position to ensure that the observed double
      // press corresponds to the active touch sequence. It should be ok to
      // check the exact times and positions here, since a tap down gesture
      // event is created directly from the corresponding down motion event when
      // the gesture is initially detected.
      touch_down_time_ == event_time && touch_down_position_ == position) {
    SetState(SELECTION_PENDING);
  }
}

void LongPressDragSelector::OnScrollBeginEvent() {
  SetState(INACTIVE);
}

void LongPressDragSelector::OnSelectionActivated() {
  if (state_ == SELECTION_PENDING)
    SetState(DRAG_PENDING);
}

void LongPressDragSelector::OnSelectionDeactivated() {
  SetState(INACTIVE);
}

void LongPressDragSelector::SetState(SelectionState state) {
  if (state_ == state)
    return;

  const bool was_dragging = state_ == DRAGGING;
  const bool was_active = IsActive();
  state_ = state;

  // TODO(jdduke): Add UMA for tracking relative longpress drag frequency.
  if (was_dragging)
    client_->OnDragEnd(*this);

  if (was_active != IsActive())
    client_->OnLongPressDragActiveStateChanged();
}

}  // namespace ui
