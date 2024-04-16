// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_animator.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/check_op.h"
#include "base/observer_list.h"
#include "base/trace_event/trace_event.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/element_animations.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_delegate.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator_collection.h"

#define SAFE_INVOKE_VOID(function, running_anim, ...) \
    if (running_anim.is_sequence_alive()) \
      function(running_anim.sequence(), ##__VA_ARGS__)
#define SAFE_INVOKE_BOOL(function, running_anim) \
    ((running_anim.is_sequence_alive()) \
        ? function(running_anim.sequence()) \
        : false)
#define SAFE_INVOKE_PTR(function, running_anim)                           \
  ((running_anim.is_sequence_alive()) ? function(running_anim.sequence()) \
                                      : nullptr)

namespace ui {

namespace {

const int kLayerAnimatorDefaultTransitionDurationMs = 120;

}  // namespace

// LayerAnimator public --------------------------------------------------------

LayerAnimator::LayerAnimator(base::TimeDelta transition_duration)
    : transition_duration_(transition_duration) {
  animation_ =
      cc::Animation::Create(cc::AnimationIdProvider::NextAnimationId());
}

LayerAnimator::~LayerAnimator() {
  for (size_t i = 0; i < running_animations_.size(); ++i) {
    if (running_animations_[i].is_sequence_alive())
      running_animations_[i].sequence()->OnAnimatorDestroyed();
  }
  ClearAnimationsInternal();
  delegate_ = nullptr;
  DCHECK(!animation_->animation_timeline());
}

// static
scoped_refptr<LayerAnimator> LayerAnimator::CreateDefaultAnimator() {
  return base::MakeRefCounted<LayerAnimator>(base::Milliseconds(0));
}

// static
scoped_refptr<LayerAnimator> LayerAnimator::CreateImplicitAnimator() {
  return base::MakeRefCounted<LayerAnimator>(
      base::Milliseconds(kLayerAnimatorDefaultTransitionDurationMs));
}

// This macro provides the implementation for the setter and getter (well,
// the getter of the target value) for an animated property. For example,
// it is used for the implementations of SetTransform and GetTargetTransform.
// It is worth noting that SetFoo avoids invoking the usual animation machinery
// if the transition duration is zero -- in this case we just set the property
// on the layer animation delegate immediately.
#define ANIMATED_PROPERTY(type, property, name, member_type, member)   \
  void LayerAnimator::Set##name(type value) {                          \
    base::TimeDelta duration = GetTransitionDuration();                \
    if (duration.is_zero() && delegate() &&                            \
        (preemption_strategy_ != ENQUEUE_NEW_ANIMATION)) {             \
      /* Stopping an animation may result in destruction of `this`. */ \
      const auto weak_ptr = weak_ptr_factory_.GetWeakPtr();            \
      StopAnimatingProperty(LayerAnimationElement::property);          \
      if (!weak_ptr || !delegate())                                    \
        return;                                                        \
      delegate()->Set##name##FromAnimation(                            \
          value, PropertyChangeReason::NOT_FROM_ANIMATION);            \
      return;                                                          \
    }                                                                  \
    std::unique_ptr<LayerAnimationElement> element =                   \
        LayerAnimationElement::Create##name##Element(value, duration); \
    element->set_tween_type(tween_type_);                              \
    StartAnimation(new LayerAnimationSequence(std::move(element)));    \
  }                                                                    \
                                                                       \
  member_type LayerAnimator::GetTarget##name() const {                 \
    LayerAnimationElement::TargetValue target(delegate());             \
    GetTargetValue(&target);                                           \
    return target.member;                                              \
  }

ANIMATED_PROPERTY(const gfx::Transform&,
                  TRANSFORM,
                  Transform,
                  gfx::Transform,
                  transform)
ANIMATED_PROPERTY(const gfx::Rect&, BOUNDS, Bounds, gfx::Rect, bounds)
ANIMATED_PROPERTY(float, OPACITY, Opacity, float, opacity)
ANIMATED_PROPERTY(bool, VISIBILITY, Visibility, bool, visibility)
ANIMATED_PROPERTY(float, BRIGHTNESS, Brightness, float, brightness)
ANIMATED_PROPERTY(float, GRAYSCALE, Grayscale, float, grayscale)
ANIMATED_PROPERTY(SkColor, COLOR, Color, SkColor, color)
ANIMATED_PROPERTY(const gfx::Rect&, CLIP, ClipRect, gfx::Rect, clip_rect)
ANIMATED_PROPERTY(const gfx::RoundedCornersF&,
                  ROUNDED_CORNERS,
                  RoundedCorners,
                  gfx::RoundedCornersF,
                  rounded_corners)
