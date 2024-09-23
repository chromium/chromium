// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/scale_gesture_detector.h"

#include <limits.h>

#include <algorithm>
#include <cmath>

#include "base/check.h"
#include "base/logging.h"
#include "base/numerics/angle_conversions.h"
#include "base/numerics/math_constants.h"
#include "ui/events/gesture_detection/scale_gesture_listeners.h"
#include "ui/events/velocity_tracker/motion_event.h"

using base::TimeTicks;

namespace ui {
namespace {

const float kScaleFactor = .5f;

// Using a small epsilon when comparing slop distances allows pixel
// perfect slop determination when using fractional DPI coordinates
// (assuming the slop region and DPI scale are reasonably
// proportioned).
const float kSlopEpsilon = .05f;

}  // namespace

// Note: These constants were taken directly from the default (unscaled)
// versions found in Android's ViewConfiguration. Do not change these default
// values without explicitly consulting an OWNER.
ScaleGestureDetector::Config::Config()
    : span_slop(16),
      min_scaling_span(200),
      min_pinch_update_span_delta(0),
      stylus_scale_enabled(false) {}

ScaleGestureDetector::Config::~Config() {}

ScaleGestureDetector::ScaleGestureDetector(const Config& config,
                                           ScaleGestureListener* listener)
    : listener_(listener),
      stylus_scale_enabled_(config.stylus_scale_enabled),
      focus_x_(0),
      focus_y_(0),
      curr_angles_({}),
      prev_angles_({}),
      curr_span_(0),
      prev_span_(0),
      initial_span_(0),
      curr_span_x_(0),
      curr_span_y_(0),
      prev_span_x_(0),
      prev_span_y_(0),
      in_progress_(false),
      span_slop_(0),
      min_span_(0),
      anchored_scale_start_x_(0),
      anchored_scale_start_y_(0),
      anchored_scale_mode_(ANCHORED_SCALE_MODE_NONE),
      event_before_or_above_starting_gesture_event_(false) {
  DCHECK(listener_);

  span_slop_ = config.span_slop;
  min_span_ = config.min_scaling_span;
}

ScaleGestureDetector::~ScaleGestureDetector() {}

bool ScaleGestureDetector::OnTouchEvent(const MotionEvent& event) {
  curr_time_ = event.GetEventTime();

  const MotionEvent::Action action = event.GetAction();

  const int count = static_cast<int>(event.GetPointerCount());
  const bool is_stylus_button_down =
      (event.GetButtonState() & MotionEvent::BUTTON_STYLUS_PRIMARY) != 0;

  const bool anchored_scale_cancelled =
      anchored_scale_mode_ == ANCHORED_SCALE_MODE_STYLUS &&
      !is_stylus_button_down;

  const bool stream_complete =
      action == MotionEvent::Action::UP ||
      action == MotionEvent::Action::CANCEL || anchored_scale_cancelled ||
      (action == MotionEvent::Action::POINTER_DOWN && InAnchoredScaleMode());

  if (action == MotionEvent::Action::DOWN || stream_complete) {
    // Reset any scale in progress with the listener.
    // If it's an ACTION_DOWN we're beginning a new event stream.
    // This means the app probably didn't give us all the events. Shame on it.
    if (in_progress_) {
      listener_->OnScaleEnd(*this, event);
      ResetScaleWithSpan(0);
    } else if (InAnchoredScaleMode() && stream_complete) {
      ResetScaleWithSpan(0);
    }

    curr_angles_.clear();
    prev_angles_.clear();

    if (stream_complete)
      return true;
  }

  if (!in_progress_ && stylus_scale_enabled_ && !InAnchoredScaleMode() &&
      !stream_complete && is_stylus_button_down) {
    // Start of a stylus scale gesture.
    anchored_scale_start_x_ = event.GetX();
    anchored_scale_start_y_ = event.GetY();
    anchored_scale_mode_ = ANCHORED_SCALE_MODE_STYLUS;
    initial_span_ = 0;
  }

  const bool config_changed = action == MotionEvent::Action::DOWN ||
                              action == MotionEvent::Action::POINTER_UP ||
                              action == MotionEvent::Action::POINTER_DOWN ||
                              anchored_scale_cancelled;

  const bool pointer_up = action == MotionEvent::Action::POINTER_UP;
  const int skip_index = pointer_up ? event.GetActionIndex() : -1;

  // Determine focal point.
  float sum_x = 0, sum_y = 0;
  const int unreleased_point_count = pointer_up ? count - 1 : count;
  const float inverse_unreleased_point_count = 1.0f / unreleased_point_count;

  float focus_x;
  float focus_y;
  if (InAnchoredScaleMode()) {
    // In double tap mode, the focal pt is always where the double tap
    // gesture started.
    focus_x = anchored_scale_start_x_;
    focus_y = anchored_scale_start_y_;
    if (event.GetY() < focus_y) {
      event_before_or_above_starting_gesture_event_ = true;
    } else {
      event_before_or_above_starting_gesture_event_ = false;
    }
  } else {
    for (int i = 0; i < count; i++) {
      if (skip_index == i)
        continue;
      sum_x += event.GetX(i);
      sum_y += event.GetY(i);
    }

    focus_x = sum_x * inverse_unreleased_point_count;
    focus_y = sum_y * inverse_unreleased_point_count;
  }

  // Determine average deviation from focal point.
  float dev_sum_x = 0, dev_sum_y = 0;
  for (int i = 0; i < count; i++) {
    if (skip_index == i)
      continue;

    dev_sum_x += std::abs(event.GetX(i) - focus_x);
    dev_sum_y += std::abs(event.GetY(i) - focus_y);
  }

  // Insert the values of `curr_angles_` into `prev_angles_`. The
  // values of `curr_angles_` will be updated in the following blocks.
  std::swap(curr_angles_, prev_angles_);

  if (count != static_cast<int>(curr_angles_.size())) {
    // If the number of items in `curr_angles_` do not match the count,
    // we need to reconstruct this vector. These values will be
    // substituted in the next block.
    curr_angles_.resize(count, 0.f);
  }

  // `angles` are the angles between the horizontal axis and the lines
  // connecting each individual finger locations to the focal point.
  // They are stored so that their `ActionIndex` matches the index of
  // the vector.
  for (int i = 0; i < count; i++) {
    curr_angles_[i] = CalculateAngle(event, i, focus_x, focus_y);
  }

  const float dev_x = dev_sum_x * inverse_unreleased_point_count;
  const float dev_y = dev_sum_y * inverse_unreleased_point_count;

  // Span is the average distance between touch points through the focal point;
  // i.e. the diameter of the circle with a radius of the average deviation from
  // the focal point.
  const float span_x = dev_x * 2;
  const float span_y = dev_y * 2;
  float span;
  if (InAnchoredScaleMode()) {
    span = span_y;
  } else {
    span = std::sqrt(span_x * span_x + span_y * span_y);
  }

  // Dispatch begin/end events as needed.
  // If the configuration changes, notify the app to reset its current state by
  // beginning a fresh scale event stream.
  const bool was_in_progress = in_progress_;
  focus_x_ = focus_x;
  focus_y_ = focus_y;
  if (!InAnchoredScaleMode() && in_progress_ && config_changed) {
    listener_->OnScaleEnd(*this, event);
    ResetScaleWithSpan(span);
  }
  if (config_changed) {
    prev_span_x_ = curr_span_x_ = span_x;
    prev_span_y_ = curr_span_y_ = span_y;
    initial_span_ = prev_span_ = curr_span_ = span;
  }

  const float min_span = InAnchoredScaleMode() ? span_slop_ : min_span_;
  bool span_exceeds_min_span = span >= min_span + kSlopEpsilon ||
                               initial_span_ >= min_span + kSlopEpsilon;
  if (!in_progress_ && span_exceeds_min_span &&
      (was_in_progress ||
       std::abs(span - initial_span_) > span_slop_ + kSlopEpsilon)) {
    float zoom_sign = span > initial_span_ ? 1 : -1;

    prev_span_x_ = curr_span_x_ = span_x;
    prev_span_y_ = curr_span_y_ = span_y;
    curr_span_ = span;

    // To ensure we don't lose any delta when the first event crosses the min
    // and slop thresholds, the prev_span on the first update will be the point
    // at which zooming would have started.
    prev_span_ = std::max(initial_span_ + zoom_sign * span_slop_, min_span);

    prev_time_ = curr_time_;
    in_progress_ = listener_->OnScaleBegin(*this, event);
  }

  // Handle motion; focal point and span/scale factor are changing.
  if (action == MotionEvent::Action::MOVE) {
    curr_span_x_ = span_x;
    curr_span_y_ = span_y;
    curr_span_ = span;

    bool update_prev = true;

    if (in_progress_)
      update_prev = listener_->OnScale(*this, event);

    if (update_prev) {
      prev_span_x_ = curr_span_x_;
      prev_span_y_ = curr_span_y_;
      prev_span_ = curr_span_;
      prev_time_ = curr_time_;
      prev_angles_ = curr_angles_;
    }
  }

  if (!InAnchoredScaleMode() && in_progress_ &&
      span < min_span_ + kSlopEpsilon) {
    listener_->OnScaleEnd(*this, event);
    ResetScaleWithSpan(span);
  }

  return true;
}

bool ScaleGestureDetector::IsInProgress() const { return in_progress_; }

bool ScaleGestureDetector::InAnchoredScaleMode() const {
  return anchored_scale_mode_ != ANCHORED_SCALE_MODE_NONE;
}

float ScaleGestureDetector::GetFocusX() const { return focus_x_; }

float ScaleGestureDetector::GetFocusY() const { return focus_y_; }

float ScaleGestureDetector::GetCurrentSpan() const { return curr_span_; }

float ScaleGestureDetector::GetCurrentSpanX() const { return curr_span_x_; }

float ScaleGestureDetector::GetCurrentSpanY() const { return curr_span_y_; }

float ScaleGestureDetector::GetPreviousSpan() const { return prev_span_; }

float ScaleGestureDetector::GetPreviousSpanX() const { return prev_span_x_; }

float ScaleGestureDetector::GetPreviousSpanY() const { return prev_span_y_; }

float ScaleGestureDetector::GetScaleFactor() const {
  float curr_span = curr_span_;
  if (InAnchoredScaleMode()) {
    // Drag is moving up; the further away from the gesture start, the smaller
    // the span should be, the closer, the larger the span, and therefore the
    // larger the scale.
    const bool scale_up = (event_before_or_above_starting_gesture_event_ &&
                           (curr_span < prev_span_)) ||
                          (!event_before_or_above_starting_gesture_event_ &&
                           (curr_span > prev_span_));
    const float span_diff =
        (std::abs(1.f - (curr_span / prev_span_)) * kScaleFactor);
    return prev_span_ <= 0 ? 1.f
                           : (scale_up ? (1.f + span_diff) : (1.f - span_diff));
  }

  // If this will be the last update because this event crossed the min
  // threshold, calculate the update as if the event stopped right at the
  // boundary.
  if (curr_span < min_span_ + kSlopEpsilon)
    curr_span = min_span_;

  return prev_span_ > 0 ? curr_span / prev_span_ : 1;
}

float ScaleGestureDetector::GetAngleChange() const {
  int count = curr_angles_.size();
  if (count != static_cast<int>(prev_angles_.size()) || count == 0) {
    return 0;
  }

  float angle_change_sum = 0.f;
  for (int i = 0; i < count; ++i) {
    float angle_change = curr_angles_[i] - prev_angles_[i];

    // The angle difference should be in (-180, 180].
    if (angle_change <= -180.f) {
      angle_change += 360.f;
    } else if (angle_change > 180.f) {
      angle_change -= 360.f;
    }

    angle_change_sum += angle_change;
  }

  // Calculate the average angle change.
  const float inverse_valid_point_count = 1.f / count;
  return angle_change_sum * inverse_valid_point_count;
}

base::TimeDelta ScaleGestureDetector::GetTimeDelta() const {
  return curr_time_ - prev_time_;
}

base::TimeTicks ScaleGestureDetector::GetEventTime() const {
  return curr_time_;
}

bool ScaleGestureDetector::OnDoubleTap(const MotionEvent& ev) {
  // Double tap: start watching for a swipe.
  anchored_scale_start_x_ = ev.GetX();
  anchored_scale_start_y_ = ev.GetY();
  anchored_scale_mode_ = ANCHORED_SCALE_MODE_DOUBLE_TAP;
  return true;
}

void ScaleGestureDetector::ResetScaleWithSpan(float span) {
  in_progress_ = false;
  initial_span_ = span;
  anchored_scale_mode_ = ANCHORED_SCALE_MODE_NONE;
}

float ScaleGestureDetector::CalculateAngle(const MotionEvent& event,
                                           int action_index,
                                           float focus_x,
                                           float focus_y) const {
  DCHECK_GE(action_index, 0);
  DCHECK_LT(action_index, static_cast<int>(event.GetPointerCount()));
  const float delta_x = event.GetX(action_index) - focus_x;
  const float delta_y = event.GetY(action_index) - focus_y;
  if (delta_x != 0.f && delta_y != 0.f) {
    // `std::atan2` returns value in (-pi, pi].
    // `std::atan2(y, x)`'s value when both x and y are `0` depends
    // on the implementation, but we explicitly use `0` here.
    return base::RadToDeg(std::atan2(delta_y, delta_x));
  }
  return 0.f;
}

}  // namespace ui
