// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_LAYER_ANIMATOR_H_
#define UI_COMPOSITOR_LAYER_ANIMATOR_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "cc/animation/animation_delegate.h"
#include "cc/trees/target_property.h"
#include "ui/compositor/compositor_export.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_threaded_animation_delegate.h"
#include "ui/gfx/animation/tween.h"

namespace cc {
class Animation;
class AnimationTimeline;
class Layer;
class SingleKeyframeEffectAnimation;
}

namespace gfx {
class Animation;
class Rect;
class Transform;
}

namespace ui {
class Compositor;
class ImplicitAnimationObserver;
class Layer;
class LayerAnimationSequence;
class LayerAnimationDelegate;
class LayerAnimationObserver;
class LayerAnimatorCollection;
class ScopedLayerAnimationSettings;

// When a property of layer needs to be changed it is set by way of
// LayerAnimator. This enables LayerAnimator to animate property changes.
// NB: during many tests, set_disable_animations_for_test is used and causes
// all animations to complete immediately. The layer animation is ref counted
// so that if its owning layer is deleted (and the owning layer is only other
// class that should ever hold a ref ptr to a LayerAnimator), the animator can
// ensure that it is not disposed of until it finishes executing. It does this
// by holding a reference to itself for the duration of methods for which it
// must guarantee that |this| is valid.
class COMPOSITOR_EXPORT LayerAnimator : public base::RefCounted<LayerAnimator>,
                                        public LayerThreadedAnimationDelegate,
                                        public cc::AnimationDelegate {
 public:
  enum PreemptionStrategy {
    IMMEDIATELY_SET_NEW_TARGET,
    IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
    ENQUEUE_NEW_ANIMATION,
    REPLACE_QUEUED_ANIMATIONS
  };

  explicit LayerAnimator(base::TimeDelta transition_duration);

  // No implicit animations when properties are set.
  static LayerAnimator* CreateDefaultAnimator();

  // Implicitly animates when properties are set.
  static LayerAnimator* CreateImplicitAnimator();

  // Sets the transform on the delegate. May cause an implicit animation.
  virtual void SetTransform(const gfx::Transform& transform);
  gfx::Transform GetTargetTransform() const;

  // Sets the bounds on the delegate. May cause an implicit animation.
  virtual void SetBounds(const gfx::Rect& bounds);
  gfx::Rect GetTargetBounds() const;

  // Sets the opacity on the delegate. May cause an implicit animation.
  virtual void SetOpacity(float opacity);
  float GetTargetOpacity() const;

  // Sets the visibility of the delegate. May cause an implicit animation.
  virtual void SetVisibility(bool visibility);
  bool GetTargetVisibility() const;

  // Sets the brightness on the delegate. May cause an implicit animation.
  virtual void SetBrightness(float brightness);
  float GetTargetBrightness() const;

  // Sets the grayscale on the delegate. May cause an implicit animation.
  virtual void SetGrayscale(float grayscale);
  float GetTargetGrayscale() const;

  // Sets the color on the delegate. May cause an implicit animation.
  virtual void SetColor(SkColor color);
  SkColor GetTargetColor() const;

  // Sets the clip rect on the delegate. May cause an implicit animation.
  virtual void SetClipRect(const gfx::Rect& clip_rect);
  gfx::Rect GetTargetClipRect() const;

  // Sets the rounded corners on the delegate. May cause an implicit animation.
  virtual void SetRoundedCorners(const gfx::RoundedCornersF& rounded_corners);
  gfx::RoundedCornersF GetTargetRoundedCorners() const;

  // Returns the default length of animations, including adjustment for slow
  // animation mode if set.
  base::TimeDelta GetTransitionDuration() const;

  // Sets the layer animation delegate the animator is associated with. The
  // animator does not own the delegate. The layer animator expects a non-NULL
  // delegate for most of its operations, so do not call any methods without
  // a valid delegate installed.
  void SetDelegate(LayerAnimationDelegate* delegate);

  // Unsubscribe from |cc_layer_| and subscribe to |new_layer|.
  void SwitchToLayer(scoped_refptr<cc::Layer> new_layer);

  // Attach Animation to Layer and AnimationTimeline
  void AttachLayerAndTimeline(Compositor* compositor);
  // Detach Animation from Layer and AnimationTimeline
  void DetachLayerAndTimeline(Compositor* compositor);

  cc::SingleKeyframeEffectAnimation* GetAnimationForTesting() const;

  // Sets the animation preemption strategy. This determines the behaviour if
  // a property is set during an animation. The default is
  // IMMEDIATELY_SET_NEW_TARGET (see ImmediatelySetNewTarget below).
  void set_preemption_strategy(PreemptionStrategy strategy) {
    preemption_strategy_ = strategy;
  }

  PreemptionStrategy preemption_strategy() const {
    return preemption_strategy_;
  }

  // Start an animation sequence. If an animation for the same property is in
  // progress, it needs to be interrupted with the new animation. The animator
  // takes ownership of this animation sequence.
  void StartAnimation(LayerAnimationSequence* animation);

  // Schedule an animation to be run when possible. The animator takes ownership
  // of this animation sequence.
  void ScheduleAnimation(LayerAnimationSequence* animation);

  // Starts the animations to be run together, ensuring that the first elements
  // in these sequences have the same effective start time even when some of
  // them start on the compositor thread (but there is no such guarantee for
  // the effective start time of subsequent elements). Obviously will not work
  // if they animate any common properties. The animator takes ownership of the
  // animation sequences. Takes PreemptionStrategy into account.
  void StartTogether(const std::vector<LayerAnimationSequence*>& animations);

  // Schedules the animations to be run together, ensuring that the first
  // elements in these sequences have the same effective start time even when
  // some of them start on the compositor thread (but there is no such guarantee
  // for the effective start time of subsequent elements). Obviously will not
  // work if they animate any common properties. The animator takes ownership
  // of the animation sequences.
  void ScheduleTogether(const std::vector<LayerAnimationSequence*>& animations);

  // Schedules a pause for length |duration| of all the specified properties.
  // End the list with -1.
  void SchedulePauseForProperties(
      base::TimeDelta duration,
      LayerAnimationElement::AnimatableProperties properties_to_pause);

  // Returns true if there is an animation in the queue (animations remain in
  // the queue until they complete, so this includes running animations).
  bool is_animating() const { return !animation_queue_.empty(); }

  // Returns true if there is an animation in the queue that animates the given
  // property (animations remain in the queue until they complete, so this
  // includes running animations).
  bool IsAnimatingProperty(
      LayerAnimationElement::AnimatableProperty property) const {
    return IsAnimatingOnePropertyOf(property);
  }

  // Returns true if there is an animation in the queue that animates at least
  // one of the given property (animations remain in the queue until they
  // complete, so this includes running animations).
  bool IsAnimatingOnePropertyOf(
      LayerAnimationElement::AnimatableProperties properties) const;

  // Stops animating the given property. No effect if there is no running
  // animation for the given property. Skips to the final state of the
  // animation.
  void StopAnimatingProperty(
      LayerAnimationElement::AnimatableProperty property);

  // Stops all animation and clears any queued animations. This call progresses
  // animations to their end points and notifies all observers.
  void StopAnimating() { StopAnimatingInternal(false); }

  // This is similar to StopAnimating, but aborts rather than finishes the
  // animations and notifies all observers.
  void AbortAllAnimations() { StopAnimatingInternal(true); }

  // Adds/remove |observer| from the observer list. Observers are notified when
  // animations are scheduled, start, end or are aborted. They may also be
  // notified when they're attached/detached from a LayerAnimationSequence (see
  // LayerAnimationObserver).
  void AddObserver(LayerAnimationObserver* observer);
  void RemoveObserver(LayerAnimationObserver* observer);

  void AddOwnedObserver(
      std::unique_ptr<ImplicitAnimationObserver> animation_observer);
  void RemoveAndDestroyOwnedObserver(
      ImplicitAnimationObserver* animation_observer);

  // Called when a threaded animation is actually started.
  void OnThreadedAnimationStarted(base::TimeTicks monotonic_time,
                                  cc::TargetProperty::Type target_property,
                                  int group_id);

  // This determines how implicit animations will be tweened. This has no
  // effect on animations that are explicitly started or scheduled. The default
  // is Tween::LINEAR.
  void set_tween_type(gfx::Tween::Type tween_type) { tween_type_ = tween_type; }
  gfx::Tween::Type tween_type() const { return tween_type_; }

  // For testing purposes only.
  void set_disable_timer_for_test(bool disable_timer) {
    disable_timer_for_test_ = disable_timer;
  }

  void set_last_step_time(base::TimeTicks time) {
    last_step_time_ = time;
  }
  base::TimeTicks last_step_time() const { return last_step_time_; }

  void Step(base::TimeTicks time_now);

  void AddToCollection(LayerAnimatorCollection* collection);
  void RemoveFromCollection(LayerAnimatorCollection* collection);

 protected:
  ~LayerAnimator() override;

  LayerAnimationDelegate* delegate() { return delegate_; }
  const LayerAnimationDelegate* delegate() const { return delegate_; }

  // Virtual for testing.
  virtual void ProgressAnimation(LayerAnimationSequence* sequence,
                                 base::TimeTicks now);

  void ProgressAnimationToEnd(LayerAnimationSequence* sequence);

  // Returns true if the sequence is owned by this animator.
  bool HasAnimation(LayerAnimationSequence* sequence) const;

 private:
  friend class base::RefCounted<LayerAnimator>;
  friend class ScopedLayerAnimationSettings;
  friend class LayerAnimatorTestController;
  FRIEND_TEST_ALL_PREFIXES(LayerAnimatorTest, AnimatorStartedCorrectly);
  FRIEND_TEST_ALL_PREFIXES(LayerAnimatorTest,
                           AnimatorRemovedFromCollectionWhenLayerIsDestroyed);

  class RunningAnimation {
   public:
    RunningAnimation(const base::WeakPtr<LayerAnimationSequence>& sequence);
    RunningAnimation(const RunningAnimation& other);
    ~RunningAnimation();

    bool is_sequence_alive() const { return !!sequence_.get(); }
    LayerAnimationSequence* sequence() const { return sequence_.get(); }

   private:
    base::WeakPtr<LayerAnimationSequence> sequence_;

    // Copy and assign are allowed.
  };

  using RunningAnimations = std::vector<RunningAnimation>;
  using AnimationQueue =
      base::circular_deque<std::unique_ptr<LayerAnimationSequence>>;

  // Finishes all animations by either advancing them to their final state or by
  // aborting them.
  void StopAnimatingInternal(bool abort);

  // Starts or stops stepping depending on whether thare are running animations.
  void UpdateAnimationState();

  // Removes the sequences from both the running animations and the queue.
  // Returns a pointer to the removed animation, if any. NOTE: the caller is
  // responsible for deleting the returned pointer.
  LayerAnimationSequence* RemoveAnimation(
      LayerAnimationSequence* sequence) WARN_UNUSED_RESULT;

  // Progresses to the end of the sequence before removing it.
  void FinishAnimation(LayerAnimationSequence* sequence, bool abort);

  // Finishes any running animation with zero duration.
  void FinishAnyAnimationWithZeroDuration();

  // Clears the running animations and the queue. No sequences are progressed.
  void ClearAnimations();

  // Returns the running animation animating the given property, if any.
  RunningAnimation* GetRunningAnimation(
      LayerAnimationElement::AnimatableProperty property);

  // Checks if the sequence has already been added to the queue and adds it
  // to the front if note.
  void AddToQueueIfNotPresent(LayerAnimationSequence* sequence);

  // Any running or queued animation that affects a property in common with
  // |sequence| is either finished or aborted depending on |abort|.
  void RemoveAllAnimationsWithACommonProperty(LayerAnimationSequence* sequence,
                                              bool abort);

  // Preempts a running animation by progressing both the running animation and
  // the given sequence to the end.
  void ImmediatelySetNewTarget(LayerAnimationSequence* sequence);

  // Preempts by aborting the running animation, and starts the given animation.
  void ImmediatelyAnimateToNewTarget(LayerAnimationSequence* sequence);

  // Preempts by adding the new animation to the queue.
  void EnqueueNewAnimation(LayerAnimationSequence* sequence);

  // Preempts by wiping out any unstarted animation in the queue and then
  // enqueuing this animation.
  void ReplaceQueuedAnimations(LayerAnimationSequence* sequence);

  // If there's an animation in the queue that doesn't animate the same property
  // as a running animation, or an animation schedule to run before it, start it
  // up. Repeat until there are no such animations.
  void ProcessQueue();

  // Attempts to add the sequence to the list of running animations. Returns
  // false if there is an animation running that already animates one of the
  // properties affected by |sequence|.
  bool StartSequenceImmediately(LayerAnimationSequence* sequence);

  // Sets the value of target as if all the running and queued animations were
  // allowed to finish.
  void GetTargetValue(LayerAnimationElement::TargetValue* target) const;

  // Called whenever an animation is added to the animation queue. Either by
  // starting the animation or adding to the queue.
  void OnScheduled(LayerAnimationSequence* sequence);

  // Sets |transition_duration_| unless |is_transition_duration_locked_| is set.
  void SetTransitionDuration(base::TimeDelta duration);

  // Clears the animation queues and notifies any running animations that they
  // have been aborted.
  void ClearAnimationsInternal();

  // Cleans up any running animations that may have been deleted.
  void PurgeDeletedAnimations();

  LayerAnimatorCollection* GetLayerAnimatorCollection();

  // cc::AnimationDelegate implementation.
  void NotifyAnimationStarted(base::TimeTicks monotonic_time,
                              int target_property,
                              int group_id) override;
  void NotifyAnimationFinished(base::TimeTicks monotonic_time,
                               int target_property,
                               int group_id) override {}
  void NotifyAnimationAborted(base::TimeTicks monotonic_time,
                              int target_property,
                              int group_id) override {}
  void NotifyAnimationTakeover(
      base::TimeTicks monotonic_time,
      int target_property,
      base::TimeTicks animation_start_time,
      std::unique_ptr<cc::AnimationCurve> curve) override {}
  void NotifyLocalTimeUpdated(
      base::Optional<base::TimeDelta> local_time) override {}

  // Implementation of LayerThreadedAnimationDelegate.
  void AddThreadedAnimation(
      std::unique_ptr<cc::KeyframeModel> keyframe_model) override;
  void RemoveThreadedAnimation(int keyframe_model_id) override;

  void AttachLayerToAnimation(int layer_id);
  void DetachLayerFromAnimation();

  void set_animation_metrics_reporter(AnimationMetricsReporter* reporter) {
    animation_metrics_reporter_ = reporter;
  }

  // This is the queue of animations to run.
  AnimationQueue animation_queue_;

  // The target of all layer animations.
  LayerAnimationDelegate* delegate_;

  // Plays CC animations.
  scoped_refptr<cc::SingleKeyframeEffectAnimation> animation_;

  // The currently running animations.
  RunningAnimations running_animations_;

  // Determines how animations are replaced.
  PreemptionStrategy preemption_strategy_;

  // Whether the length of animations is locked. While it is locked
  // SetTransitionDuration does not set |transition_duration_|.
  bool is_transition_duration_locked_;

  // The default length of animations.
  base::TimeDelta transition_duration_;

  // The default tween type for implicit transitions
  gfx::Tween::Type tween_type_;

  // Used for coordinating the starting of animations.
  base::TimeTicks last_step_time_;

  // True if we are being stepped by our container.
  bool is_started_;

  // This prevents the animator from automatically stepping through animations
  // and allows for manual stepping.
  bool disable_timer_for_test_;

  // Prevents timer adjustments in case when we start multiple animations
  // with preemption strategies that discard previous animations.
  bool adding_animations_;

  // Helper to output UMA performance metrics.
  AnimationMetricsReporter* animation_metrics_reporter_;

  // Observers are notified when layer animations end, are scheduled or are
  // aborted.
  base::ObserverList<LayerAnimationObserver>::Unchecked observers_;

  std::vector<std::unique_ptr<ImplicitAnimationObserver>> owned_observer_list_;

  DISALLOW_COPY_AND_ASSIGN(LayerAnimator);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_LAYER_ANIMATOR_H_