ANIMATED_PROPERTY(const gfx::LinearGradient&,
                  GRADIENT_MASK,
                  GradientMask,
                  gfx::LinearGradient,
                  gradient_mask)

#undef ANIMATED_PROPERTY

base::TimeDelta LayerAnimator::GetTransitionDuration() const {
  return transition_duration_;
}

void LayerAnimator::SetDelegate(LayerAnimationDelegate* delegate) {
  if (delegate_ && is_started_) {
    LayerAnimatorCollection* collection = GetLayerAnimatorCollection();
    if (collection)
      collection->StopAnimator(this);
  }
  SwitchToLayer(delegate ? delegate->GetCcLayer() : nullptr);
  delegate_ = delegate;
  if (delegate_ && is_started_) {
    LayerAnimatorCollection* collection = GetLayerAnimatorCollection();
    if (collection)
      collection->StartAnimator(this);
  }
}

void LayerAnimator::SwitchToLayer(scoped_refptr<cc::Layer> new_layer) {
  if (delegate_)
    DetachLayerFromAnimation();
  if (new_layer)
    AttachLayerToAnimation(new_layer->id());
}

void LayerAnimator::AttachLayerAndTimeline(Compositor* compositor) {
  DCHECK(compositor);

  cc::AnimationTimeline* timeline = compositor->GetAnimationTimeline();
  DCHECK(timeline);
  timeline->AttachAnimation(animation_);

  DCHECK(delegate_->GetCcLayer());
  AttachLayerToAnimation(delegate_->GetCcLayer()->id());

  for (auto& layer_animation_sequence : animation_queue_)
    layer_animation_sequence->OnAnimatorAttached(delegate());
}

void LayerAnimator::DetachLayerAndTimeline(Compositor* compositor) {
  DCHECK(compositor);

  cc::AnimationTimeline* timeline = compositor->GetAnimationTimeline();
  DCHECK(timeline);

  DetachLayerFromAnimation();
  timeline->DetachAnimation(animation_);

  for (auto& layer_animation_sequence : animation_queue_)
    layer_animation_sequence->OnAnimatorDetached();
}

void LayerAnimator::AttachLayerToAnimation(int layer_id) {
  // For ui, layer and element ids are equivalent.
  cc::ElementId element_id(layer_id);
  if (!animation_->element_id())
    animation_->AttachElement(element_id);
  else
    DCHECK_EQ(animation_->element_id(), element_id);

  animation_->set_animation_delegate(this);
}

void LayerAnimator::DetachLayerFromAnimation() {
  animation_->set_animation_delegate(nullptr);

  if (animation_->element_id())
    animation_->DetachElement();
}

void LayerAnimator::AddThreadedAnimation(
    std::unique_ptr<cc::KeyframeModel> animation) {
  animation_->AddKeyframeModel(std::move(animation));
}

void LayerAnimator::RemoveThreadedAnimation(int keyframe_model_id) {
  animation_->RemoveKeyframeModel(keyframe_model_id);
}

cc::Animation* LayerAnimator::GetAnimationForTesting() const {
  return animation_.get();
}

void LayerAnimator::StartAnimation(LayerAnimationSequence* animation) {
  scoped_refptr<LayerAnimator> retain(this);
  OnScheduled(animation);
  if (!StartSequenceImmediately(animation)) {
    // Attempt to preempt a running animation.
    switch (preemption_strategy_) {
      case IMMEDIATELY_SET_NEW_TARGET:
        ImmediatelySetNewTarget(animation);
        break;
      case IMMEDIATELY_ANIMATE_TO_NEW_TARGET:
        ImmediatelyAnimateToNewTarget(animation);
        break;
      case ENQUEUE_NEW_ANIMATION:
        EnqueueNewAnimation(animation);
        break;
      case REPLACE_QUEUED_ANIMATIONS:
        ReplaceQueuedAnimations(animation);
        break;
    }
  }
  FinishAnyAnimationWithZeroDuration();
  UpdateAnimationState();
}

void LayerAnimator::ScheduleAnimation(LayerAnimationSequence* animation) {
  scoped_refptr<LayerAnimator> retain(this);
  OnScheduled(animation);
  if (is_animating()) {
    animation_queue_.push_back(
        std::unique_ptr<LayerAnimationSequence>(animation));
    ProcessQueue();
  } else {
    StartSequenceImmediately(animation);
  }
  UpdateAnimationState();
}

