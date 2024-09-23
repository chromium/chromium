// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/animation_throughput_reporter.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "cc/animation/animation.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_delegate.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
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
// based on LayerAnimationObserver signals. On the other hand, if there are no
// animations to track, the tracker is released with its owner reporter.
class AnimationThroughputReporter::AnimationTracker
    : public CallbackLayerAnimationObserver {
 public:
  AnimationTracker(LayerAnimator* animator, ReportCallback report_callback)
      : CallbackLayerAnimationObserver(
            base::BindRepeating(&AnimationTracker::OnAnimationEnded,
                                base::Unretained(this))),
        animator_(animator),
        report_callback_(std::move(report_callback)) {
    DCHECK(report_callback_);
  }

  AnimationTracker(const AnimationTracker& other) = delete;
  AnimationTracker& operator=(const AnimationTracker& other) = delete;

  ~AnimationTracker() override = default;

  // Whether there are/will be animations to track and the track is actively
  // tracking them.
  bool HasAnimationsToTrack() const {
    return active() && !attached_sequences().empty();
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

    // Start tracking on the first animation. Do not start tracking if the
    // animation is finished when started, which happens when an animation
    // is created either with 0 duration, or in its final state.
    if (!first_animation_group_id_.has_value() &&
        !sequence->IsFinished(base::TimeTicks::Now())) {
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

  void MaybeStartTracking() {
    // No tracking if no layer animation sequence is started.
    if (!first_animation_group_id_.has_value())
      return;

    // No tracking if |animator_| is not attached to a timeline. Layer animation
    // sequence would not tick without a timeline.
    if (!AnimationThroughputReporter::IsAnimatorAttachedToTimeline(animator_)) {
      return;
    }

    ui::Compositor* compositor =
        AnimationThroughputReporter::GetCompositor(animator_);
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

      // `OnAnimationEnded` could be called multiple times when scheduling
      // animations. Destroy the tracker so that it is not stopped/canceled
      // more than once. New tracker will be created when new animation sequence
      // is started.
      throughput_tracker_.reset();
    }

    first_animation_group_id_.reset();
    started_animations_aborted_ = false;
    return should_delete_;
  }

  // Whether this class should delete itself on animation ended.
  bool should_delete_ = false;

  const raw_ptr<LayerAnimator, DanglingUntriaged> animator_;

  std::optional<ThroughputTracker> throughput_tracker_;

  std::optional<int> first_animation_group_id_;
  bool started_animations_aborted_ = false;

  AnimationThroughputReporter::ReportCallback report_callback_;
};

AnimationThroughputReporter::AnimationThroughputReporter(
    scoped_refptr<LayerAnimator> animator,
    ReportCallback report_callback)
    : animator_(std::move(animator)),
      animation_tracker_(
          std::make_unique<AnimationTracker>(animator_.get(),
                                             std::move(report_callback))) {
  animator_->AddObserver(animation_tracker_.get());
}

AnimationThroughputReporter::~AnimationThroughputReporter() {
  // Directly remove |animation_tracker_| from |LayerAnimator::observers_|
  // rather than calling LayerAnimator::RemoveObserver(), to avoid removing it
  // from the scheduled animation sequences.
  animator_->observers_.RemoveObserver(animation_tracker_.get());

  // Drop the animator reference. If this is the last reference, the animator
  // will be destroyed. When the animator destruction happens, it destroys its
  // LayerAnimationSequences and detach observers from them. As a result,
  // AnimationTracker::OnAnimationEnded would be called after all animation
  // sequences are detached. After this, animator will no longer be accessed
  // by AnimationTracker and HasAnimationsToTrack() would correctly report
  // that there are no animations to track.
  animator_.reset();

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
