// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/input_scroll_elasticity_controller.h"

#include <math.h>

#include <algorithm>

#include "base/bind.h"
#include "cc/input/input_handler.h"
#include "ui/events/types/scroll_types.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

// InputScrollElasticityController is based on
// WebKit/Source/platform/mac/InputScrollElasticityController.mm
/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

namespace ui {

namespace {

const float kScrollVelocityZeroingTimeout = 0.10f;
const float kRubberbandMinimumRequiredDeltaBeforeStretch = 10;

const float kRubberbandStiffness = 20;
const float kRubberbandAmplitude = 0.31f;
const float kRubberbandPeriod = 1.6f;

// For these functions which compute the stretch amount, always return a
// rounded value, instead of a floating-point value. The reason for this is
// that Blink's scrolling can become erratic with fractional scroll amounts (in
// particular, if you have a scroll offset of 0.5, Blink will never actually
// bring that value back to 0, which breaks the logic used to determine if a
// layer is pinned in a direction).

gfx::Vector2d StretchAmountForTimeDelta(const gfx::Vector2dF& initial_position,
                                        const gfx::Vector2dF& initial_velocity,
                                        float elapsed_time) {
  // Compute the stretch amount at a given time after some initial conditions.
  // Do this by first computing an intermediary position given the initial
  // position, initial velocity, time elapsed, and no external forces. Then
  // take the intermediary position and damp it towards zero by multiplying
  // against a negative exponential.
  float amplitude = kRubberbandAmplitude;
  float period = kRubberbandPeriod;
  float critical_dampening_factor =
      expf((-elapsed_time * kRubberbandStiffness) / period);

  return gfx::ToRoundedVector2d(gfx::ScaleVector2d(
      initial_position +
          gfx::ScaleVector2d(initial_velocity, elapsed_time * amplitude),
      critical_dampening_factor));
}

gfx::Vector2d StretchAmountForReboundDelta(const gfx::Vector2dF& delta) {
  float stiffness = std::max(kRubberbandStiffness, 1.0f);
  return gfx::ToRoundedVector2d(gfx::ScaleVector2d(delta, 1.0f / stiffness));
}

gfx::Vector2d StretchScrollForceForStretchAmount(const gfx::Vector2dF& delta) {
  return gfx::ToRoundedVector2d(
      gfx::ScaleVector2d(delta, kRubberbandStiffness));
}

}  // namespace

InputScrollElasticityController::InputScrollElasticityController(
    cc::ScrollElasticityHelper* helper)
    : helper_(helper),
      state_(kStateInactive),
      momentum_animation_reset_at_next_frame_(false),
      received_overscroll_update_(false) {}

InputScrollElasticityController::~InputScrollElasticityController() {
}

base::WeakPtr<InputScrollElasticityController>
InputScrollElasticityController::GetWeakPtr() {
  if (helper_)
    return weak_factory_.GetWeakPtr();
  return base::WeakPtr<InputScrollElasticityController>();
}

void InputScrollElasticityController::ObserveRealScrollBegin(
    bool enter_momentum,
    bool leave_momentum) {
  if (enter_momentum) {
    if (state_ == kStateInactive)
      state_ = kStateMomentumScroll;
  } else if (leave_momentum) {
    scroll_velocity = gfx::Vector2dF();
    last_scroll_event_timestamp_ = base::TimeTicks();
    state_ = kStateActiveScroll;
    pending_overscroll_delta_ = gfx::Vector2dF();
  }
}

void InputScrollElasticityController::ObserveScrollUpdate(
    const gfx::Vector2dF& event_delta,
    const gfx::Vector2dF& unused_scroll_delta,
    const base::TimeTicks event_timestamp,
    const cc::OverscrollBehavior overscroll_behavior,
    bool has_momentum) {
  if (state_ == kStateMomentumAnimated || state_ == kStateInactive)
    return;

  if (!received_overscroll_update_ && !unused_scroll_delta.IsZero()) {
    overscroll_behavior_ = overscroll_behavior;
    received_overscroll_update_ = true;
  }

  UpdateVelocity(event_delta, event_timestamp);
  Overscroll(event_delta, unused_scroll_delta);
  if (has_momentum && !helper_->StretchAmount().IsZero())
    EnterStateMomentumAnimated(event_timestamp);
}

void InputScrollElasticityController::ObserveRealScrollEnd(
    const base::TimeTicks event_timestamp) {
  if (state_ == kStateMomentumAnimated || state_ == kStateInactive)
    return;

  if (helper_->StretchAmount().IsZero()) {
    EnterStateInactive();
  } else {
    EnterStateMomentumAnimated(event_timestamp);
  }
}

void InputScrollElasticityController::ObserveGestureEventAndResult(
    const blink::WebGestureEvent& gesture_event,
    const cc::InputHandlerScrollResult& scroll_result) {
  base::TimeTicks event_timestamp = gesture_event.TimeStamp();

  switch (gesture_event.GetType()) {
    case blink::WebInputEvent::kGestureScrollBegin: {
      received_overscroll_update_ = false;
      overscroll_behavior_ = cc::OverscrollBehavior();
      if (gesture_event.data.scroll_begin.synthetic)
        return;

      bool enter_momentum =
          gesture_event.data.scroll_begin.inertial_phase ==
          blink::WebGestureEvent::InertialPhaseState::kMomentum;
      bool leave_momentum =
          gesture_event.data.scroll_begin.inertial_phase ==
              blink::WebGestureEvent::InertialPhaseState::kNonMomentum &&
          gesture_event.data.scroll_begin.delta_hint_units ==
              ui::input_types::ScrollGranularity::kScrollByPrecisePixel;
      ObserveRealScrollBegin(enter_momentum, leave_momentum);
      break;
    }
    case blink::WebInputEvent::kGestureScrollUpdate: {
      gfx::Vector2dF event_delta(-gesture_event.data.scroll_update.delta_x,
                                 -gesture_event.data.scroll_update.delta_y);
      bool has_momentum = gesture_event.data.scroll_update.inertial_phase ==
                          blink::WebGestureEvent::InertialPhaseState::kMomentum;
      ObserveScrollUpdate(event_delta, scroll_result.unused_scroll_delta,
                          event_timestamp, scroll_result.overscroll_behavior,
                          has_momentum);
      break;
    }
    case blink::WebInputEvent::kGestureScrollEnd: {
      if (gesture_event.data.scroll_end.synthetic)
        return;
      ObserveRealScrollEnd(event_timestamp);
      break;
    }
    default:
      break;
  }
}

void InputScrollElasticityController::UpdateVelocity(
    const gfx::Vector2dF& event_delta,
    const base::TimeTicks& event_timestamp) {
  float time_delta =
      (event_timestamp - last_scroll_event_timestamp_).InSecondsF();
  if (time_delta < kScrollVelocityZeroingTimeout && time_delta > 0) {
    scroll_velocity = gfx::Vector2dF(event_delta.x() / time_delta,
                                     event_delta.y() / time_delta);
  } else {
    scroll_velocity = gfx::Vector2dF();
  }
  last_scroll_event_timestamp_ = event_timestamp;
}

void InputScrollElasticityController::Overscroll(
    const gfx::Vector2dF& input_delta,
    const gfx::Vector2dF& overscroll_delta) {
  // The effect can be dynamically disabled by setting disallowing user
  // scrolling. When disabled, disallow active or momentum overscrolling, but
  // allow any current overscroll to animate back.
  if (!helper_->IsUserScrollable())
    return;

  gfx::Vector2dF adjusted_overscroll_delta =
      pending_overscroll_delta_ + overscroll_delta;
  pending_overscroll_delta_ = gfx::Vector2dF();

  // Only allow one direction to overscroll at a time, and slightly prefer
  // scrolling vertically by applying the equal case to delta_y.
  if (fabsf(input_delta.y()) >= fabsf(input_delta.x()))
    adjusted_overscroll_delta.set_x(0);
  else
    adjusted_overscroll_delta.set_y(0);

  // Don't allow overscrolling in a direction where scrolling is possible.
  if (!PinnedHorizontally(adjusted_overscroll_delta.x()))
    adjusted_overscroll_delta.set_x(0);
  if (!PinnedVertically(adjusted_overscroll_delta.y()))
    adjusted_overscroll_delta.set_y(0);

  // Don't allow overscrolling in a direction that has
  // OverscrollBehaviorTypeNone.
  if (overscroll_behavior_.x ==
      cc::OverscrollBehavior::kOverscrollBehaviorTypeNone)
    adjusted_overscroll_delta.set_x(0);
  if (overscroll_behavior_.y ==
      cc::OverscrollBehavior::kOverscrollBehaviorTypeNone)
    adjusted_overscroll_delta.set_y(0);

  // Require a minimum of 10 units of overscroll before starting the rubber-band
  // stretch effect, so that small stray motions don't trigger it. If that
  // minimum isn't met, save what remains in |pending_overscroll_delta_| for
  // the next event.
  gfx::Vector2dF old_stretch_amount = helper_->StretchAmount();
  gfx::Vector2dF stretch_scroll_force_delta;
  if (old_stretch_amount.x() != 0 ||
      fabsf(adjusted_overscroll_delta.x()) >=
          kRubberbandMinimumRequiredDeltaBeforeStretch) {
    stretch_scroll_force_delta.set_x(adjusted_overscroll_delta.x());
  } else {
    pending_overscroll_delta_.set_x(adjusted_overscroll_delta.x());
  }
  if (old_stretch_amount.y() != 0 ||
      fabsf(adjusted_overscroll_delta.y()) >=
          kRubberbandMinimumRequiredDeltaBeforeStretch) {
    stretch_scroll_force_delta.set_y(adjusted_overscroll_delta.y());
  } else {
    pending_overscroll_delta_.set_y(adjusted_overscroll_delta.y());
  }

  // Update the stretch amount according to the spring equations.
  if (stretch_scroll_force_delta.IsZero())
    return;
  stretch_scroll_force_ += stretch_scroll_force_delta;
  gfx::Vector2dF new_stretch_amount =
      StretchAmountForReboundDelta(stretch_scroll_force_);
  helper_->SetStretchAmount(new_stretch_amount);
}

void InputScrollElasticityController::EnterStateInactive() {
  DCHECK_NE(kStateInactive, state_);
  DCHECK(helper_->StretchAmount().IsZero());
  state_ = kStateInactive;
  stretch_scroll_force_ = gfx::Vector2dF();
}

void InputScrollElasticityController::EnterStateMomentumAnimated(
    const base::TimeTicks& triggering_event_timestamp) {
  DCHECK_NE(kStateMomentumAnimated, state_);
  state_ = kStateMomentumAnimated;

  momentum_animation_start_time_ = triggering_event_timestamp;
  momentum_animation_initial_stretch_ = helper_->StretchAmount();
  momentum_animation_initial_velocity_ = scroll_velocity;
  momentum_animation_reset_at_next_frame_ = false;

  // Similarly to the logic in Overscroll, prefer vertical scrolling to
  // horizontal scrolling.
  if (fabsf(momentum_animation_initial_velocity_.y()) >=
      fabsf(momentum_animation_initial_velocity_.x()))
    momentum_animation_initial_velocity_.set_x(0);

  if (!CanScrollHorizontally())
    momentum_animation_initial_velocity_.set_x(0);

  if (!CanScrollVertically())
    momentum_animation_initial_velocity_.set_y(0);

  // TODO(crbug.com/394562): This can go away once input is batched to the front
  // of the frame? Then Animate() would always happen after this, so it would
  // have a chance to tick the animation there and would return if any
  // animations were active.
  helper_->RequestOneBeginFrame();
}

void InputScrollElasticityController::Animate(base::TimeTicks time) {
  if (state_ != kStateMomentumAnimated)
    return;

  if (momentum_animation_reset_at_next_frame_) {
    momentum_animation_start_time_ = time;
    momentum_animation_initial_stretch_ = helper_->StretchAmount();
    momentum_animation_initial_velocity_ = gfx::Vector2dF();
    momentum_animation_reset_at_next_frame_ = false;
  }

  float time_delta =
      std::max((time - momentum_animation_start_time_).InSecondsF(), 0.0);

  gfx::Vector2dF old_stretch_amount = helper_->StretchAmount();
  gfx::Vector2dF new_stretch_amount = StretchAmountForTimeDelta(
      momentum_animation_initial_stretch_, momentum_animation_initial_velocity_,
      time_delta);
  gfx::Vector2dF stretch_delta = new_stretch_amount - old_stretch_amount;

  // If the new stretch amount is near zero, set it directly to zero and enter
  // the inactive state.
  if (fabs(new_stretch_amount.x()) < 1 && fabs(new_stretch_amount.y()) < 1) {
    helper_->SetStretchAmount(gfx::Vector2dF());
    EnterStateInactive();
    return;
  }

  // If we are not pinned in the direction of the delta, then the delta is only
  // allowed to decrease the existing stretch -- it cannot increase a stretch
  // until it is pinned.
  if (!PinnedHorizontally(stretch_delta.x())) {
    if (stretch_delta.x() > 0 && old_stretch_amount.x() < 0)
      stretch_delta.set_x(std::min(stretch_delta.x(), -old_stretch_amount.x()));
    else if (stretch_delta.x() < 0 && old_stretch_amount.x() > 0)
      stretch_delta.set_x(std::max(stretch_delta.x(), -old_stretch_amount.x()));
    else
      stretch_delta.set_x(0);
  }
  if (!PinnedVertically(stretch_delta.y())) {
    if (stretch_delta.y() > 0 && old_stretch_amount.y() < 0)
      stretch_delta.set_y(std::min(stretch_delta.y(), -old_stretch_amount.y()));
    else if (stretch_delta.y() < 0 && old_stretch_amount.y() > 0)
      stretch_delta.set_y(std::max(stretch_delta.y(), -old_stretch_amount.y()));
    else
      stretch_delta.set_y(0);
  }
  new_stretch_amount = old_stretch_amount + stretch_delta;

  stretch_scroll_force_ =
      StretchScrollForceForStretchAmount(new_stretch_amount);
  helper_->SetStretchAmount(new_stretch_amount);
  // TODO(danakj): Make this a return value back to the compositor to have it
  // schedule another frame and/or a draw. (Also, crbug.com/551138.)
  helper_->RequestOneBeginFrame();
}

bool InputScrollElasticityController::PinnedHorizontally(
    float direction) const {
  gfx::ScrollOffset scroll_offset = helper_->ScrollOffset();
  gfx::ScrollOffset max_scroll_offset = helper_->MaxScrollOffset();
  if (direction < 0)
    return scroll_offset.x() <= 0;
  if (direction > 0)
    return scroll_offset.x() >= max_scroll_offset.x();
  return false;
}

bool InputScrollElasticityController::PinnedVertically(float direction) const {
  gfx::ScrollOffset scroll_offset = helper_->ScrollOffset();
  gfx::ScrollOffset max_scroll_offset = helper_->MaxScrollOffset();
  if (direction < 0)
    return scroll_offset.y() <= 0;
  if (direction > 0)
    return scroll_offset.y() >= max_scroll_offset.y();
  return false;
}

bool InputScrollElasticityController::CanScrollHorizontally() const {
  return helper_->MaxScrollOffset().x() > 0;
}

bool InputScrollElasticityController::CanScrollVertically() const {
  return helper_->MaxScrollOffset().y() > 0;
}

void InputScrollElasticityController::ReconcileStretchAndScroll() {
  gfx::Vector2dF stretch = helper_->StretchAmount();
  if (stretch.IsZero())
    return;

  gfx::ScrollOffset scroll_offset = helper_->ScrollOffset();
  gfx::ScrollOffset max_scroll_offset = helper_->MaxScrollOffset();

  // Compute stretch_adjustment which will be added to |stretch| and subtracted
  // from the |scroll_offset|.
  gfx::Vector2dF stretch_adjustment;
  if (stretch.x() < 0 && scroll_offset.x() > 0) {
    stretch_adjustment.set_x(
        std::min(-stretch.x(), static_cast<float>(scroll_offset.x())));
  }
  if (stretch.x() > 0 && scroll_offset.x() < max_scroll_offset.x()) {
    stretch_adjustment.set_x(std::max(
        -stretch.x(),
        static_cast<float>(scroll_offset.x() - max_scroll_offset.x())));
  }
  if (stretch.y() < 0 && scroll_offset.y() > 0) {
    stretch_adjustment.set_y(
        std::min(-stretch.y(), static_cast<float>(scroll_offset.y())));
  }
  if (stretch.y() > 0 && scroll_offset.y() < max_scroll_offset.y()) {
    stretch_adjustment.set_y(std::max(
        -stretch.y(),
        static_cast<float>(scroll_offset.y() - max_scroll_offset.y())));
  }

  if (stretch_adjustment.IsZero())
    return;

  gfx::Vector2dF new_stretch_amount = stretch + stretch_adjustment;
  helper_->ScrollBy(-stretch_adjustment);
  helper_->SetStretchAmount(new_stretch_amount);

  // Update the internal state for the active scroll or animation to avoid
  // discontinuities.
  switch (state_) {
    case kStateActiveScroll:
      stretch_scroll_force_ =
          StretchScrollForceForStretchAmount(new_stretch_amount);
      break;
    case kStateMomentumAnimated:
      momentum_animation_reset_at_next_frame_ = true;
      break;
    default:
      // These cases should not be hit because the stretch must be zero in the
      // Inactive and MomentumScroll states.
      NOTREACHED();
      break;
  }
}

}  // namespace ui