void LayerAnimator::StartTogether(
    const std::vector<LayerAnimationSequence*>& animations) {
  scoped_refptr<LayerAnimator> retain(this);
  if (preemption_strategy_ == IMMEDIATELY_SET_NEW_TARGET) {
    std::vector<LayerAnimationSequence*>::const_iterator iter;
    for (iter = animations.begin(); iter != animations.end(); ++iter) {
      StartAnimation(*iter);
    }
    return;
  }

  adding_animations_ = true;
  if (!is_animating()) {
    LayerAnimatorCollection* collection = GetLayerAnimatorCollection();
    if (collection && collection->HasActiveAnimators())
      last_step_time_ = collection->last_tick_time();
    else
      last_step_time_ = base::TimeTicks::Now();
  }

  // Collect all the affected properties.
  LayerAnimationElement::AnimatableProperties animated_properties =
      LayerAnimationElement::UNKNOWN;

  std::vector<LayerAnimationSequence*>::const_iterator iter;
  for (iter = animations.begin(); iter != animations.end(); ++iter)
    animated_properties |= (*iter)->properties();

  // Starting a zero duration pause that affects all the animated properties
  // will prevent any of the sequences from animating until there are no
  // running animations that affect any of these properties, as well as
  // handle preemption strategy.
  StartAnimation(new LayerAnimationSequence(
      LayerAnimationElement::CreatePauseElement(animated_properties,
                                                base::TimeDelta())));

  bool wait_for_group_start = false;
  for (iter = animations.begin(); iter != animations.end(); ++iter)
    wait_for_group_start |=
        delegate_ && (*iter)->IsFirstElementThreaded(delegate_);
  int group_id = cc::AnimationIdProvider::NextGroupId();

  // These animations (provided they don't animate any common properties) will
  // now animate together if trivially scheduled.
  for (iter = animations.begin(); iter != animations.end(); ++iter) {
    (*iter)->set_animation_group_id(group_id);
    (*iter)->set_waiting_for_group_start(wait_for_group_start);
    ScheduleAnimation(*iter);
  }

  adding_animations_ = false;
  UpdateAnimationState();
}


void LayerAnimator::ScheduleTogether(
    const std::vector<LayerAnimationSequence*>& animations) {
  scoped_refptr<LayerAnimator> retain(this);

  // Collect all the affected properties.
  LayerAnimationElement::AnimatableProperties animated_properties =
      LayerAnimationElement::UNKNOWN;

  std::vector<LayerAnimationSequence*>::const_iterator iter;
  for (iter = animations.begin(); iter != animations.end(); ++iter)
    animated_properties |= (*iter)->properties();

  // Scheduling a zero duration pause that affects all the animated properties
  // will prevent any of the sequences from animating until there are no
  // running animations that affect any of these properties.
  ScheduleAnimation(new LayerAnimationSequence(
      LayerAnimationElement::CreatePauseElement(animated_properties,
                                                base::TimeDelta())));

  bool wait_for_group_start = false;
  for (iter = animations.begin(); iter != animations.end(); ++iter)
    wait_for_group_start |=
        delegate_ && (*iter)->IsFirstElementThreaded(delegate_);

  int group_id = cc::AnimationIdProvider::NextGroupId();

  // These animations (provided they don't animate any common properties) will
  // now animate together if trivially scheduled.
  for (iter = animations.begin(); iter != animations.end(); ++iter) {
    (*iter)->set_animation_group_id(group_id);
    (*iter)->set_waiting_for_group_start(wait_for_group_start);
    ScheduleAnimation(*iter);
  }

  UpdateAnimationState();
}

void LayerAnimator::SchedulePauseForProperties(
    base::TimeDelta duration,
    LayerAnimationElement::AnimatableProperties properties_to_pause) {
  ScheduleAnimation(new ui::LayerAnimationSequence(
                        ui::LayerAnimationElement::CreatePauseElement(
                            properties_to_pause, duration)));
}

bool LayerAnimator::IsAnimatingOnePropertyOf(
    LayerAnimationElement::AnimatableProperties properties) const {
  for (auto& layer_animation_sequence : animation_queue_) {
    if (layer_animation_sequence->properties() & properties)
      return true;
  }
  return false;
}

