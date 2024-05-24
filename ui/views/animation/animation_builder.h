// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_ANIMATION_BUILDER_H_
#define UI_VIEWS_ANIMATION_ANIMATION_BUILDER_H_

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/animation/animation_key.h"
#include "ui/views/animation/animation_sequence_block.h"
#include "ui/views/views_export.h"

namespace ui {
class Layer;
}

namespace views {

class AnimationAbortHandle;

// Provides an unfinalized animation sequence block if any to build animations.

// Usage notes for callbacks set on AnimationBuilder:
// When setting callbacks for the animations note that the AnimationBuilder’s
// observer that calls these callbacks may outlive the callback's parameters.

// The OnEnded callback runs when all animations created on the AnimationBuilder
// have finished. The OnAborted callback runs when any one animation created on
// the AnimationBuilder has been aborted. Therefore, these callbacks and every
// object the callback accesses needs to outlive all the Layers/LayerOwners
// being animated on since the Layers ultimately own the objects that run the
// animation. Otherwise, developers may need to use weak pointers or force
// animations to be cancelled in the object’s destructor to prevent accessing
// destroyed objects. Note that aborted notifications can be sent during the
// destruction process. Therefore subclasses that own the Layers may actually be
// destroyed before the OnAborted callback is run.

class VIEWS_EXPORT AnimationBuilder {
 public:
  class Observer : public ui::LayerAnimationObserver {
   public:
    Observer();
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    ~Observer() override;

    void SetOnStarted(base::OnceClosure callback);
    void SetOnEnded(base::OnceClosure callback, base::Location location);
    void SetOnWillRepeat(base::RepeatingClosure callback);
    void SetOnAborted(base::OnceClosure callback, base::Location location);
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

    bool GetAttachedToSequence() const { return attached_to_sequence_; }

   protected:
    void OnAttachedToSequence(ui::LayerAnimationSequence* sequence) override;
    void OnDetachedFromSequence(ui::LayerAnimationSequence* sequence) override;
    bool RequiresNotificationWhenAnimatorDestroyed() const override;

   private:
    using RepeatMap = base::flat_map<ui::LayerAnimationSequence*, int>;
    RepeatMap repeat_map_;
    base::OnceClosure on_started_;
    base::OnceClosure on_ended_;
    // Record where the on_ended_ callback was set from. Needed to debug a
    // bad callback crash (https://g-issues.chromium.org/issues/335902543).
    // TODO(b/335902543): Remove on_ended_location_.
    base::Location on_ended_location_;
    base::RepeatingClosure on_will_repeat_;
    base::OnceClosure on_aborted_;
    // Record where the on_aborted_ callback was set from. Needed to debug a
    // bad callback crash (https://g-issues.chromium.org/issues/335902543).
    // TODO(b/335902543): Remove on_aborted_location_.
    base::Location on_aborted_location_;
    base::OnceClosure on_scheduled_;

    bool attached_to_sequence_ = false;
    // Incremented when a sequence is attached and decremented when a sequence
    // ends. Does not account for aborted sequences. This provides a more
    // reliable way of tracking when all sequences have ended since IsFinished
    // can return true before a sequence is started if the duration is zero.
    int sequences_to_run_ = 0;
    raw_ptr<AnimationAbortHandle, DanglingUntriaged> abort_handle_ = nullptr;
  };

  AnimationBuilder();
  AnimationBuilder(AnimationBuilder&& rhs);
  AnimationBuilder& operator=(AnimationBuilder&& rhs);
  ~AnimationBuilder();

  // Options for the whole animation
  AnimationBuilder& SetPreemptionStrategy(
      ui::LayerAnimator::PreemptionStrategy preemption_strategy);
  // Registers |callback| to be called when the animation starts.
  // Must use before creating a sequence block.
  AnimationBuilder& OnStarted(base::OnceClosure callback);
  // Registers |callback| to be called when the animation ends. Not called if
  // animation is aborted.
  // Must use before creating a sequence block.
  AnimationBuilder& OnEnded(base::OnceClosure callback,
                            base::Location location = FROM_HERE);
  // Registers |callback| to be called when a sequence repetition ends and will
  // repeat. Not called if sequence is aborted.
  // Must use before creating a sequence block.
  AnimationBuilder& OnWillRepeat(base::RepeatingClosure callback);
  // Registers |callback| to be called if animation is aborted for any reason.
  // Should never do anything that may cause another animation to be started.
  // Must use before creating a sequence block.
  AnimationBuilder& OnAborted(base::OnceClosure callback,
                              base::Location location = FROM_HERE);
  // Registers |callback| to be called when the animation is scheduled.
  // Must use before creating a sequence block.
  AnimationBuilder& OnScheduled(base::OnceClosure callback);

