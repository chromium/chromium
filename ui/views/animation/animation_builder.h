// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_ANIMATION_BUILDER_H_
#define UI_VIEWS_ANIMATION_ANIMATION_BUILDER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/types/pass_key.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/views/animation/animation_sequence.h"
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

  // Called when the animation starts.
  AnimationBuilder& OnStarted(base::OnceClosure callback);
  // Called when the animation ends. Not called if animation is aborted.
  AnimationBuilder& OnEnded(base::OnceClosure callback);
  // Called when a sequence repetition ends and will repeat. Not called if
  // sequence is aborted.
  AnimationBuilder& OnWillRepeat(base::RepeatingClosure callback);
  // Called if animation is aborted for any reason. Should never do anything
  // that may cause another animation to be started.
  AnimationBuilder& OnAborted(base::OnceClosure callback);
  // Called when the animation is scheduled.
  AnimationBuilder& OnScheduled(base::OnceClosure callback);

  // Adds `sequence` for `target`.
  void AddLayerAnimationSequence(
      base::PassKey<AnimationSequence>,
      ui::LayerOwner* target,
      std::unique_ptr<ui::LayerAnimationSequence> sequence);

 private:
  class Observer;

  AnimationSequenceBlock NewSequence(bool repeating);
  Observer* GetObserver();

  std::multimap<ui::LayerOwner*, std::unique_ptr<ui::LayerAnimationSequence>>
      layer_animation_sequences_;
  std::vector<AnimationSequence> animation_sequences_;
  std::unique_ptr<Observer> animation_observer_;
};
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_ANIMATION_BUILDER_H_