void LayerAnimator::StopAnimatingProperty(
    LayerAnimationElement::AnimatableProperty property) {
  scoped_refptr<LayerAnimator> retain(this);
  while (true) {
    // GetRunningAnimation purges deleted animations before searching, so we are
    // guaranteed to find a live animation if any is returned at all.
    RunningAnimation* running = GetRunningAnimation(property);
    if (!running)
      break;
    // As was mentioned above, this sequence must be alive.
    DCHECK(running->is_sequence_alive());
    FinishAnimation(running->sequence(), false);
  }
}

void LayerAnimator::AddObserver(LayerAnimationObserver* observer) {
  if (!observers_.HasObserver(observer)) {
    observers_.AddObserver(observer);
    for (auto& layer_animation_sequence : animation_queue_)
      layer_animation_sequence->AddObserver(observer);
  }
}

void LayerAnimator::RemoveObserver(LayerAnimationObserver* observer) {
  observers_.RemoveObserver(observer);
  // Remove the observer from all sequences as well.
  for (AnimationQueue::iterator queue_iter = animation_queue_.begin();
       queue_iter != animation_queue_.end(); ++queue_iter) {
    (*queue_iter)->RemoveObserver(observer);
  }
}

void LayerAnimator::AddOwnedObserver(
    std::unique_ptr<ImplicitAnimationObserver> animation_observer) {
  owned_observer_list_.push_back(std::move(animation_observer));
}

void LayerAnimator::RemoveAndDestroyOwnedObserver(
    ImplicitAnimationObserver* animation_observer) {
  std::erase_if(owned_observer_list_,
                [animation_observer](
                    const std::unique_ptr<ImplicitAnimationObserver>& other) {
                  return other.get() == animation_observer;
                });
}

base::CallbackListSubscription LayerAnimator::AddSequenceScheduledCallback(
    SequenceScheduledCallback callback) {
  return sequence_scheduled_callbacks_.Add(std::move(callback));
}

void LayerAnimator::OnThreadedAnimationStarted(
    base::TimeTicks monotonic_time,
    cc::TargetProperty::Type target_property,
    int group_id) {
  LayerAnimationElement::AnimatableProperty property =
      LayerAnimationElement::ToAnimatableProperty(target_property);

  RunningAnimation* running = GetRunningAnimation(property);
  if (!running)
    return;
  DCHECK(running->is_sequence_alive());

  if (running->sequence()->animation_group_id() != group_id)
    return;

  running->sequence()->OnThreadedAnimationStarted(monotonic_time,
                                                  target_property, group_id);
  if (!running->sequence()->waiting_for_group_start())
    return;

  base::TimeTicks start_time = monotonic_time;

  running->sequence()->set_waiting_for_group_start(false);

  // The call to GetRunningAnimation made above already purged deleted
  // animations, so we are guaranteed that all the animations we iterate
  // over now are alive.
  for (auto iter = running_animations_.begin();
       iter != running_animations_.end(); ++iter) {
    // Ensure that each sequence is only Started once, regardless of the
    // number of sequences in the group that have threaded first elements.
    if (((*iter).sequence()->animation_group_id() == group_id) &&
        !(*iter).sequence()->IsFirstElementThreaded(delegate_) &&
        (*iter).sequence()->waiting_for_group_start()) {
      (*iter).sequence()->set_start_time(start_time);
      (*iter).sequence()->set_waiting_for_group_start(false);
      (*iter).sequence()->Start(delegate());
    }
  }
}

void LayerAnimator::AddToCollection(LayerAnimatorCollection* collection) {
  DCHECK_EQ(collection, GetLayerAnimatorCollection());
  if (is_animating() && !is_started_) {
    collection->StartAnimator(this);
    is_started_ = true;
  }
}

void LayerAnimator::RemoveFromCollection(LayerAnimatorCollection* collection) {
  DCHECK_EQ(collection, GetLayerAnimatorCollection());
  if (is_started_) {
    collection->StopAnimator(this);
    is_started_ = false;
  }
  DCHECK(!animation_->element_animations() ||
         !animation_->element_animations()->HasTickingKeyframeEffect());
}

// LayerAnimator protected -----------------------------------------------------

void LayerAnimator::ProgressAnimation(LayerAnimationSequence* sequence,
                                      base::TimeTicks now) {
  if (!delegate() || sequence->waiting_for_group_start())
    return;

  sequence->Progress(now, delegate());
}

void LayerAnimator::ProgressAnimationToEnd(LayerAnimationSequence* sequence) {
  if (!delegate())
    return;

  sequence->ProgressToEnd(delegate());
}

