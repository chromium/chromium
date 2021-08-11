// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/animation_sequence.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/animation_key.h"

namespace views {

struct AnimationSequence::Value {
  base::TimeDelta start;
  std::unique_ptr<ui::LayerAnimationElement> element;

  bool operator<(const Value& key) const {
    return std::tie(start, element) < std::tie(key.start, key.element);
  }
};

AnimationSequence::AnimationSequence(base::PassKey<AnimationBuilder>,
                                     AnimationBuilder* owner,
                                     bool repeating)
    : owner_(owner), repeating_(repeating) {}

AnimationSequence::AnimationSequence(AnimationSequence&&) = default;

AnimationSequence& AnimationSequence::operator=(AnimationSequence&&) = default;

AnimationSequence::~AnimationSequence() {
  DCHECK(values_.empty());
}

void AnimationSequence::AddElement(
    base::PassKey<AnimationSequenceBlock>,
    AnimationKey key,
    base::TimeDelta start,
    std::unique_ptr<ui::LayerAnimationElement> element) {
  auto& values = values_[key];
  Value value = {start, std::move(element)};
  auto it = base::ranges::upper_bound(values, value);
  values.insert(it, std::move(value));
}

AnimationBuilder& AnimationSequence::TerminateSequence(
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

    owner_->AddLayerAnimationSequence(base::PassKey<AnimationSequence>(),
                                      pair.first.target, std::move(sequence));
  }

  values_.clear();

  return *owner_;
}

}  // namespace views
