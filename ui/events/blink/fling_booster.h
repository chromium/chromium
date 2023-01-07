// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_FLING_BOOSTER_H_
#define UI_EVENTS_BLINK_FLING_BOOSTER_H_

#include "base/time/time.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"

namespace ui {

// This class is used to track fling state and provide "fling boosting".
// Boosting is a feature where successive flings can repeatedly increase the
// fling velocity so that users can scroll through long documents. This
// boosting logic occurs only in certain circumstances so we track the state
// and conditions in this class. The FlingController will request the velocity
// for all flings from this class; if FlingBooster decides the fling should be
// boosted it'll add the new fling's velocity to the previous one's.
class FlingBooster {
 public:
  FlingBooster() = default;

  FlingBooster(const FlingBooster&) = delete;
  FlingBooster& operator=(const FlingBooster&) = delete;

  gfx::Vector2dF GetVelocityForFlingStart(
      const blink::WebGestureEvent& gesture_start);
  void ObserveGestureEvent(const blink::WebGestureEvent& gesture_event);
  void ObserveProgressFling(const gfx::Vector2dF& current_velocity);
  void Reset();

 private:
  bool ShouldBoostFling(const blink::WebGestureEvent& fling_start_event);


  // When non-null, the current gesture stream is being considered for
  // boosting. If a fling hasn't occurred by this time, we won't cause a boost.
  // Note, however, that we'll extend this time as we see scroll updates.
  base::TimeTicks cutoff_time_for_boost_;

  // Tracks the (possibly boosted) velocity used at the previous FlingStart.
  // When a new fling is started and we decide to boost, we'll add this
  // velocity to it.
  gfx::Vector2dF previous_fling_starting_velocity_;

  // Tracks the current fling's velocity as it decays. We'll prevent boosting
  // if this crosses the kMinBoostFlingSpeedSquare threshold.
  gfx::Vector2dF current_fling_velocity_;

  // These store the current active fling source device and modifier keys (e.g.
  // Ctrl) since a new fling start event must have the same source device and
  // modifiers to be able to boost the active fling.
  blink::WebGestureDevice source_device_ =
      blink::WebGestureDevice::kUninitialized;
  int modifiers_ = 0;

  // Track the last timestamp we've seen a scroll update that we're evaluating
  // as a boost. This is used to calculate the velocity; if it's too slow we'll
  // avoid boosting.
  base::TimeTicks previous_boosting_scroll_timestamp_;
};

}  // namespace ui

#endif  // UI_EVENTS_BLINK_FLING_BOOSTER_H_
