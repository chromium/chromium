// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_ANIMATION_BUILDER_H_
#define UI_VIEWS_ANIMATION_ANIMATION_BUILDER_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "ui/compositor/layer_animation_element.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace ui {
class LayerAnimationSequence;
class LayerAnimationElement;
}  // namespace ui

namespace views {

// This AnimationBuilder API is currently in the experimental phase and only
// used within ui/views/examples/.
class VIEWS_EXPORT AnimationBuilder {
 public:
  AnimationBuilder();
  ~AnimationBuilder();

  AnimationBuilder& SetDuration(base::TimeDelta duration);

  // These methods should be changed to OnSetXXX if we integrate with the View
  // base class.
  AnimationBuilder& SetOpacity(View* view, float target_opacity);
  AnimationBuilder& SetRoundedCorners(views::View* view,
                                      gfx::RoundedCornersF& rounded_corners);

  // No effect if called before NewSequence();
  AnimationBuilder& Repeat();
  AnimationBuilder& NewSequence();
  AnimationBuilder& EndSequence();

 private:
  // We may want to change this to our own struct.
  using AnimationKey =
      std::pair<View*, ui::LayerAnimationElement::AnimatableProperty>;

  void CreateNewEntry(const AnimationKey& key);
  void AddAnimation(const AnimationKey& key,
                    std::unique_ptr<ui::LayerAnimationElement> element);

  std::map<AnimationKey,
           std::vector<std::unique_ptr<ui::LayerAnimationSequence>>>
      animation_sequences_;

  base::TimeDelta duration_ = base::TimeDelta::FromSeconds(1);

  bool is_sequence_repeating_ = false;
};
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_ANIMATION_BUILDER_H_
