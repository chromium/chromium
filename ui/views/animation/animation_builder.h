// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_ANIMATION_BUILDER_H_
#define UI_VIEWS_ANIMATION_ANIMATION_BUILDER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/no_destructor.h"
#include "base/types/pass_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/animation/animation_key.h"
#include "ui/views/animation/animation_sequence_block.h"
#include "ui/views/views_export.h"

// This AnimationBuilder API is currently in the experimental phase and only
// used within ui/views/examples/.

namespace ui {
class Layer;
}

namespace views {

class AnimationAbortHandle;

class VIEWS_EXPORT AnimationBuilder {
 public:
  class Observer : public ui::LayerAnimationObserver {
   public:
    Observer();
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    ~Observer() override;

    void SetOnStarted(base::OnceClosure callback);
    void SetOnEnded(base::OnceClosure callback);
    void SetOnWillRepeat(base::RepeatingClosure callback);
    void SetOnAborted(base::OnceClosure callback);
    void SetOnScheduled(base::OnceClosure callback);

    void SetAbortHandle(AnimationAbortHandle* abort_handle);

    // ui::LayerAnimationObserver:
    void OnLayerAnimationStarted(ui::LayerAnimationSequence* sequence) override;
    void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override;
    void OnLayerAnimationWillRepeat(
        ui::LayerAnimationSequence* sequence) override;
    void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override;
    void OnLayerAnimationScheduled(
        ui::LayerAnimationSequence* sequence) override;

   protected:
    void OnDetachedFromSequence(ui::LayerAnimationSequence* sequence) override;
    bool RequiresNotificationWhenAnimatorDestroyed() const override;

   private:
    using RepeatMap = base::flat_map<ui::LayerAnimationSequence*, int>;
    RepeatMap repeat_map_;
    base::OnceClosure on_started_;
    base::OnceClosure on_ended_;
    base::RepeatingClosure on_will_repeat_;
    base::OnceClosure on_aborted_;
    base::OnceClosure on_scheduled_;

    AnimationAbortHandle* abort_handle_ = nullptr;
  };

  AnimationBuilder();
  ~AnimationBuilder();

  // Options for the whole animation
  AnimationBuilder& SetPreemptionStrategy(
      ui::LayerAnimator::PreemptionStrategy preemption_strategy);

  // Creates a new sequence (that optionally repeats).
  AnimationSequenceBlock Once();
  AnimationSequenceBlock Repeatedly();

  // Returns a handle that can be destroyed later to abort all running
  // animations.
  // Caveat: ALL properties will be aborted, including those not initiated
  // by the builder.
  std::unique_ptr<AnimationAbortHandle> GetAbortHandle();

  // Adds an animation element `element` for `key` at `start` to `values`.
  void AddLayerAnimationElement(
      base::PassKey<AnimationSequenceBlock>,
      AnimationKey key,
      base::TimeDelta start,
      base::TimeDelta original_duration,
      std::unique_ptr<ui::LayerAnimationElement> element);

  // Called when a block ends.  Ensures all animations in the sequence will run
  // until at least `end`.
  void BlockEndedAt(base::PassKey<AnimationSequenceBlock>, base::TimeDelta end);

  // Called when the sequence is ended. Converts `values_` to
  // `layer_animation_sequences_`.
  void TerminateSequence(base::PassKey<AnimationSequenceBlock>);

  // Called through the corresponding functions on AnimationSequenceBlock.
  void SetOnStarted(base::PassKey<AnimationSequenceBlock>,
                    base::OnceClosure callback);
  void SetOnEnded(base::PassKey<AnimationSequenceBlock>,
                  base::OnceClosure callback);
  void SetOnWillRepeat(base::PassKey<AnimationSequenceBlock>,
                       base::RepeatingClosure callback);
  void SetOnAborted(base::PassKey<AnimationSequenceBlock>,
                    base::OnceClosure callback);
  void SetOnScheduled(base::PassKey<AnimationSequenceBlock>,
                      base::OnceClosure callback);

  static void SetObserverDeletedCallbackForTesting(
      base::RepeatingClosure deleted_closure);

 private:
  struct Value;

  Observer* GetObserver();

  // Resets data for the current sequence as necessary, creates and returns the
  // initial block.
  AnimationSequenceBlock NewSequence();

  // Returns a reference to the observer deleted callback used for testing.
  static base::RepeatingClosure& GetObserverDeletedCallback();

  // Data for all sequences.
  std::multimap<ui::Layer*, std::unique_ptr<ui::LayerAnimationSequence>>
      layer_animation_sequences_;
  std::unique_ptr<Observer> animation_observer_;
  absl::optional<ui::LayerAnimator::PreemptionStrategy> preemption_strategy_;

  // Data for the current sequence.
  bool repeating_;
  base::TimeDelta end_;
  // Each vector is kept in sorted order.
  std::map<AnimationKey, std::vector<Value>> values_;

  AnimationAbortHandle* abort_handle_ = nullptr;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_ANIMATION_BUILDER_H_