bool LayerAnimator::HasAnimation(LayerAnimationSequence* sequence) const {
  for (AnimationQueue::const_iterator queue_iter = animation_queue_.begin();
       queue_iter != animation_queue_.end(); ++queue_iter) {
    if ((*queue_iter).get() == sequence)
      return true;
  }
  return false;
}

// LayerAnimator private -------------------------------------------------------

void LayerAnimator::Step(base::TimeTicks now) {
  TRACE_EVENT0("ui", "LayerAnimator::Step");
  scoped_refptr<LayerAnimator> retain(this);

  last_step_time_ = now;

  PurgeDeletedAnimations();

  // We need to make a copy of the running animations because progressing them
  // and finishing them may indirectly affect the collection of running
  // animations.
  RunningAnimations running_animations_copy = running_animations_;
  for (size_t i = 0; i < running_animations_copy.size(); ++i) {
    if (!SAFE_INVOKE_BOOL(HasAnimation, running_animations_copy[i]))
      continue;

    if (running_animations_copy[i].sequence()->IsFinished(now)) {
      SAFE_INVOKE_VOID(FinishAnimation, running_animations_copy[i], false);
    } else {
      SAFE_INVOKE_VOID(ProgressAnimation, running_animations_copy[i], now);
    }
  }
}

void LayerAnimator::StopAnimatingInternal(bool abort) {
  scoped_refptr<LayerAnimator> retain(this);
  while (is_animating() && delegate()) {
    // We're going to attempt to finish the first running animation. Let's
    // ensure that it's valid.
    PurgeDeletedAnimations();

    // If we've purged all running animations, attempt to start one up.
    if (running_animations_.empty())
      ProcessQueue();

    DCHECK(!running_animations_.empty());

    // Still no luck, let's just bail and clear all animations.
    if (running_animations_.empty()) {
      ClearAnimationsInternal();
      break;
    }

    SAFE_INVOKE_VOID(FinishAnimation, running_animations_[0], abort);
  }
}

void LayerAnimator::UpdateAnimationState() {
  if (disable_timer_for_test_)
    return;

  const bool should_start = is_animating();
  LayerAnimatorCollection* collection = GetLayerAnimatorCollection();
  if (collection) {
    if (should_start && !is_started_)
      collection->StartAnimator(this);
    else if (!should_start && is_started_)
      collection->StopAnimator(this);
    is_started_ = should_start;
  } else {
    is_started_ = false;
  }
}

LayerAnimationSequence* LayerAnimator::RemoveAnimation(
    LayerAnimationSequence* sequence) {
  std::unique_ptr<LayerAnimationSequence> to_return;

  bool is_running = false;

  // First remove from running animations
  for (auto iter = running_animations_.begin();
       iter != running_animations_.end(); ++iter) {
    if ((*iter).sequence() == sequence) {
      running_animations_.erase(iter);
      is_running = true;
      break;
    }
  }

  // Then remove from the queue
  for (AnimationQueue::iterator queue_iter = animation_queue_.begin();
       queue_iter != animation_queue_.end(); ++queue_iter) {
    if (queue_iter->get() == sequence) {
      to_return = std::move(*queue_iter);
      animation_queue_.erase(queue_iter);
      break;
    }
  }

  // Do not continue and attempt to start other sequences if the delegate is
  // nullptr.
  // TODO(crbug.com/40790139): Guard other uses of delegate_ in this class.
  if (!delegate())
    return to_return.release();

  if (!to_return.get() || !to_return->waiting_for_group_start() ||
      !to_return->IsFirstElementThreaded(delegate_))
    return to_return.release();

  // The removed sequence may have been responsible for making other sequences
  // wait for a group start. If no other sequences in the group have a
  // threaded first element, the group no longer needs the additional wait.
  bool is_wait_still_needed = false;
  int group_id = to_return->animation_group_id();
  for (AnimationQueue::iterator queue_iter = animation_queue_.begin();
       queue_iter != animation_queue_.end(); ++queue_iter) {
    if (((*queue_iter)->animation_group_id() == group_id) &&
        (*queue_iter)->IsFirstElementThreaded(delegate_)) {
      is_wait_still_needed = true;
      break;
    }
  }

  if (is_wait_still_needed)
    return to_return.release();

  for (AnimationQueue::iterator queue_iter = animation_queue_.begin();
       queue_iter != animation_queue_.end(); ++queue_iter) {
    if ((*queue_iter)->animation_group_id() == group_id &&
        (*queue_iter)->waiting_for_group_start()) {
      (*queue_iter)->set_waiting_for_group_start(false);
      if (is_running) {
        (*queue_iter)->set_start_time(last_step_time_);
        (*queue_iter)->Start(delegate());
      }
    }
  }
  return to_return.release();
}