  // Returns a handle that can be destroyed later to abort all running
  // animations. Must use before creating a sequence block.
  // Caveat: ALL properties will be aborted, including those not initiated
  // by the builder.
  std::unique_ptr<AnimationAbortHandle> GetAbortHandle();

  // Creates a new sequence (that optionally repeats).
  AnimationSequenceBlock& Once();
  AnimationSequenceBlock& Repeatedly();

  // Adds an animation element `element` for `key` at `start` to `values`.
  void AddLayerAnimationElement(
      base::PassKey<AnimationSequenceBlock>,
      AnimationKey key,
      base::TimeDelta start,
      base::TimeDelta original_duration,
      std::unique_ptr<ui::LayerAnimationElement> element);

  // Swaps `current_sequence_` with `new_sequence` and returns the old one.
  [[nodiscard]] std::unique_ptr<AnimationSequenceBlock> SwapCurrentSequence(
      base::PassKey<AnimationSequenceBlock>,
      std::unique_ptr<AnimationSequenceBlock> new_sequence);

  // Called when a block ends.  Ensures all animations in the sequence will run
  // until at least `end`.
  void BlockEndedAt(base::PassKey<AnimationSequenceBlock>, base::TimeDelta end);

  // Called when the sequence is ended. Converts `values_` to
  // `layer_animation_sequences_`.
  void TerminateSequence(base::PassKey<AnimationSequenceBlock>, bool repeating);

  // Returns a left value reference to the object held by `current_sequence_`.
  // Assumes that `current_sequence_` is set.
  // NOTE: be wary when keeping this method's return value because the current
  // sequence held by an `AnimationBuilder` instance could be destroyed during
  // `AnimationBuilder` instance's life cycle.
  AnimationSequenceBlock& GetCurrentSequence();

  static void SetObserverDeletedCallbackForTesting(
      base::RepeatingClosure deleted_closure);

 private:
  struct Value;

  Observer* GetObserver();

  // Resets data for the current sequence as necessary, creates a new sequence
  // block and returns the new block's left value reference.
  AnimationSequenceBlock& NewSequence(bool repeating);

  // Returns a reference to the observer deleted callback used for testing.
  static base::RepeatingClosure& GetObserverDeletedCallback();

  // Data for all sequences.
  std::multimap<ui::Layer*, std::unique_ptr<ui::LayerAnimationSequence>>
      layer_animation_sequences_;
  std::unique_ptr<Observer> animation_observer_;
  // Sets up observer callbacks before .Once() or .Repeatedly() is called to
  // start the sequence. next_animation_observer_ is moved to
  // animation_observer_ once .Once() or Repeatedly() is called.
  std::unique_ptr<Observer> next_animation_observer_;
  std::optional<ui::LayerAnimator::PreemptionStrategy> preemption_strategy_;

  // Data for the current sequence.
  base::TimeDelta end_;
  // Each vector is kept in sorted order.
  std::map<AnimationKey, std::vector<Value>> values_;

  raw_ptr<AnimationAbortHandle, DanglingUntriaged> abort_handle_ = nullptr;

  // An unfinalized sequence block currently used to build animations. NOTE: the
  // animation effects carried by `current_sequence_` attach to a layer only
  // after `current_sequence_` is destroyed.
  // The life cycle of `current_sequence_`:
  // (1) The old sequence is replaced by a new one. When being replaced, the
  // old sequence is destroyed.
  // (2) Gets destroyed when the host `AnimationBuilder` is destroyed.
  std::unique_ptr<AnimationSequenceBlock> current_sequence_;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_ANIMATION_BUILDER_H_
