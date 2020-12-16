// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/animation_throughput_reporter.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observation.h"
#include "cc/animation/animation.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_delegate.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_observer.h"
#include "ui/compositor/throughput_tracker.h"

namespace ui {

// AnimationTracker tracks the layer animations that are created during the
// lifetime of its owner AnimationThroughputReporter.
//
// Lifetime of this tracker class is a bit complicated. If there are animations
// to track (i.e. HasAnimationsToTrack() returns true) when the owner reporter
// is going away, it needs to have the same lifetime of the animations to track
// the performance. In such case, the owner reporter would drop the ownership
// and set set_should_delete() to let the tracker manages its own lifetime
// based on LayerDetroyed and LayerAnimationObserver signals. On the other hand,
// if there are no animations to track, the tracker is released with its owner
// reporter.
class AnimationThroughputReporter::AnimationTracker
    : public CallbackLayerAnimationObserver,
      public LayerObserver {
 public:
  AnimationTracker(Layer* layer, ReportCallback report_callback)
      : CallbackLayerAnimationObserver(
            base::BindRepeating(&AnimationTracker::OnAnimationEnded,
                                base::Unretained(this))),
        animator_(layer->GetAnimator()),
        report_callback_(std::move(report_callback)) {
    DCHECK(report_callback_);
    layer_observation_.Observe(layer);
  }

  AnimationTracker(const AnimationTracker& other) = delete;
  AnimationTracker& operator=(const AnimationTracker& other) = delete;

  ~AnimationTracker() override {
    // No auto delete in the observer callbacks since `this` is being
    // destructed.
    should_delete_ = false;

    // Cancels existing tracking if any.
    throughput_tracker_.reset();

    // Stops observing animations so that `animator` destruction does not call
    // back into half destructed `this` if `this` holds the last reference of
    // `animator_`.
    StopObserving();
  }

  // Whether there are/will be animations to track. That is, there is an
  // underlying layer and there are attached animation sequences.
  bool HasAnimationsToTrack() const {
    return layer_observation_.IsObserving() && !attached_sequences().empty();
  }

  void set_should_delete(bool should_delete) { should_delete_ = should_delete; }

 private:
  // CallbackLayerAnimationObserver:
  void OnAnimatorAttachedToTimeline() override { MaybeStartTracking(); }
  void OnAnimatorDetachedFromTimeline() override {
    // Gives up tracking when detached from the timeline.
    first_animation_group_id_.reset();
    if (throughput_tracker_)
      throughput_tracker_.reset();

    // OnAnimationEnded would not happen after detached from the timeline.
    // So do the clean up here.
    if (should_delete_)
      delete this;
  }
  void OnLayerAnimationStarted(LayerAnimationSequence* sequence) override {
    CallbackLayerAnimationObserver::OnLayerAnimationStarted(sequence);

    if (!first_animation_group_id_.has_value()) {
      first_animation_group_id_ = sequence->animation_group_id();
      MaybeStartTracking();
    }

    // Make sure SetActive() is called so that OnAnimationEnded callback will be
    // invoked when all attached layer animation sequences finish.
    if (!active())
      SetActive();
  }
  void OnLayerAnimationAborted(LayerAnimationSequence* sequence) override {
    // Check whether the aborted animation sequence is among the relevant ones
    // (started while the tracker is alive). This is done by checking the
    // animation_group_id() and assuming the id is monotonic increasing.
    if (first_animation_group_id_.has_value() &&
        first_animation_group_id_.value() <= sequence->animation_group_id()) {
      started_animations_aborted_ = true;
    }

    // Note the following call could delete |this|.
    CallbackLayerAnimationObserver::OnLayerAnimationAborted(sequence);
  }

  // LayerObserver:
  void LayerDestroyed(Layer* layer) override {
    DCHECK(layer_observation_.IsObservingSource(layer));

    layer_observation_.Reset();

    // No more tracking needed when underlying layer is gone.
    if (should_delete_)
      delete this;
  }

  void MaybeStartTracking() {
    // No tracking if no layer animation sequence is started.
    if (!first_animation_group_id_.has_value())
      return;

    // No tracking if |animator_| is not attached to a timeline. Layer animation
    // sequence would not tick without a timeline.
    if (!AnimationThroughputReporter::IsAnimatorAttachedToTimeline(
            animator_.get())) {
      return;
    }

    ui::Compositor* compositor =
        AnimationThroughputReporter::GetCompositor(animator_.get());
    throughput_tracker_ = compositor->RequestNewThroughputTracker();
    throughput_tracker_->Start(report_callback_);
  }

  // Invoked when all animation sequences finish.
  bool OnAnimationEnded(const CallbackLayerAnimationObserver& self) {
    // |throughput_tracker| could reset when detached from animation timeline.
    if (throughput_tracker_) {
      if (started_animations_aborted_)
        throughput_tracker_->Cancel();
      else
        throughput_tracker_->Stop();
    }

    first_animation_group_id_.reset();
    started_animations_aborted_ = false;
    return should_delete_;
  }

  // Whether this class should delete itself on animation ended.
  bool should_delete_ = false;

  base::ScopedObservation<Layer, LayerObserver> layer_observation_{this};
  scoped_refptr<LayerAnimator> animator_;

  base::Optional<ThroughputTracker> throughput_tracker_;

  base::Optional<int> first_animation_group_id_;
  bool started_animations_aborted_ = false;

  AnimationThroughputReporter::ReportCallback report_callback_;
};

AnimationThroughputReporter::AnimationThroughputReporter(
    LayerAnimator* animator,
    ReportCallback report_callback)
    : animator_(animator),
      animation_tracker_(
          std::make_unique<AnimationTracker>(animator_->delegate()->GetLayer(),
                                             std::move(report_callback))) {
  animator_->AddObserver(animation_tracker_.get());
}

AnimationThroughputReporter::~AnimationThroughputReporter() {
  // Directly remove |animation_tracker_| from |LayerAnimator::observers_|
  // rather than calling LayerAnimator::RemoveObserver(), to avoid removing it
  // from the scheduled animation sequences.
  animator_->observers_.RemoveObserver(animation_tracker_.get());

  // |animation_tracker_| deletes itself when its tracked animations finish.
  if (animation_tracker_->HasAnimationsToTrack())
    animation_tracker_.release()->set_should_delete(true);
}

// static
Compositor* AnimationThroughputReporter::GetCompositor(
    LayerAnimator* animator) {
  return animator->delegate()->GetLayer()->GetCompositor();
}

// static
bool AnimationThroughputReporter::IsAnimatorAttachedToTimeline(
    LayerAnimator* animator) {
  return animator->animation_->animation_timeline();
}

}  // namespace ui
