// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_ANIMATION_BUILDER_H_
#define UI_VIEWS_EXAMPLES_ANIMATION_BUILDER_H_

#include <map>
#include <memory>
#include <vector>

#include "ui/views/view.h"

namespace ui {
class LayerAnimationSequence;
class LayerAnimationElement;
}  // namespace ui

namespace views {

// This AnimationBuilder API is currently in the experimental phase and only
// used within ui/views/examples/.
// This class should eventually be moved out of ui/views/examples/ if we proceed
// with this implementation.
class AnimationBuilder {
 public:
  AnimationBuilder();
  ~AnimationBuilder();

  AnimationBuilder& SetDuration(base::TimeDelta duration);

  // These methods should be changed to OnSetXXX if we integrate with the View
  // base class.
  AnimationBuilder& SetOpacity(View* view, float target_opacity);
  AnimationBuilder& SetRoundedCorners(views::View* view,
                                      gfx::RoundedCornersF& rounded_corners);

  // No effect if called before StartSequence();
  AnimationBuilder& Repeat();
  // Currently does not support nested sequences
  AnimationBuilder& StartSequence();
  AnimationBuilder& EndSequence();

 private:
  void CreateNewEntry(View* view);
  void AddAnimation(View* view,
                    std::unique_ptr<ui::LayerAnimationElement> element);

  std::map<View*, std::vector<std::unique_ptr<ui::LayerAnimationSequence>>>
      animation_sequences_;
  bool in_sequence_ = false;
  bool is_sequence_repeating_ = false;
  base::TimeDelta duration_ = base::TimeDelta::FromSeconds(1);
  base::TimeDelta old_duration_;
};
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_ANIMATION_BUILDER_H_