void LayerAnimator::FinishAnimation(
    LayerAnimationSequence* sequence, bool abort) {
  scoped_refptr<LayerAnimator> retain(this);
  std::unique_ptr<LayerAnimationSequence> removed(RemoveAnimation(sequence));
  if (abort)
    sequence->Abort(delegate());
  else
    ProgressAnimationToEnd(sequence);
  if (!delegate())
    return;
  ProcessQueue();
  UpdateAnimationState();
}

void LayerAnimator::FinishAnyAnimationWithZeroDuration() {
  scoped_refptr<LayerAnimator> retain(this);
  // Special case: if we've started a 0 duration animation, just finish it now
  // and get rid of it. We need to make a copy because Progress may indirectly
  // cause new animations to start running.
  RunningAnimations running_animations_copy = running_animations_;
  for (size_t i = 0; i < running_animations_copy.size(); ++i) {
    if (!SAFE_INVOKE_BOOL(HasAnimation, running_animations_copy[i]))
      continue;

    if (running_animations_copy[i].sequence()->IsFinished(
          running_animations_copy[i].sequence()->start_time())) {
      SAFE_INVOKE_VOID(ProgressAnimationToEnd, running_animations_copy[i]);
      std::unique_ptr<LayerAnimationSequence> removed(
          SAFE_INVOKE_PTR(RemoveAnimation, running_animations_copy[i]));
    }
  }
  ProcessQueue();
  UpdateAnimationState();
}

void LayerAnimator::ClearAnimations() {
  scoped_refptr<LayerAnimator> retain(this);
  ClearAnimationsInternal();
}

LayerAnimator::RunningAnimation* LayerAnimator::GetRunningAnimation(
    LayerAnimationElement::AnimatableProperty property) {
  PurgeDeletedAnimations();
  for (auto iter = running_animations_.begin();
       iter != running_animations_.end(); ++iter) {
    if ((*iter).sequence()->properties() & property)
      return &(*iter);
  }
  return nullptr;
}

void LayerAnimator::AddToQueueIfNotPresent(LayerAnimationSequence* animation) {
  // If we don't have the animation in the queue yet, add it.
  bool found_sequence = false;
  for (AnimationQueue::iterator queue_iter = animation_queue_.begin();
       queue_iter != animation_queue_.end(); ++queue_iter) {
    if (queue_iter->get() == animation) {
      found_sequence = true;
      break;
    }
  }

  if (!found_sequence) {
    animation_queue_.push_front(
        std::unique_ptr<LayerAnimationSequence>(animation));
  }
}

void LayerAnimator::RemoveAllAnimationsWithACommonProperty(
    LayerAnimationSequence* sequence, bool abort) {
  // For all the running animations, if they animate the same property,
  // progress them to the end and remove them. Note, Aborting or Progressing
  // animations may affect the collection of running animations, so we need to
  // operate on a copy.
  RunningAnimations running_animations_copy = running_animations_;
  for (size_t i = 0; i < running_animations_copy.size(); ++i) {
    if (!SAFE_INVOKE_BOOL(HasAnimation, running_animations_copy[i]))
      continue;

    if (running_animations_copy[i].sequence()->HasConflictingProperty(
            sequence->properties())) {
      std::unique_ptr<LayerAnimationSequence> removed(
          SAFE_INVOKE_PTR(RemoveAnimation, running_animations_copy[i]));
      if (abort)
        running_animations_copy[i].sequence()->Abort(delegate());
      else
        SAFE_INVOKE_VOID(ProgressAnimationToEnd, running_animations_copy[i]);
    }
  }

  // Same for the queued animations that haven't been started. Again, we'll
  // need to operate on a copy.
  std::vector<base::WeakPtr<LayerAnimationSequence> > sequences;
  for (AnimationQueue::iterator queue_iter = animation_queue_.begin();
       queue_iter != animation_queue_.end(); ++queue_iter)
    sequences.push_back((*queue_iter)->AsWeakPtr());

  for (size_t i = 0; i < sequences.size(); ++i) {
    if (!sequences[i].get() || !HasAnimation(sequences[i].get()))
      continue;

    if (sequences[i]->HasConflictingProperty(sequence->properties())) {
      std::unique_ptr<LayerAnimationSequence> removed(
          RemoveAnimation(sequences[i].get()));
      if (abort)
        sequences[i]->Abort(delegate());
      else
        ProgressAnimationToEnd(sequences[i].get());
    }
  }
}

