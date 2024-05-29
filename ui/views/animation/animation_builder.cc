// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/animation_builder.h"

#include <algorithm>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/debug/alias.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/animation/animation_abort_handle.h"
#include "ui/views/animation/animation_key.h"
#include "ui/views/animation/animation_sequence_block.h"

namespace views {

AnimationBuilder::Observer::Observer() = default;

AnimationBuilder::Observer::~Observer() {
  DCHECK(attached_to_sequence_)
      << "You must register callbacks and get abort handle before "
      << "creating a sequence block.";
  if (abort_handle_)
    abort_handle_->OnObserverDeleted();
  base::RepeatingClosure& on_observer_deleted =
      AnimationBuilder::GetObserverDeletedCallback();
  if (on_observer_deleted)
    on_observer_deleted.Run();
}

void AnimationBuilder::Observer::SetOnStarted(base::OnceClosure callback) {
  DCHECK(!on_started_);
  on_started_ = std::move(callback);
}

void AnimationBuilder::Observer::SetOnEnded(base::OnceClosure callback,
                                            base::Location location) {
  DCHECK(!on_ended_);
  on_ended_ = std::move(callback);
  on_ended_location_ = location;
}

void AnimationBuilder::Observer::SetOnWillRepeat(
    base::RepeatingClosure callback) {
  DCHECK(!on_will_repeat_);
  on_will_repeat_ = std::move(callback);
}

void AnimationBuilder::Observer::SetOnAborted(base::OnceClosure callback,
                                              base::Location location) {
  DCHECK(!on_aborted_);
  on_aborted_ = std::move(callback);
  on_aborted_location_ = location;
}

void AnimationBuilder::Observer::SetOnScheduled(base::OnceClosure callback) {
  DCHECK(!on_scheduled_);
  on_scheduled_ = std::move(callback);
}

void AnimationBuilder::Observer::OnLayerAnimationStarted(
    ui::LayerAnimationSequence* sequence) {
  if (abort_handle_ && abort_handle_->animation_state() ==
                           AnimationAbortHandle::AnimationState::kNotStarted) {
    abort_handle_->OnAnimationStarted();
  }
  if (on_started_)
    std::move(on_started_).Run();
}

void AnimationBuilder::Observer::SetAbortHandle(
    AnimationAbortHandle* abort_handle) {
  abort_handle_ = abort_handle;
}

void AnimationBuilder::Observer::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* sequence) {
  if (--sequences_to_run_ == 0) {
    if (on_ended_) {
      // Ensure that the stack contains information about which on_ended_
      // callback this is. Needed to debug a bad callback crash
      // (https://g-issues.chromium.org/issues/335902543).
      // TODO(b/335902543): Remove on_ended_location.
      base::Location on_ended_location = on_ended_location_;
      base::debug::Alias(&on_ended_location);
      std::move(on_ended_).Run();
    }
    if (abort_handle_ && abort_handle_->animation_state() ==
                             AnimationAbortHandle::AnimationState::kRunning)
      abort_handle_->OnAnimationEnded();
  }
}

void AnimationBuilder::Observer::OnLayerAnimationWillRepeat(
    ui::LayerAnimationSequence* sequence) {
  if (!on_will_repeat_)
    return;
  // First time through, initialize the repeat_map_ with the sequences.
  if (repeat_map_.empty()) {
    for (ui::LayerAnimationSequence* seq : attached_sequences()) {
      repeat_map_[seq] = 0;
    }
  }
  // Only trigger the repeat callback on the last LayerAnimationSequence on
  // which this observer is attached.
  const int next_cycle = ++repeat_map_[sequence];
  if (base::ranges::none_of(
          repeat_map_, [next_cycle](int count) { return count < next_cycle; },
          &RepeatMap::value_type::second)) {
    on_will_repeat_.Run();
  }
}

void AnimationBuilder::Observer::OnLayerAnimationAborted(
    ui::LayerAnimationSequence* sequence) {
  if (on_aborted_) {
    // Ensure that the stack contains information about which on_aborted_
    // callback this is. Needed to debug a bad callback crash
    // (https://g-issues.chromium.org/issues/335902543).
    // TODO(b/335902543): Remove on_aborted_location_.
    base::Location on_aborted_location = on_aborted_location_;
    base::debug::Alias(&on_aborted_location);
    std::move(on_aborted_).Run();
  }
  if (abort_handle_ && abort_handle_->animation_state() ==
                           AnimationAbortHandle::AnimationState::kRunning)
    abort_handle_->OnAnimationEnded();
}

