// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_INPUT_SCROLL_ELASTICITY_CONTROLLER_H_
#define UI_EVENTS_BLINK_INPUT_SCROLL_ELASTICITY_CONTROLLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "cc/input/overscroll_behavior.h"
#include "cc/input/scroll_elasticity_helper.h"
#include "third_party/blink/public/platform/web_gesture_event.h"
#include "third_party/blink/public/platform/web_input_event.h"

// InputScrollElasticityController is based on
// WebKit/Source/platform/mac/ScrollElasticityController.h
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

namespace cc {
struct InputHandlerScrollResult;
}  // namespace cc

namespace ui {

class InputScrollElasticityController {
 public:
  explicit InputScrollElasticityController(cc::ScrollElasticityHelper* helper);
  virtual ~InputScrollElasticityController();

  base::WeakPtr<InputScrollElasticityController> GetWeakPtr();

  // These methods that are "real" should only be called if the associated
  // event is not synthetic. Otherwise, calling them will disrupt elastic
  // scrolling.
  void ObserveRealScrollBegin(bool enter_momentum, bool leave_momentum);
  void ObserveScrollUpdate(const gfx::Vector2dF& event_delta,
                           const gfx::Vector2dF& unused_scroll_delta,
                           const base::TimeTicks event_timestamp,
                           const cc::OverscrollBehavior overscroll_behavior,
                           bool has_momentum);
  void ObserveRealScrollEnd(const base::TimeTicks event_timestamp);

  // Update the overscroll state based a gesture event that has been processed.
  // Note that this assumes that all events are coming from a single input
  // device. If the user simultaneously uses multiple input devices, Cocoa may
  // not correctly pass all the gesture begin and end events. In this case,
  // this class may disregard some scrolls that come in at unexpected times.
  void ObserveGestureEventAndResult(
      const blink::WebGestureEvent& gesture_event,
      const cc::InputHandlerScrollResult& scroll_result);

  void Animate(base::TimeTicks time);

  void ReconcileStretchAndScroll();

 private:
  enum State {
    // The initial state, during which the overscroll amount is zero and
    // there are no active or momentum scroll events coming in. This state
    // is briefly returned to between the active and momentum phases of a
    // scroll (if there is no overscroll).
    kStateInactive,
    // The state between receiving PhaseBegan/MayBegin and PhaseEnd/Cancelled,
    // corresponding to the period during which the user has fingers on the
    // trackpad. The overscroll amount is updated as input events are received.
    // When PhaseEnd is received, the state transitions to Inactive if there is
    // no overscroll and MomentumAnimated if there is non-zero overscroll.
    kStateActiveScroll,
    // The state between receiving a momentum PhaseBegan and PhaseEnd, while
    // there is no overscroll. The overscroll amount is updated as input events
    // are received. If the overscroll is ever non-zero then the state
    // immediately transitions to kStateMomentumAnimated.
    kStateMomentumScroll,
    // The state while the overscroll amount is updated by an animation. If
    // the user places fingers on the trackpad (a PhaseMayBegin is received)
    // then the state transition to kStateActiveScroll. Otherwise the state
    // transitions to Inactive when the overscroll amount becomes zero.
    kStateMomentumAnimated,
  };

  void UpdateVelocity(const gfx::Vector2dF& event_delta,
                      const base::TimeTicks& event_timestamp);
  void Overscroll(const gfx::Vector2dF& input_delta,
                  const gfx::Vector2dF& overscroll_delta);
  void EnterStateMomentumAnimated(
      const base::TimeTicks& triggering_event_timestamp);
  void EnterStateInactive();

  // Returns true if |direction| is pointing in a direction in which it is not
  // possible to scroll any farther horizontally (or vertically). It is only in
  // this circumstance that an overscroll in that direction may begin.
  bool PinnedHorizontally(float direction) const;
  bool PinnedVertically(float direction) const;
  // Whether or not the content of the page is scrollable horizontaly (or
  // vertically).
  bool CanScrollHorizontally() const;
  bool CanScrollVertically() const;

  cc::ScrollElasticityHelper* helper_;
  State state_;

  // If there is no overscroll, require a minimum overscroll delta before
  // starting the rubber-band effect. Track the amount of scrolling that has
  // has occurred but has not yet caused rubber-band stretching in
  // |pending_overscroll_delta_|.
  gfx::Vector2dF pending_overscroll_delta_;

  // Maintain a calculation of the velocity of the scroll, based on the input
  // scroll delta divide by the time between input events. Track this velocity
  // in |scroll_velocity| and the previous input event timestamp for finite
  // differencing in |last_scroll_event_timestamp_|.
  gfx::Vector2dF scroll_velocity;
  base::TimeTicks last_scroll_event_timestamp_;

  // The force of the rubber-band spring. This is equal to the cumulative sum
  // of all overscroll offsets since entering a non-Inactive state. This is
  // reset to zero only when entering the Inactive state.
  gfx::Vector2dF stretch_scroll_force_;

  // Momentum animation state. This state is valid only while the state is
  // MomentumAnimated, and is initialized in EnterStateMomentumAnimated.
  base::TimeTicks momentum_animation_start_time_;
  gfx::Vector2dF momentum_animation_initial_stretch_;
  gfx::Vector2dF momentum_animation_initial_velocity_;

  // This is set in response to a scroll (most likely programmatic) occuring
  // while animating the momentum phase. In this case, re-set the initial
  // velocity, stretch, and start time at the next frame (this is the same
  // behavior as would happen if the scroll were caused by an active scroll).
  bool momentum_animation_reset_at_next_frame_;

  bool received_overscroll_update_;
  cc::OverscrollBehavior overscroll_behavior_;

  base::WeakPtrFactory<InputScrollElasticityController> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(InputScrollElasticityController);
};

}  // namespace ui

#endif  // UI_EVENTS_BLINK_INPUT_SCROLL_ELASTICITY_CONTROLLER_H_
