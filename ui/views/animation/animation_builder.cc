// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/animation_builder.h"

#include <algorithm>
#include <tuple>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/check_op.h"
#include "base/memory/ptr_util.h"
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

void AnimationBuilder::Observer::SetOnEnded(base::OnceClosure callback) {
  DCHECK(!on_ended_);
  on_ended_ = std::move(callback);
}

void AnimationBuilder::Observer::SetOnWillRepeat(
    base::RepeatingClosure callback) {
  DCHECK(!on_will_repeat_);
  on_will_repeat_ = std::move(callback);
}

void AnimationBuilder::Observer::SetOnAborted(base::OnceClosure callback) {
  DCHECK(!on_aborted_);
  on_aborted_ = std::move(callback);
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
  const auto running =
      base::ranges::count_if(attached_sequences(), [](auto* sequence) {
        return !sequence->IsFinished(base::TimeTicks::Now());
      });
  if (running <= 1) {
    if (on_ended_)
      std::move(on_ended_).Run();
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
    for (auto* seq : attached_sequences())
      repeat_map_[seq] = 0;
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
  if (on_aborted_)
    std::move(on_aborted_).Run();
  if (abort_handle_ && abort_handle_->animation_state() ==
                           AnimationAbortHandle::AnimationState::kRunning)
    abort_handle_->OnAnimationEnded();
}

void AnimationBuilder::Observer::OnLayerAnimationScheduled(
    ui::LayerAnimationSequence* sequence) {
  if (on_scheduled_)
    std::move(on_scheduled_).Run();
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
    return std::tie(start, original_duration, element) <
           std::tie(key.start, key.original_duration, key.element);
  }
};

AnimationBuilder::AnimationBuilder() = default;

AnimationBuilder::~AnimationBuilder() {
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

AnimationSequenceBlock AnimationBuilder::Once() {
  repeating_ = false;
  return NewSequence();
}

AnimationSequenceBlock AnimationBuilder::Repeatedly() {
  repeating_ = true;
  return NewSequence();
}

void AnimationBuilder::SetOnStarted(base::PassKey<AnimationSequenceBlock>,
                                    base::OnceClosure callback) {
  GetObserver()->SetOnStarted(std::move(callback));
}

void AnimationBuilder::SetOnEnded(base::PassKey<AnimationSequenceBlock>,
                                  base::OnceClosure callback) {
  GetObserver()->SetOnEnded(std::move(callback));
}

void AnimationBuilder::SetOnWillRepeat(base::PassKey<AnimationSequenceBlock>,
                                       base::RepeatingClosure callback) {
  GetObserver()->SetOnWillRepeat(std::move(callback));
}

void AnimationBuilder::SetOnAborted(base::PassKey<AnimationSequenceBlock>,
                                    base::OnceClosure callback) {
  GetObserver()->SetOnAborted(std::move(callback));
}

void AnimationBuilder::SetOnScheduled(base::PassKey<AnimationSequenceBlock>,
                                      base::OnceClosure callback) {
  GetObserver()->SetOnScheduled(std::move(callback));
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

void AnimationBuilder::BlockEndedAt(base::PassKey<AnimationSequenceBlock>,
                                    base::TimeDelta end) {
  end_ = std::max(end_, end);
}

void AnimationBuilder::TerminateSequence(
    base::PassKey<AnimationSequenceBlock>) {
  for (auto& pair : values_) {
    auto sequence = std::make_unique<ui::LayerAnimationSequence>();
    sequence->set_is_repeating(repeating_);
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
  animation_observer_.release();
}

std::unique_ptr<AnimationAbortHandle> AnimationBuilder::GetAbortHandle() {
  DCHECK(!abort_handle_) << "An abort handle is already created.";
  abort_handle_ = new AnimationAbortHandle(GetObserver());
  return base::WrapUnique(abort_handle_);
}

AnimationBuilder::Observer* AnimationBuilder::GetObserver() {
  if (!animation_observer_)
    animation_observer_ = std::make_unique<Observer>();
  return animation_observer_.get();
}

// static
void AnimationBuilder::SetObserverDeletedCallbackForTesting(
    base::RepeatingClosure deleted_closure) {
  GetObserverDeletedCallback() = std::move(deleted_closure);
}

AnimationSequenceBlock AnimationBuilder::NewSequence() {
  end_ = base::TimeDelta();
  return AnimationSequenceBlock(base::PassKey<AnimationBuilder>(), this,
                                base::TimeDelta());
}

// static
base::RepeatingClosure& AnimationBuilder::GetObserverDeletedCallback() {
  static base::NoDestructor<base::RepeatingClosure> on_observer_deleted;
  return *on_observer_deleted;
}

}  // namespace views
