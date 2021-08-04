// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_ANIMATION_BUILDER_H_
#define UI_VIEWS_ANIMATION_ANIMATION_BUILDER_H_

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/scoped_observation.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace views {

// This AnimationBuilder API is currently in the experimental phase and only
// used within ui/views/examples/.
class VIEWS_EXPORT AnimationBuilder {
 public:
  AnimationBuilder();
  ~AnimationBuilder();

  AnimationBuilder& NewSequence();
  AnimationBuilder& EndSequence();

  AnimationBuilder& SetDuration(base::TimeDelta duration);

  // No effect if called before NewSequence();
  AnimationBuilder& Repeat();

  AnimationBuilder& Then();

  // These methods should be changed to OnSetXXX if we integrate with the View
  // base class.
  AnimationBuilder& SetOpacity(View* view, float target_opacity);
  AnimationBuilder& SetRoundedCorners(views::View* view,
                                      gfx::RoundedCornersF& rounded_corners);


  // Called when the animation starts.
  void OnStarted(base::OnceClosure callback);
  // Called when the animation ends. Not called if animation is aborted.
  void OnEnded(base::OnceClosure callback);
  // Called when a sequence repetition ends and will repeat. Not called if
  // sequence is aborted.
  void OnWillRepeat(base::RepeatingClosure callback);
  // Called if animation is aborted for any reason. Should never do anything
  // that may cause another animation to be started.
  void OnAborted(base::OnceClosure callback);
  // Called when the animation is scheduled.
  void OnScheduled(base::OnceClosure callback);

 private:
  // We may want to change this to our own struct.
  using AnimationKey =
      std::pair<View*, ui::LayerAnimationElement::AnimatableProperty>;

  class AnimationBuilderObserver : ui::LayerAnimationObserver {
   public:
    AnimationBuilderObserver();
    AnimationBuilderObserver(const AnimationBuilderObserver&) = delete;
    AnimationBuilderObserver& operator=(const AnimationBuilderObserver&) =
        delete;
    ~AnimationBuilderObserver() override;

    void ObserveAnimationSequence(ui::LayerAnimationSequence* sequence);

    void OnLayerAnimationStarted(ui::LayerAnimationSequence* sequence) override;
    void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override;
    void OnLayerAnimationWillRepeat(
        ui::LayerAnimationSequence* sequence) override;
    void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override;
    void OnLayerAnimationScheduled(
        ui::LayerAnimationSequence* sequence) override;

   private:
    void Reset();

    std::vector<base::WeakPtr<ui::LayerAnimationSequence>> sequences_;
  };

  void CreateNewEntry(const AnimationKey& key);

  void AddAnimation(const AnimationKey& key,
                    std::unique_ptr<ui::LayerAnimationElement> element);

  std::map<AnimationKey,
           std::vector<std::unique_ptr<ui::LayerAnimationSequence>>>
      animation_sequences_;

  base::TimeDelta duration_ = base::TimeDelta::FromSeconds(1);

  bool is_sequence_repeating_ = false;

  std::unique_ptr<AnimationBuilderObserver> animation_observer_;
};
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_ANIMATION_BUILDER_H_
