// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/animation_builder.h"

#include "base/containers/contains.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"

namespace {
constexpr auto kDefaultDuration = base::TimeDelta::FromMilliseconds(150);
}  // namespace

namespace views {

AnimationBuilder::AnimationBuilder() = default;

AnimationBuilder::~AnimationBuilder() {
  // Collect all animations of a view into one vector so we can start them
  // together.
  base::flat_map<View*, std::vector<ui::LayerAnimationSequence*>>
      all_animations;
  for (auto& animation : animation_sequences_) {
    View* view = animation.first.view;
    if (!view->layer())
      view->SetPaintToLayer();
    for (auto& s : animation.second)
      all_animations[view].push_back(s.release());
  }
  for (auto& a : all_animations)
    a.first->layer()->GetAnimator()->StartTogether(a.second);
}

AnimationBuilder& AnimationBuilder::NewSequence() {
  // Add an empty sequence for all existing views. If the same property is
  // animated at the same time in different sequences PreemptionStrategy will
  // determine how the animations are replaced.
  for (auto& animation : animation_sequences_) {
    auto new_sequence = std::make_unique<ui::LayerAnimationSequence>();
    animation_sequences_[animation.first].push_back(std::move(new_sequence));
  }
  return *this;
}

AnimationBuilder& AnimationBuilder::EndSequence() {
  is_sequence_repeating_ = false;
  duration_ = kDefaultDuration;
  // Remove sequences that were not added to.
  for (auto& animation : animation_sequences_) {
    if (animation_sequences_[animation.first].back()->size() == 0) {
      animation_sequences_[animation.first].pop_back();
    }
  }
  return *this;
}

AnimationBuilder& AnimationBuilder::SetDuration(base::TimeDelta duration) {
  duration_ = duration;
  return *this;
}

AnimationBuilder& AnimationBuilder::Repeat() {
  // Go through all empty sequences added in StartSequence() and set the correct
  // repeating behavior.
  is_sequence_repeating_ = true;
  for (auto& animation : animation_sequences_) {
    animation_sequences_[animation.first].back()->set_is_repeating(
        is_sequence_repeating_);
  }
  return *this;
}

AnimationBuilder& AnimationBuilder::Then() {
  return *this;
}

AnimationBuilder& AnimationBuilder::SetOpacity(View* view,
                                               float target_opacity) {
  AnimationKey key = {view, ui::LayerAnimationElement::OPACITY};
  AddAnimation(key, ui::LayerAnimationElement::CreateOpacityElement(
                        target_opacity, duration_));
  return *this;
}

AnimationBuilder& AnimationBuilder::SetRoundedCorners(
    View* view,
    gfx::RoundedCornersF& rounded_corners) {
  AnimationKey key = {view, ui::LayerAnimationElement::ROUNDED_CORNERS};
  AddAnimation(key, ui::LayerAnimationElement::CreateRoundedCornersElement(
                        rounded_corners, duration_));
  return *this;
}

void AnimationBuilder::OnStarted(base::OnceClosure callback) {}

void AnimationBuilder::OnEnded(base::OnceClosure callback) {}

void AnimationBuilder::OnWillRepeat(base::RepeatingClosure callback) {}

void AnimationBuilder::OnAborted(base::OnceClosure callback) {}

void AnimationBuilder::OnScheduled(base::OnceClosure callback) {}

void AnimationBuilder::CreateNewEntry(const AnimationKey& key) {
  auto new_sequence = std::make_unique<ui::LayerAnimationSequence>();
  new_sequence->set_is_repeating(is_sequence_repeating_);
  animation_sequences_[key].push_back(std::move(new_sequence));
}

// TODO(elainechien): Add a DCHECK to make sure in one block we do not add two
// different property changes on the same view.
void AnimationBuilder::AddAnimation(
    const AnimationKey& key,
    std::unique_ptr<ui::LayerAnimationElement> element) {
  // Create an entry if it doesn't exist.
  if (animation_sequences_.find(key) == animation_sequences_.end())
    CreateNewEntry(key);
  animation_sequences_[key].back()->AddElement(std::move(element));
}

AnimationBuilder::AnimationBuilderObserver::AnimationBuilderObserver() =
    default;

AnimationBuilder::AnimationBuilderObserver::~AnimationBuilderObserver() {
  Reset();
}

void AnimationBuilder::AnimationBuilderObserver::ObserveAnimationSequence(
    ui::LayerAnimationSequence* sequence) {
  DCHECK(!base::Contains(sequences_, sequence,
                         &base::WeakPtr<ui::LayerAnimationSequence>::get));
  sequence->AddObserver(this);
  sequences_.emplace_back(sequence->AsWeakPtr());
}

void AnimationBuilder::AnimationBuilderObserver::OnLayerAnimationStarted(
    ui::LayerAnimationSequence* sequence) {}

void AnimationBuilder::AnimationBuilderObserver::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* sequence) {}

void AnimationBuilder::AnimationBuilderObserver::OnLayerAnimationWillRepeat(
    ui::LayerAnimationSequence* sequence) {}

void AnimationBuilder::AnimationBuilderObserver::OnLayerAnimationAborted(
    ui::LayerAnimationSequence* sequence) {}

void AnimationBuilder::AnimationBuilderObserver::OnLayerAnimationScheduled(
    ui::LayerAnimationSequence* sequence) {}

void AnimationBuilder::AnimationBuilderObserver::Reset() {
  for (auto& sequence : sequences_) {
    if (sequence.get())
      sequence.get()->RemoveObserver(this);
  }
}

}  // namespace views