void LayerAnimator::ImmediatelySetNewTarget(LayerAnimationSequence* sequence) {
  // Need to detect if our sequence gets destroyed.
  base::WeakPtr<LayerAnimationSequence> weak_sequence_ptr =
      sequence->AsWeakPtr();

  const bool abort = false;
  RemoveAllAnimationsWithACommonProperty(sequence, abort);
  if (!weak_sequence_ptr.get())
    return;

  LayerAnimationSequence* removed = RemoveAnimation(sequence);
  DCHECK(removed == nullptr || removed == sequence);
  if (!weak_sequence_ptr.get())
    return;

  ProgressAnimationToEnd(sequence);
  if (!weak_sequence_ptr.get())
    return;

  delete sequence;
}

void LayerAnimator::ImmediatelyAnimateToNewTarget(
    LayerAnimationSequence* sequence) {
  // Need to detect if our sequence gets destroyed.
  base::WeakPtr<LayerAnimationSequence> weak_sequence_ptr =
      sequence->AsWeakPtr();

  const bool abort = true;
  RemoveAllAnimationsWithACommonProperty(sequence, abort);
  if (!weak_sequence_ptr.get())
    return;

  AddToQueueIfNotPresent(sequence);
  if (!weak_sequence_ptr.get())
    return;

  StartSequenceImmediately(sequence);
}

void LayerAnimator::EnqueueNewAnimation(LayerAnimationSequence* sequence) {
  // It is assumed that if there was no conflicting animation, we would
  // not have been called. No need to check for a collision; just
  // add to the queue.
  animation_queue_.push_back(std::unique_ptr<LayerAnimationSequence>(sequence));
  ProcessQueue();
}

void LayerAnimator::ReplaceQueuedAnimations(LayerAnimationSequence* sequence) {
  // Need to detect if our sequence gets destroyed.
  base::WeakPtr<LayerAnimationSequence> weak_sequence_ptr =
      sequence->AsWeakPtr();

  // Remove all animations that aren't running. Note: at each iteration i is
  // incremented or an element is removed from the queue, so
  // animation_queue_.size() - i is always decreasing and we are always making
  // progress towards the loop terminating.
  for (size_t i = 0; i < animation_queue_.size();) {
    if (!weak_sequence_ptr.get())
      break;

    PurgeDeletedAnimations();

    bool is_running = false;
    for (RunningAnimations::const_iterator iter = running_animations_.begin();
         iter != running_animations_.end(); ++iter) {
      if ((*iter).sequence() == animation_queue_[i].get()) {
        is_running = true;
        break;
      }
    }

    if (!is_running)
      delete RemoveAnimation(animation_queue_[i].get());
    else
      ++i;
  }
  animation_queue_.push_back(std::unique_ptr<LayerAnimationSequence>(sequence));
  ProcessQueue();
}

void LayerAnimator::ProcessQueue() {
  bool started_sequence = false;
  do {
    started_sequence = false;
    // Build a list of all currently animated properties.
    LayerAnimationElement::AnimatableProperties animated =
        LayerAnimationElement::UNKNOWN;
    for (RunningAnimations::const_iterator iter = running_animations_.begin();
         iter != running_animations_.end(); ++iter) {
      if (!(*iter).is_sequence_alive())
        continue;

      animated |= (*iter).sequence()->properties();
    }

    // Try to find an animation that doesn't conflict with an animated
    // property or a property that will be animated before it. Note: starting
    // the animation may indirectly cause more animations to be started, so we
    // need to operate on a copy.
    std::vector<base::WeakPtr<LayerAnimationSequence> > sequences;
    for (AnimationQueue::iterator queue_iter = animation_queue_.begin();
         queue_iter != animation_queue_.end(); ++queue_iter)
      sequences.push_back((*queue_iter)->AsWeakPtr());

    for (size_t i = 0; i < sequences.size(); ++i) {
      if (!sequences[i].get() || !HasAnimation(sequences[i].get()))
        continue;

      if (!sequences[i]->HasConflictingProperty(animated)) {
        StartSequenceImmediately(sequences[i].get());
        started_sequence = true;
        break;
      }

      // Animation couldn't be started. Add its properties to the collection so
      // that we don't start a conflicting animation. For example, if our queue
      // has the elements { {T,B}, {B} } (that is, an element that animates both
      // the transform and the bounds followed by an element that animates the
      // bounds), and we're currently animating the transform, we can't start
      // the first element because it animates the transform, too. We cannot
      // start the second element, either, because the first element animates
      // bounds too, and needs to go first.
      animated |= sequences[i]->properties();
    }

    // If we started a sequence, try again. We may be able to start several.
  } while (started_sequence);
}

