// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/fling_booster.h"

#include "base/trace_event/trace_event.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;

namespace {
// Minimum fling velocity required for the active fling and new fling for the
// two to accumulate.
const double kMinBoostFlingSpeedSquare = 350. * 350.;

// Minimum velocity for the active touch scroll to preserve (boost) an active
// fling for which cancellation has been deferred.
const double kMinBoostTouchScrollSpeedSquare = 150 * 150.;

// Timeout window after which the active fling will be cancelled if no animation
// ticks, scrolls or flings of sufficient velocity relative to the current fling
// are received. The default value on Android native views is 40ms, but we use a
// slightly increased value to accomodate small IPC message delays.
constexpr base::TimeDelta kFlingBoostTimeoutDelay =
    base::TimeDelta::FromSecondsD(0.05);
}  // namespace

namespace ui {

gfx::Vector2dF FlingBooster::GetVelocityForFlingStart(
    const blink::WebGestureEvent& fling_start) {
  DCHECK_EQ(blink::WebInputEvent::kGestureFlingStart, fling_start.GetType());
  gfx::Vector2dF velocity(fling_start.data.fling_start.velocity_x,
                          fling_start.data.fling_start.velocity_y);
  TRACE_EVENT2("input", "FlingBooster::GetVelocityForFlingStart", "vx",
               velocity.x(), "vy", velocity.y());

  if (ShouldBoostFling(fling_start)) {
    velocity += previous_fling_starting_velocity_;
    TRACE_EVENT_INSTANT2("input", "Boosted", TRACE_EVENT_SCOPE_THREAD, "vx",
                         velocity.x(), "vy", velocity.y());
  }

  Reset();

  previous_fling_starting_velocity_ = velocity;
  current_fling_velocity_ = velocity;
  source_device_ = fling_start.SourceDevice();
  modifiers_ = fling_start.GetModifiers();

  return previous_fling_starting_velocity_;
}

void FlingBooster::ObserveGestureEvent(const WebGestureEvent& gesture_event) {
  TRACE_EVENT1("input", "FlingBooster::ObserveGestureEvent", "type",
               WebInputEvent::GetName(gesture_event.GetType()));
  if (previous_fling_starting_velocity_.IsZero())
    return;

  // If a cutoff time is set (we're waiting for a boost) and we've now exceeded
  // it, reset the booster since we're not going to boost the current gesture.
  if (!cutoff_time_for_boost_.is_null() &&
      gesture_event.TimeStamp() > cutoff_time_for_boost_) {
    TRACE_EVENT_INSTANT0("input", "Timeout", TRACE_EVENT_SCOPE_THREAD);
    Reset();
    return;
  }

  // Gestures from a different source should prevent boosting.
  if (gesture_event.SourceDevice() != source_device_) {
    Reset();
    return;
  }

  switch (gesture_event.GetType()) {
    case WebInputEvent::kGestureScrollBegin: {
      cutoff_time_for_boost_ =
          gesture_event.TimeStamp() + kFlingBoostTimeoutDelay;
      break;
    }
    case WebInputEvent::kGestureScrollUpdate: {
      if (gesture_event.data.scroll_update.inertial_phase ==
          WebGestureEvent::InertialPhaseState::kMomentum) {
        return;
      }

      if (cutoff_time_for_boost_.is_null())
        return;

      // If the user scrolls in a direction counter to the current scroll, don't
      // boost.
      gfx::Vector2dF delta(gesture_event.data.scroll_update.delta_x,
                           gesture_event.data.scroll_update.delta_y);
      if (gfx::DotProduct(previous_fling_starting_velocity_, delta) <= 0) {
        TRACE_EVENT_INSTANT0("input", "Direction", TRACE_EVENT_SCOPE_THREAD);
        Reset();
        return;
      }

      // Scrolls must be of sufficient velocity to maintain the active fling.
      // Unfortunately we can't simply use the velocity_x|y fields on the
      // gesture event because they're not populated when converting from
      // Android's MotionEvents.
      if (!previous_boosting_scroll_timestamp_.is_null()) {
        const double time_since_last_boost_event =
            (gesture_event.TimeStamp() - previous_boosting_scroll_timestamp_)
                .InSecondsF();
        if (time_since_last_boost_event >= 0.001) {
          const gfx::Vector2dF scroll_velocity =
              gfx::ScaleVector2d(delta, 1. / time_since_last_boost_event);
          if (scroll_velocity.LengthSquared() <
              kMinBoostTouchScrollSpeedSquare) {
            TRACE_EVENT_INSTANT0("input", "Velocity", TRACE_EVENT_SCOPE_THREAD);
            Reset();
            return;
          }
        }
      }

      previous_boosting_scroll_timestamp_ = gesture_event.TimeStamp();
      cutoff_time_for_boost_ =
          gesture_event.TimeStamp() + kFlingBoostTimeoutDelay;
      break;
    }
    case WebInputEvent::kGestureScrollEnd: {
      previous_boosting_scroll_timestamp_ = base::TimeTicks();
      break;
    }
    case WebInputEvent::kGestureFlingCancel: {
      if (gesture_event.data.fling_cancel.prevent_boosting) {
        TRACE_EVENT_INSTANT0("input", "GFC PreventBoosting",
                             TRACE_EVENT_SCOPE_THREAD);
        Reset();
        return;
      }

      previous_boosting_scroll_timestamp_ = base::TimeTicks();
      cutoff_time_for_boost_ =
          gesture_event.TimeStamp() + kFlingBoostTimeoutDelay;
      break;
    }
    default:
      break;
  }
}

void FlingBooster::ObserveProgressFling(
    const gfx::Vector2dF& current_velocity) {
  TRACE_EVENT2("input", "FlingBooster::ObserveProgressFling", "vx",
               current_velocity.x(), "vy", current_velocity.y());
  if (previous_fling_starting_velocity_.IsZero())
    return;
  current_fling_velocity_ = current_velocity;
}

bool FlingBooster::ShouldBoostFling(const WebGestureEvent& fling_start_event) {
  DCHECK_EQ(WebInputEvent::kGestureFlingStart, fling_start_event.GetType());
  if (previous_fling_starting_velocity_.IsZero()) {
    TRACE_EVENT_INSTANT0("input", "No Boost - NoActiveFling",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  if (source_device_ != fling_start_event.SourceDevice()) {
    TRACE_EVENT_INSTANT0("input", "No Boost - SourceDevice",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  if (modifiers_ != fling_start_event.GetModifiers()) {
    TRACE_EVENT_INSTANT0("input", "No Boost - Modifiers",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  if (cutoff_time_for_boost_.is_null()) {
    TRACE_EVENT_INSTANT0("input", "No Boost - CutoffTimeUnset",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  if (fling_start_event.TimeStamp() > cutoff_time_for_boost_) {
    TRACE_EVENT_INSTANT0("input", "No Boost - Timeout",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  gfx::Vector2dF new_fling_velocity(
      fling_start_event.data.fling_start.velocity_x,
      fling_start_event.data.fling_start.velocity_y);

  if (gfx::DotProduct(previous_fling_starting_velocity_, new_fling_velocity) <=
      0) {
    TRACE_EVENT_INSTANT0("input", "No Boost - Direction",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  if (current_fling_velocity_.LengthSquared() < kMinBoostFlingSpeedSquare) {
    TRACE_EVENT_INSTANT0("input", "No Boost - CurrentVelocity",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  if (new_fling_velocity.LengthSquared() < kMinBoostFlingSpeedSquare) {
    TRACE_EVENT_INSTANT0("input", "No Boost - NewVelocity",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  return true;
}

void FlingBooster::Reset() {
  TRACE_EVENT0("input", "FlingBooster::Reset");
  cutoff_time_for_boost_ = base::TimeTicks();
  previous_fling_starting_velocity_ = gfx::Vector2dF();
  current_fling_velocity_ = gfx::Vector2dF();
  source_device_ = blink::WebGestureDevice::kUninitialized;
  modifiers_ = 0;
  previous_boosting_scroll_timestamp_ = base::TimeTicks();
}

}  // namespace ui
