// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_TEST_LAYER_ANIMATION_OBSERVER_H_
#define UI_COMPOSITOR_TEST_TEST_LAYER_ANIMATION_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer_animation_observer.h"

namespace ui {

class LayerAnimationSequence;

// Listens to animation ended notifications. Remembers the last sequence that
// it was notified about.
class TestLayerAnimationObserver : public LayerAnimationObserver {
 public:
  TestLayerAnimationObserver();
  ~TestLayerAnimationObserver() override;

  // Resets all the data tracking LayerAnimationObserver observations.
  void ResetLayerAnimationObserverations();

  // LayerAnimationObserver:
  void OnLayerAnimationScheduled(LayerAnimationSequence* sequence) override;
  void OnLayerAnimationStarted(LayerAnimationSequence* sequence) override;
  void OnLayerAnimationAborted(LayerAnimationSequence* sequence) override;
  void OnLayerAnimationEnded(LayerAnimationSequence* sequence) override;
  void OnLayerAnimationWillRepeat(LayerAnimationSequence* sequence) override;
  bool RequiresNotificationWhenAnimatorDestroyed() const override;

  const LayerAnimationSequence* last_attached_sequence() const {
    return last_attached_sequence_;
  }

  int last_attached_sequence_epoch() const {
    return last_attached_sequence_epoch_;
  }

  const LayerAnimationSequence* last_scheduled_sequence() const {
    return last_scheduled_sequence_;
  }

  int last_scheduled_sequence_epoch() const {
    return last_scheduled_sequence_epoch_;
  }

  const LayerAnimationSequence* last_started_sequence() const {
    return last_started_sequence_;
  }

  int last_started_sequence_epoch() const {
    return last_started_sequence_epoch_;
  }

  const LayerAnimationSequence* last_aborted_sequence() const {
    return last_aborted_sequence_;
  }

  int last_aborted_sequence_epoch() const {
    return last_aborted_sequence_epoch_;
  }

  const LayerAnimationSequence* last_ended_sequence() const {
    return last_ended_sequence_;
  }

  int last_ended_sequence_epoch() const { return last_ended_sequence_epoch_; }

  const LayerAnimationSequence* last_repetition_ended_sequence() const {
    return last_repetition_ended_sequence_;
  }

  int last_repetition_ended_sequence_epoch() const {
    return last_repetition_ended_sequence_epoch_;
  }

  const LayerAnimationSequence* last_detached_sequence() const {
    return last_detached_sequence_;
  }

  int last_detached_sequence_epoch() const {
    return last_detached_sequence_epoch_;
  }

  void set_requires_notification_when_animator_destroyed(bool value) {
    requires_notification_when_animator_destroyed_ = value;
  }

  testing::AssertionResult NoEventsObserved();

  testing::AssertionResult AttachedEpochIsBeforeScheduledEpoch();

  testing::AssertionResult ScheduledEpochIsBeforeStartedEpoch();

  testing::AssertionResult StartedEpochIsBeforeEndedEpoch();

  testing::AssertionResult StartedEpochIsBeforeAbortedEpoch();

  testing::AssertionResult AbortedEpochIsBeforeStartedEpoch();

  testing::AssertionResult AbortedEpochIsBeforeDetachedEpoch();

  testing::AssertionResult EndedEpochIsBeforeStartedEpoch();

  testing::AssertionResult EndedEpochIsBeforeDetachedEpoch();

 protected:
  // LayerAnimationObserver:
  void OnAttachedToSequence(LayerAnimationSequence* sequence) override;
  void OnDetachedFromSequence(LayerAnimationSequence* sequence) override;

 private:
  int next_epoch_;

  raw_ptr<const LayerAnimationSequence, AcrossTasksDanglingUntriaged>
      last_attached_sequence_;
  int last_attached_sequence_epoch_;

  raw_ptr<const LayerAnimationSequence, AcrossTasksDanglingUntriaged>
      last_scheduled_sequence_;
  int last_scheduled_sequence_epoch_;

  raw_ptr<const LayerAnimationSequence, AcrossTasksDanglingUntriaged>
      last_started_sequence_;
  int last_started_sequence_epoch_;

  raw_ptr<const LayerAnimationSequence, AcrossTasksDanglingUntriaged>
      last_aborted_sequence_;
  int last_aborted_sequence_epoch_;

  raw_ptr<const LayerAnimationSequence, AcrossTasksDanglingUntriaged>
      last_ended_sequence_;
  int last_ended_sequence_epoch_;

  raw_ptr<const LayerAnimationSequence> last_repetition_ended_sequence_;
  int last_repetition_ended_sequence_epoch_;

  raw_ptr<const LayerAnimationSequence, AcrossTasksDanglingUntriaged>
      last_detached_sequence_;
  int last_detached_sequence_epoch_;

  bool requires_notification_when_animator_destroyed_;

  // Copy and assign are allowed.
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_TEST_LAYER_ANIMATION_OBSERVER_H_