bool LayerAnimator::StartSequenceImmediately(LayerAnimationSequence* sequence) {
  PurgeDeletedAnimations();

  // Ensure that no one is animating one of the sequence's properties already.
  for (RunningAnimations::const_iterator iter = running_animations_.begin();
       iter != running_animations_.end(); ++iter) {
    if ((*iter).sequence()->HasConflictingProperty(sequence->properties()))
      return false;
  }

  // All clear, actually start the sequence.
  // All LayerAnimators share the same LayerAnimatorCollection. Use the
  // last_tick_time() from there to ensure animations started during the same
  // event complete at the same time.
  base::TimeTicks start_time;
  LayerAnimatorCollection* collection = GetLayerAnimatorCollection();
  if (is_animating() || adding_animations_)
    start_time = last_step_time_;
  else if (collection && collection->HasActiveAnimators())
    start_time = collection->last_tick_time();
  else
    start_time = base::TimeTicks::Now();

  if (!sequence->animation_group_id())
    sequence->set_animation_group_id(cc::AnimationIdProvider::NextGroupId());

  running_animations_.push_back(
      RunningAnimation(sequence->AsWeakPtr()));

  // Need to keep a reference to the animation.
  AddToQueueIfNotPresent(sequence);

  if (!sequence->waiting_for_group_start() ||
      sequence->IsFirstElementThreaded(delegate_)) {
    sequence->set_start_time(start_time);
    sequence->Start(delegate());
  }

  // Ensure that animations get stepped at their start time.
  Step(start_time);

  return true;
}

void LayerAnimator::GetTargetValue(
    LayerAnimationElement::TargetValue* target) const {
  for (AnimationQueue::const_iterator iter = animation_queue_.begin();
       iter != animation_queue_.end(); ++iter) {
    (*iter)->GetTargetValue(target);
  }
}

void LayerAnimator::OnScheduled(LayerAnimationSequence* sequence) {
  sequence_scheduled_callbacks_.Notify(sequence);
  for (LayerAnimationObserver& observer : observers_)
    sequence->AddObserver(&observer);
  sequence->OnScheduled();
}

void LayerAnimator::SetTransitionDuration(base::TimeDelta duration) {
  if (is_transition_duration_locked_)
    return;
  transition_duration_ = duration;
}

void LayerAnimator::ClearAnimationsInternal() {
  PurgeDeletedAnimations();

  // Abort should never affect the set of running animations, but just in case
  // clients are badly behaved, we will use a copy of the running animations.
  RunningAnimations running_animations_copy = running_animations_;
  for (size_t i = 0; i < running_animations_copy.size(); ++i) {
    if (!SAFE_INVOKE_BOOL(HasAnimation, running_animations_copy[i]))
      continue;

    std::unique_ptr<LayerAnimationSequence> removed(
        RemoveAnimation(running_animations_copy[i].sequence()));
    if (removed.get())
      removed->Abort(delegate());
  }
  // This *should* have cleared the list of running animations.
  DCHECK(running_animations_.empty());
  running_animations_.clear();
  animation_queue_.clear();
  UpdateAnimationState();
}

void LayerAnimator::PurgeDeletedAnimations() {
  for (size_t i = 0; i < running_animations_.size();) {
    if (!running_animations_[i].is_sequence_alive())
      running_animations_.erase(running_animations_.begin() + i);
    else
      i++;
  }
}

LayerAnimatorCollection* LayerAnimator::GetLayerAnimatorCollection() {
  return delegate_ ? delegate_->GetLayerAnimatorCollection() : nullptr;
}

void LayerAnimator::NotifyAnimationStarted(base::TimeTicks monotonic_time,
                                           int target_property,
                                           int group) {
  OnThreadedAnimationStarted(
      monotonic_time, static_cast<cc::TargetProperty::Type>(target_property),
      group);
}

LayerAnimator::RunningAnimation::RunningAnimation(
    const base::WeakPtr<LayerAnimationSequence>& sequence)
    : sequence_(sequence) {
}

LayerAnimator::RunningAnimation::RunningAnimation(
    const RunningAnimation& other) = default;

LayerAnimator::RunningAnimation::~RunningAnimation() { }

}  // namespace ui
