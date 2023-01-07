// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/test_layer_animation_observer.h"

#include <cstddef>

namespace ui {

TestLayerAnimationObserver::TestLayerAnimationObserver()
    : next_epoch_(0),
      last_attached_sequence_(nullptr),
      last_attached_sequence_epoch_(-1),
      last_scheduled_sequence_(nullptr),
      last_scheduled_sequence_epoch_(-1),
      last_started_sequence_(nullptr),
      last_started_sequence_epoch_(-1),
      last_aborted_sequence_(nullptr),
      last_aborted_sequence_epoch_(-1),
      last_ended_sequence_(nullptr),
      last_ended_sequence_epoch_(-1),
      last_repetition_ended_sequence_(nullptr),
      last_repetition_ended_sequence_epoch_(-1),
      last_detached_sequence_(nullptr),
      last_detached_sequence_epoch_(-1),
      requires_notification_when_animator_destroyed_(false) {}

TestLayerAnimationObserver::~TestLayerAnimationObserver() {
}

void TestLayerAnimationObserver::ResetLayerAnimationObserverations() {
  next_epoch_ = 0;
  last_attached_sequence_ = nullptr;
  last_attached_sequence_epoch_ = -1;
  last_scheduled_sequence_ = nullptr;
  last_scheduled_sequence_epoch_ = -1;
  last_started_sequence_ = nullptr;
  last_started_sequence_epoch_ = -1;
  last_aborted_sequence_ = nullptr;
  last_aborted_sequence_epoch_ = -1;
  last_ended_sequence_ = nullptr;
  last_ended_sequence_epoch_ = -1;
  last_repetition_ended_sequence_ = nullptr;
  last_repetition_ended_sequence_epoch_ = -1;
  last_detached_sequence_ = nullptr;
  last_detached_sequence_epoch_ = -1;
}

void TestLayerAnimationObserver::OnAttachedToSequence(
    LayerAnimationSequence* sequence) {
  last_attached_sequence_ = sequence;
  last_attached_sequence_epoch_ = next_epoch_++;
}

void TestLayerAnimationObserver::OnLayerAnimationScheduled(
    LayerAnimationSequence* sequence) {
  last_scheduled_sequence_ = sequence;
  last_scheduled_sequence_epoch_ = next_epoch_++;
}

void TestLayerAnimationObserver::OnLayerAnimationStarted(
    LayerAnimationSequence* sequence) {
  last_started_sequence_ = sequence;
  last_started_sequence_epoch_ = next_epoch_++;
}

void TestLayerAnimationObserver::OnLayerAnimationAborted(
    LayerAnimationSequence* sequence) {
  last_aborted_sequence_ = sequence;
  last_aborted_sequence_epoch_ = next_epoch_++;
}

void TestLayerAnimationObserver::OnLayerAnimationEnded(
    LayerAnimationSequence* sequence) {
  last_ended_sequence_ = sequence;
  last_ended_sequence_epoch_ = next_epoch_++;
}

void TestLayerAnimationObserver::OnLayerAnimationWillRepeat(
    LayerAnimationSequence* sequence) {
  last_repetition_ended_sequence_ = sequence;
  last_repetition_ended_sequence_epoch_ = next_epoch_++;
}

void TestLayerAnimationObserver::OnDetachedFromSequence(
    LayerAnimationSequence* sequence) {
  last_detached_sequence_ = sequence;
  last_detached_sequence_epoch_ = next_epoch_++;
}

bool
TestLayerAnimationObserver::RequiresNotificationWhenAnimatorDestroyed() const {
  return requires_notification_when_animator_destroyed_;
}

testing::AssertionResult TestLayerAnimationObserver::NoEventsObserved() {
  if (!last_attached_sequence_ && !last_scheduled_sequence_ &&
      !last_started_sequence_ && !last_aborted_sequence_ &&
      !last_ended_sequence_ && !last_repetition_ended_sequence_ &&
      !last_detached_sequence_) {
    return testing::AssertionSuccess();
  } else {
    testing::AssertionResult assertion_failure = testing::AssertionFailure();
    assertion_failure << "The following events have been observed:";
    if (last_attached_sequence_) {
      assertion_failure << "\n\tlast_attached_sequence_="
                        << last_attached_sequence_;
    }
    if (last_scheduled_sequence_) {
      assertion_failure << "\n\tlast_scheduled_sequence_="
                        << last_scheduled_sequence_;
    }
    if (last_started_sequence_) {
      assertion_failure << "\n\tlast_started_sequence_="
                        << last_started_sequence_;
    }
    if (last_aborted_sequence_) {
      assertion_failure << "\n\tlast_aborted_sequence_="
                        << last_aborted_sequence_;
    }
    if (last_ended_sequence_) {
      assertion_failure << "\n\tlast_ended_sequence_" << last_ended_sequence_;
    }
    if (last_repetition_ended_sequence_) {
      assertion_failure << "\n\tlast_cycle_ended_sequence_"
                        << last_repetition_ended_sequence_;
    }
    if (last_detached_sequence_) {
      assertion_failure << "\n\tlast_detached_sequence_="
                        << last_detached_sequence_;
    }
    return assertion_failure;
  }
}

testing::AssertionResult
TestLayerAnimationObserver::AttachedEpochIsBeforeScheduledEpoch() {
  if (last_attached_sequence_epoch_ < last_scheduled_sequence_epoch_) {
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionFailure()
           << "The attached epoch=" << last_attached_sequence_epoch_
           << " is NOT before the scheduled epoch="
           << last_scheduled_sequence_epoch_;
  }
}

testing::AssertionResult
TestLayerAnimationObserver::ScheduledEpochIsBeforeStartedEpoch() {
  if (last_scheduled_sequence_epoch_ < last_started_sequence_epoch_) {
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionFailure()
           << "The scheduled epoch=" << last_scheduled_sequence_epoch_
           << " is NOT before the started epoch="
           << last_started_sequence_epoch_;
  }
}

testing::AssertionResult
TestLayerAnimationObserver::StartedEpochIsBeforeEndedEpoch() {
  if (last_started_sequence_epoch_ < last_ended_sequence_epoch_) {
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionFailure()
           << "The started epoch=" << last_started_sequence_epoch_
           << " is NOT before the ended epoch=" << last_ended_sequence_epoch_;
  }
}

testing::AssertionResult
TestLayerAnimationObserver::StartedEpochIsBeforeAbortedEpoch() {
  if (last_started_sequence_epoch_ < last_aborted_sequence_epoch_) {
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionFailure()
           << "The started epoch=" << last_started_sequence_epoch_
           << " is NOT before the aborted epoch="
           << last_aborted_sequence_epoch_;
  }
}

testing::AssertionResult
TestLayerAnimationObserver::AbortedEpochIsBeforeStartedEpoch() {
  if (last_aborted_sequence_epoch_ < last_started_sequence_epoch_) {
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionFailure()
           << "The aborted epoch=" << last_aborted_sequence_epoch_
           << " is NOT before the started epoch="
           << last_started_sequence_epoch_;
  }
}

testing::AssertionResult
TestLayerAnimationObserver::AbortedEpochIsBeforeDetachedEpoch() {
  if (last_aborted_sequence_epoch_ < last_detached_sequence_epoch_) {
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionFailure()
           << "The aborted epoch=" << last_aborted_sequence_epoch_
           << " is NOT before the detached epoch="
           << last_detached_sequence_epoch_;
  }
}

testing::AssertionResult
TestLayerAnimationObserver::EndedEpochIsBeforeStartedEpoch() {
  if (last_ended_sequence_epoch_ < last_started_sequence_epoch_) {
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionFailure()
           << "The ended epoch=" << last_ended_sequence_epoch_
           << " is NOT before the started epoch="
           << last_started_sequence_epoch_;
  }
}

testing::AssertionResult
TestLayerAnimationObserver::EndedEpochIsBeforeDetachedEpoch() {
  if (last_ended_sequence_epoch_ < last_detached_sequence_epoch_) {
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionFailure()
           << "The ended epoch=" << last_ended_sequence_epoch_
           << " is NOT before the detached epoch="
           << last_detached_sequence_epoch_;
  }
}

}  // namespace ui
