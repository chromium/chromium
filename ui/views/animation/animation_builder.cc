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
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_owner.h"
#include "ui/views/animation/animation_key.h"
#include "ui/views/animation/animation_sequence_block.h"

namespace views {

class AnimationBuilder::Observer : public ui::LayerAnimationObserver {
 public:
  Observer() = default;
  Observer(const Observer&) = delete;
  Observer& operator=(const Observer&) = delete;
  ~Observer() override = default;

  void SetOnStarted(base::OnceClosure callback);
  void SetOnEnded(base::OnceClosure callback);
  void SetOnWillRepeat(base::RepeatingClosure callback);
  void SetOnAborted(base::OnceClosure callback);
  void SetOnScheduled(base::OnceClosure callback);

  // ui::LayerAnimationObserver:
  void OnLayerAnimationStarted(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationWillRepeat(
      ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationScheduled(ui::LayerAnimationSequence* sequence) override;

 private:
  base::OnceClosure on_started_;
  base::OnceClosure on_ended_;
  base::RepeatingClosure on_will_repeat_;
  base::OnceClosure on_aborted_;
  base::OnceClosure on_scheduled_;
};

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
  if (on_started_)
    std::move(on_started_).Run();
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
    delete this;
  }
}

void AnimationBuilder::Observer::OnLayerAnimationWillRepeat(
    ui::LayerAnimationSequence* sequence) {
  // TODO(kylixrd): This should only be called once for each repeat sequence.
  // Figure out how to limit this to one invocation.
}

void AnimationBuilder::Observer::OnLayerAnimationAborted(
    ui::LayerAnimationSequence* sequence) {
  if (on_aborted_)
    std::move(on_aborted_).Run();
  // TODO(kylixrd): Probably should propagate the abort to the other
  // LayerAnimationSequences.
}

void AnimationBuilder::Observer::OnLayerAnimationScheduled(
    ui::LayerAnimationSequence* sequence) {
  if (on_scheduled_)
    std::move(on_scheduled_).Run();
}

struct AnimationBuilder::Value {
  base::TimeDelta start;
  std::unique_ptr<ui::LayerAnimationElement> element;

  bool operator<(const Value& key) const {
    return std::tie(start, element) < std::tie(key.start, key.element);
  }
};

AnimationBuilder::AnimationBuilder() = default;

AnimationBuilder::~AnimationBuilder() {
  for (auto it = layer_animation_sequences_.begin();
       it != layer_animation_sequences_.end();) {
    auto* const target = it->first;
    auto end_it = layer_animation_sequences_.upper_bound(target);
    std::vector<ui::LayerAnimationSequence*> sequences;
    std::transform(it, end_it, std::back_inserter(sequences), [this](auto& it) {
      if (animation_observer_)
        it.second->AddObserver(animation_observer_.get());
      return it.second.release();
    });
    DCHECK(target->layer()) << "Animation targets must paint to a layer.";
    target->layer()->GetAnimator()->StartTogether(std::move(sequences));
    it = end_it;
  }
  if (animation_observer_)
    animation_observer_.release();
}

AnimationSequenceBlock AnimationBuilder::Once() {
  repeating_ = false;
  return AnimationSequenceBlock(base::PassKey<AnimationBuilder>(), this,
                                base::TimeDelta());
}

AnimationSequenceBlock AnimationBuilder::Repeatedly() {
  repeating_ = true;
  return AnimationSequenceBlock(base::PassKey<AnimationBuilder>(), this,
                                base::TimeDelta());
}

AnimationBuilder& AnimationBuilder::OnStarted(base::OnceClosure callback) {
  GetObserver()->SetOnStarted(std::move(callback));
  return *this;
}

AnimationBuilder& AnimationBuilder::OnEnded(base::OnceClosure callback) {
  GetObserver()->SetOnEnded(std::move(callback));
  return *this;
}

AnimationBuilder& AnimationBuilder::OnWillRepeat(
    base::RepeatingClosure callback) {
  GetObserver()->SetOnWillRepeat(std::move(callback));
  return *this;
}

AnimationBuilder& AnimationBuilder::OnAborted(base::OnceClosure callback) {
  GetObserver()->SetOnAborted(std::move(callback));
  return *this;
}

AnimationBuilder& AnimationBuilder::OnScheduled(base::OnceClosure callback) {
  GetObserver()->SetOnScheduled(std::move(callback));
  return *this;
}

void AnimationBuilder::AddLayerAnimationElement(
    base::PassKey<AnimationSequenceBlock>,
    AnimationKey key,
    base::TimeDelta start,
    std::unique_ptr<ui::LayerAnimationElement> element) {
  auto& values = values_[key];
  Value value = {start, std::move(element)};
  auto it = base::ranges::upper_bound(values, value);
  values.insert(it, std::move(value));
}

void AnimationBuilder::TerminateSequence(
    base::PassKey<AnimationSequenceBlock>) {
  for (auto& pair : values_) {
    auto sequence = std::make_unique<ui::LayerAnimationSequence>();
    sequence->set_is_repeating(repeating_);

    base::TimeDelta start;
    for (auto& value : pair.second) {
      DCHECK_GE(value.start, start)
          << "Do not overlap animations of the same property on the same view.";
      if (value.start > start) {
        sequence->AddElement(ui::LayerAnimationElement::CreatePauseElement(
            value.element->properties(), value.start - start));
        start = value.start;
      }
      start += value.element->duration();
      sequence->AddElement(std::move(value.element));
    }

    layer_animation_sequences_.insert({pair.first.target, std::move(sequence)});
  }

  values_.clear();
}

AnimationBuilder::Observer* AnimationBuilder::GetObserver() {
  if (!animation_observer_)
    animation_observer_ = std::make_unique<Observer>();
  return animation_observer_.get();
}

}  // namespace views
