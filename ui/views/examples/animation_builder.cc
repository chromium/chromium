// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/examples/animation_builder.h"

namespace views {

AnimationBuilder::AnimationBuilder() = default;

AnimationBuilder::~AnimationBuilder() {
  for (auto& animation : animation_sequences_) {
    View* view = animation.first;
    if (!view->layer())
      view->SetPaintToLayer();
    std::vector<ui::LayerAnimationSequence*> sequences;
    for (auto& s : animation.second) {
      sequences.push_back(s.release());
    }
    view->layer()->GetAnimator()->StartTogether(sequences);
  }
}

AnimationBuilder& AnimationBuilder::SetDuration(base::TimeDelta duration) {
  if (in_sequence_)
    old_duration_ = duration_;
  duration_ = duration;
  return *this;
}

AnimationBuilder& AnimationBuilder::SetOpacity(View* view,
                                               float target_opacity) {
  // Create an entry if it doesn't exist.
  if (animation_sequences_.find(view) == animation_sequences_.end())
    CreateNewEntry(view);

  AddAnimation(view, ui::LayerAnimationElement::CreateOpacityElement(
                         target_opacity, duration_));
  return *this;
}

AnimationBuilder& AnimationBuilder::SetRoundedCorners(
    View* view,
    gfx::RoundedCornersF& rounded_corners) {
  // Create an entry if it doesn't exist.
  if (animation_sequences_.find(view) == animation_sequences_.end())
    CreateNewEntry(view);

  AddAnimation(view, ui::LayerAnimationElement::CreateRoundedCornersElement(
                         rounded_corners, duration_));
  return *this;
}

AnimationBuilder& AnimationBuilder::Repeat() {
  // Go through all empty sequences added in StartSequence() and set the correct
  // repeating behavior.
  if (in_sequence_) {
    is_sequence_repeating_ = true;
    for (auto& animation : animation_sequences_) {
      animation_sequences_[animation.first].back()->set_is_repeating(
          is_sequence_repeating_);
    }
  }
  return *this;
}

AnimationBuilder& AnimationBuilder::StartSequence() {
  in_sequence_ = true;
  // Add an empty sequence for all existing views.
  for (auto& animation : animation_sequences_) {
    std::unique_ptr<ui::LayerAnimationSequence> new_sequence =
        std::make_unique<ui::LayerAnimationSequence>();
    animation_sequences_[animation.first].push_back(std::move(new_sequence));
  }
  return *this;
}

AnimationBuilder& AnimationBuilder::EndSequence() {
  in_sequence_ = false;
  is_sequence_repeating_ = false;
  duration_ = old_duration_;
  // Remove sequences that were not added to.
  for (auto& animation : animation_sequences_) {
    if (animation_sequences_[animation.first].back()->size() == 0) {
      animation_sequences_[animation.first].pop_back();
    }
  }
  return *this;
}

void AnimationBuilder::CreateNewEntry(View* view) {
  animation_sequences_[view] =
      std::vector<std::unique_ptr<ui::LayerAnimationSequence>>();
  if (in_sequence_) {
    // New empty sequence has not been added in StartSequence yet
    std::unique_ptr<ui::LayerAnimationSequence> new_sequence =
        std::make_unique<ui::LayerAnimationSequence>();
    new_sequence->set_is_repeating(is_sequence_repeating_);
    animation_sequences_[view].push_back(std::move(new_sequence));
  }
}

void AnimationBuilder::AddAnimation(
    View* view,
    std::unique_ptr<ui::LayerAnimationElement> element) {
  if (in_sequence_) {
    // Add to existing sequence so that these animations are done sequentially
    animation_sequences_[view].back()->AddElement(std::move(element));
  } else {
    // Create a new sequence with one element
    std::unique_ptr<ui::LayerAnimationSequence> new_sequence =
        std::make_unique<ui::LayerAnimationSequence>();
    new_sequence->AddElement(std::move(element));
    animation_sequences_[view].push_back(std::move(new_sequence));
  }
}

}  // namespace views