void AnimationBuilder::Observer::OnLayerAnimationScheduled(
    ui::LayerAnimationSequence* sequence) {
  if (on_scheduled_)
    std::move(on_scheduled_).Run();
}

void AnimationBuilder::Observer::OnAttachedToSequence(
    ui::LayerAnimationSequence* sequence) {
  ui::LayerAnimationObserver::OnAttachedToSequence(sequence);
  attached_to_sequence_ = true;
  sequences_to_run_++;
}

void AnimationBuilder::Observer::OnDetachedFromSequence(
    ui::LayerAnimationSequence* sequence) {
  if (attached_sequences().empty())
    delete this;
}

bool AnimationBuilder::Observer::RequiresNotificationWhenAnimatorDestroyed()
    const {
  return true;
}

struct AnimationBuilder::Value {
  base::TimeDelta start;
  // Save the original duration because the duration on the element can be a
  // scaled version. The scale can potentially be zero.
  base::TimeDelta original_duration;
  std::unique_ptr<ui::LayerAnimationElement> element;

  bool operator<(const Value& key) const {
    // Animations with zero duration need to be ordered before animations with
    // nonzero of the same start time to prevent the DCHECK from happening in
    // TerminateSequence(). These animations don't count as overlapping
    // properties.
    auto element_properties = element->properties();
    auto key_element_properties = key.element->properties();
    return std::tie(start, original_duration, element_properties) <
           std::tie(key.start, key.original_duration, key_element_properties);
  }
};

AnimationBuilder::AnimationBuilder() = default;

AnimationBuilder::AnimationBuilder(AnimationBuilder&& rhs) = default;

AnimationBuilder& AnimationBuilder::operator=(AnimationBuilder&& rhs) = default;

AnimationBuilder::~AnimationBuilder() {
  // Terminate `current_sequence_` to complete layer animation configuration.
  current_sequence_.reset();

  DCHECK(!next_animation_observer_)
      << "Callbacks were scheduled without creating a sequence block "
         "afterwards. There are no animations to run these callbacks on.";
  // The observer needs to outlive the AnimationBuilder and will manage its own
  // lifetime. GetAttachedToSequence should not return false here. This is
  // DCHECKed in the observer’s destructor.
  if (animation_observer_ && animation_observer_->GetAttachedToSequence())
    animation_observer_.release();

  for (auto it = layer_animation_sequences_.begin();
       it != layer_animation_sequences_.end();) {
    auto* const target = it->first;
    auto end_it = layer_animation_sequences_.upper_bound(target);

    if (abort_handle_)
      abort_handle_->AddLayer(target);

    ui::ScopedLayerAnimationSettings settings(target->GetAnimator());
    if (preemption_strategy_)
      settings.SetPreemptionStrategy(preemption_strategy_.value());
    std::vector<ui::LayerAnimationSequence*> sequences;
    std::transform(it, end_it, std::back_inserter(sequences),
                   [](auto& it) { return it.second.release(); });
    target->GetAnimator()->StartTogether(std::move(sequences));
    it = end_it;
  }
}

AnimationBuilder& AnimationBuilder::SetPreemptionStrategy(
    ui::LayerAnimator::PreemptionStrategy preemption_strategy) {
  preemption_strategy_ = preemption_strategy;
  return *this;
}

AnimationBuilder& AnimationBuilder::OnStarted(base::OnceClosure callback) {
  GetObserver()->SetOnStarted(std::move(callback));
  return *this;
}

AnimationBuilder& AnimationBuilder::OnEnded(base::OnceClosure callback,
                                            base::Location location) {
  GetObserver()->SetOnEnded(std::move(callback), location);
  return *this;
}

AnimationBuilder& AnimationBuilder::OnWillRepeat(
    base::RepeatingClosure callback) {
  GetObserver()->SetOnWillRepeat(std::move(callback));
  return *this;
}

AnimationBuilder& AnimationBuilder::OnAborted(base::OnceClosure callback,
                                              base::Location location) {
  GetObserver()->SetOnAborted(std::move(callback), location);
  return *this;
}

AnimationBuilder& AnimationBuilder::OnScheduled(base::OnceClosure callback) {
  GetObserver()->SetOnScheduled(std::move(callback));
  return *this;
}

AnimationSequenceBlock& AnimationBuilder::Once() {
  return NewSequence(false);
}

