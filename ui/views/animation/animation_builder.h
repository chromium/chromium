// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_ANIMATION_BUILDER_H_
#define UI_VIEWS_ANIMATION_ANIMATION_BUILDER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/no_destructor.h"
#include "base/types/pass_key.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/views/animation/animation_key.h"
#include "ui/views/animation/animation_sequence_block.h"
#include "ui/views/views_export.h"

// This AnimationBuilder API is currently in the experimental phase and only
// used within ui/views/examples/.

namespace ui {
class LayerOwner;
}

namespace views {

class VIEWS_EXPORT AnimationBuilder {
 public:
  AnimationBuilder();
  ~AnimationBuilder();

  // Creates a new sequence (that optionally repeats).
  AnimationSequenceBlock Once();
  AnimationSequenceBlock Repeatedly();

  // Adds an animation element `element` for `key` at `start` to `values`.
  void AddLayerAnimationElement(
      base::PassKey<AnimationSequenceBlock>,
      AnimationKey key,
      base::TimeDelta start,
      std::unique_ptr<ui::LayerAnimationElement> element);

  // Called when a block ends.  Ensures all animations in the sequence will run
  // until at least `end`.
  void BlockEndedAt(base::PassKey<AnimationSequenceBlock>, base::TimeDelta end);

  // Called when the sequence is ended. Converts `values_` to
  // `layer_animation_sequences_`.
  void TerminateSequence(base::PassKey<AnimationSequenceBlock>);

  // Called through the corresponding functions on AnimationSequenceBlock.
  void SetOnStarted(base::OnceClosure callback);
  void SetOnEnded(base::OnceClosure callback);
  void SetOnWillRepeat(base::RepeatingClosure callback);
  void SetOnAborted(base::OnceClosure callback);
  void SetOnScheduled(base::OnceClosure callback);

  static void SetObserverDeletedCallbackForTesting(
      base::RepeatingClosure deleted_closure);

 private:
  class Observer;
  struct Value;

  Observer* GetObserver();

  // Resets data for the current sequence as necessary, creates and returns the
  // initial block.
  AnimationSequenceBlock NewSequence();

  // Data for all sequences.
  std::multimap<ui::LayerOwner*, std::unique_ptr<ui::LayerAnimationSequence>>
      layer_animation_sequences_;
  std::unique_ptr<Observer> animation_observer_;

  // Data for the current sequence.
  bool repeating_;
  base::TimeDelta end_;
  // Each vector is kept in sorted order.
  std::map<AnimationKey, std::vector<Value>> values_;

  // Callback used for testing.
  static base::NoDestructor<base::RepeatingClosure> on_observer_deleted_;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_ANIMATION_BUILDER_H_
