// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_ANIMATION_SEQUENCE_H_
#define UI_VIEWS_ANIMATION_ANIMATION_SEQUENCE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "ui/views/animation/animation_key.h"

namespace ui {
class LayerAnimationElement;
}

namespace views {

class AnimationBuilder;
class AnimationSequenceBlock;

// An animation sequence is populated by zero or more possibly-overlapping
// animation sequence blocks.  The sequence doesn't own the blocks directly;
// rather, they hold pointers to it and repeatedly call AddElement() to add
// layer animation elements to the sequence.  Once the sequence is complete,
// TerminateSequence() causes the sequence to collect together all elements into
// layer animation sequences, which are added to the owning AnimationBuilder.
class AnimationSequence {
 public:
  // Repeat count value indicating infinite repeat.
  static constexpr int kRepeatIndefinitely = 0;

  explicit AnimationSequence(base::PassKey<AnimationBuilder>,
                             AnimationBuilder* owner);
  AnimationSequence(AnimationSequence&&);
  AnimationSequence& operator=(AnimationSequence&&);
  ~AnimationSequence();

  // Adds an animation element `element` for `key` at `start`.
  void AddElement(base::PassKey<AnimationSequenceBlock>,
                  AnimationKey key,
                  base::TimeDelta start,
                  std::unique_ptr<ui::LayerAnimationElement> element);

  // Causes this sequence to execute `repeat_count` total times.  If
  // `repeat_count` is `kRepeatIndefinitely`, the sequence loops forever.
  void set_repeat_count(base::PassKey<AnimationSequenceBlock>,
                        int repeat_count) {
    repeat_count_ = repeat_count;
  }

  // Called when the sequence is ended by EndSequence[Repeating](). Converts
  // `values_` to LayerAnimationSequences on the `owner_`.
  AnimationBuilder& TerminateSequence(base::PassKey<AnimationSequenceBlock>);

 private:
  struct Value;

  AnimationBuilder* owner_;
  int repeat_count_ = 1;

  // Each vector is kept in sorted order.
  std::map<AnimationKey, std::vector<Value>> values_;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_ANIMATION_SEQUENCE_H_