AnimationSequenceBlock& AnimationBuilder::Repeatedly() {
  return NewSequence(true);
}

void AnimationBuilder::AddLayerAnimationElement(
    base::PassKey<AnimationSequenceBlock>,
    AnimationKey key,
    base::TimeDelta start,
    base::TimeDelta original_duration,
    std::unique_ptr<ui::LayerAnimationElement> element) {
  auto& values = values_[key];
  Value value = {start, original_duration, std::move(element)};
  auto it = base::ranges::upper_bound(values, value);
  values.insert(it, std::move(value));
}

std::unique_ptr<AnimationSequenceBlock> AnimationBuilder::SwapCurrentSequence(
    base::PassKey<AnimationSequenceBlock>,
    std::unique_ptr<AnimationSequenceBlock> new_sequence) {
  auto old_sequence = std::move(current_sequence_);
  current_sequence_ = std::move(new_sequence);
  return old_sequence;
}

void AnimationBuilder::BlockEndedAt(base::PassKey<AnimationSequenceBlock>,
                                    base::TimeDelta end) {
  end_ = std::max(end_, end);
}

void AnimationBuilder::TerminateSequence(base::PassKey<AnimationSequenceBlock>,
                                         bool repeating) {
  for (auto& pair : values_) {
    auto sequence = std::make_unique<ui::LayerAnimationSequence>();
    sequence->set_is_repeating(repeating);
    if (animation_observer_)
      sequence->AddObserver(animation_observer_.get());

    base::TimeDelta start;
    ui::LayerAnimationElement::AnimatableProperties properties =
        ui::LayerAnimationElement::UNKNOWN;
    for (auto& value : pair.second) {
      DCHECK_GE(value.start, start)
          << "Do not overlap animations of the same property on the same view.";
      properties = value.element->properties();
      if (value.start > start) {
        sequence->AddElement(ui::LayerAnimationElement::CreatePauseElement(
            properties, value.start - start));
        start = value.start;
      }
      start += value.original_duration;
      sequence->AddElement(std::move(value.element));
    }

    if (start < end_) {
      sequence->AddElement(ui::LayerAnimationElement::CreatePauseElement(
          properties, end_ - start));
    }

    layer_animation_sequences_.insert({pair.first.target, std::move(sequence)});
  }

  values_.clear();
}

AnimationSequenceBlock& AnimationBuilder::GetCurrentSequence() {
  DCHECK(current_sequence_);
  return *current_sequence_;
}

std::unique_ptr<AnimationAbortHandle> AnimationBuilder::GetAbortHandle() {
  DCHECK(!abort_handle_) << "An abort handle is already created.";
  abort_handle_ = new AnimationAbortHandle(GetObserver());
  return base::WrapUnique(abort_handle_.get());
}

AnimationBuilder::Observer* AnimationBuilder::GetObserver() {
  if (!next_animation_observer_)
    next_animation_observer_ = std::make_unique<Observer>();
  return next_animation_observer_.get();
}

// static
void AnimationBuilder::SetObserverDeletedCallbackForTesting(
    base::RepeatingClosure deleted_closure) {
  GetObserverDeletedCallback() = std::move(deleted_closure);
}

AnimationSequenceBlock& AnimationBuilder::NewSequence(bool repeating) {
  // Each sequence should have its own observer.

  // Ensure to terminate the current sequence block before touching the
  // animation sequence observer so that the sequence observer is attached to
  // the layer animation sequence.
  if (current_sequence_)
    current_sequence_.reset();

  // The observer needs to outlive the AnimationBuilder and will manage its own
  // lifetime. GetAttachedToSequence should not return false here. This is
  // DCHECKed in the observer’s destructor.
  if (animation_observer_ && animation_observer_->GetAttachedToSequence())
    animation_observer_.release();
  if (next_animation_observer_) {
    animation_observer_ = std::move(next_animation_observer_);
    next_animation_observer_.reset();
  }

  end_ = base::TimeDelta();
  current_sequence_ = std::make_unique<AnimationSequenceBlock>(
      base::PassKey<AnimationBuilder>(), this, base::TimeDelta(), repeating);
  return *current_sequence_;
}

// static
base::RepeatingClosure& AnimationBuilder::GetObserverDeletedCallback() {
  static base::NoDestructor<base::RepeatingClosure> on_observer_deleted;
  return *on_observer_deleted;
}

}  // namespace views
