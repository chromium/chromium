// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_CALLBACK_LAYER_ANIMATION_OBSERVER_H_
#define UI_COMPOSITOR_CALLBACK_LAYER_ANIMATION_OBSERVER_H_

#include "base/functional/callback.h"
#include "ui/compositor/compositor_export.h"
#include "ui/compositor/layer_animation_observer.h"

namespace ui {

class LayerAnimationSequence;

// A LayerAnimationObserver that invokes a Callback when all observed
// LayerAnimationSequence's have started and finished.
//
// Example usage:
//  class Foobar {
//    // The Callback that will be invoked when all the animation sequences have
//    // started.
//    void AnimationStartedCallback(
//        const CallbackLayerAnimationObserver& observer) {
//      // Do stuff.
//    }
//
//    // The Callback that will be invoked when all the animation sequences have
//    // finished.
//    bool AnimationEndedCallback(
//        const CallbackLayerAnimationObserver& observer) {
//      // Do stuff.
//      return true;  // Returns true so that |observer| destroys itself.
//    }
//
//    // Example method that uses the CallbackLayerAnimationObserver.
//    void Animate() {
//      ui::LayerAnimator* animator_1 = layer_1->GetAnimator();
//      ui::LayerAnimator* animator_2 = layer_2->GetAnimator();
//      CallbackLayerAnimationObserver* observer =
//          new CallbackLayerAnimationObserver(
//              base::BindRepeating(&Foobar::AnimationStartedCallback),
//              base::Unretained(this));
//              base::BindRepeating(&Foobar::AnimationEndedCallback),
//              base::Unretained(this));
//      animator_1->AddObserver(observer);
//      animator_2->AddObserver(observer);
//
//      // Set up animation sequences on |animator_1| and |animator_2|.
//
//      // The callback won't be invoked until SetActive() is called.
//      observer->SetActive();
//    }
//  }
//
// TODO(bruthig): Unify the CallbackLayerAnimationObserver with the
// ImplicitAnimationObserver. (See www.crbug.com/542825).
class COMPOSITOR_EXPORT CallbackLayerAnimationObserver
    : public ui::LayerAnimationObserver {
 public:
  // The Callback type that will be invoked when all animation sequences have
  // been started.
  using AnimationStartedCallback =
      base::RepeatingCallback<void(const CallbackLayerAnimationObserver&)>;

  // The Callback type that will be invoked when all animation sequences have
  // finished. |this| will be destroyed after invoking the Callback if it
  // returns true.
  using AnimationEndedCallback =
      base::RepeatingCallback<bool(const CallbackLayerAnimationObserver&)>;

  // A dummy no-op AnimationStartedCallback.
  static void DummyAnimationStartedCallback(
      const CallbackLayerAnimationObserver&);

  // A dummy no-op AnimationEndedCallback that will return
  // |should_delete_observer|.
  //
  // Example usage:
  //
  //    ui::CallbackLayerAnimationObserver* animation_observer =
  //        new ui::CallbackLayerAnimationObserver(
  //            base::BindRepeating(&ui::CallbackLayerAnimationObserver::
  //                DummyAnimationStartedCallback),
  //            base::BindRepeating(&ui::CallbackLayerAnimationObserver::
  //                DummyAnimationEndedCallback, false));
  static bool DummyAnimationEndedCallback(
      bool should_delete_observer,
      const CallbackLayerAnimationObserver&);

  // Create an instance that will invoke the |animation_started_callback| when
  // the animation has started and the |animation_ended_callback| when the
  // animation has ended.
  CallbackLayerAnimationObserver(
      AnimationStartedCallback animation_started_callback,
      AnimationEndedCallback animation_ended_callback);

  // Create an instance that will invoke the |animation_started_callback| when
  // the animation has started and will delete |this| when the animations have
  // ended if |should_delete_observer| is true.
  CallbackLayerAnimationObserver(
      AnimationStartedCallback animation_started_callback,
      bool should_delete_observer);

  // Create an instance that will invoke the |animation_ended_callback| when the
  // animations have ended.
  explicit CallbackLayerAnimationObserver(
      AnimationEndedCallback animation_ended_callback);

  CallbackLayerAnimationObserver(const CallbackLayerAnimationObserver&) =
      delete;
  CallbackLayerAnimationObserver& operator=(
      const CallbackLayerAnimationObserver&) = delete;

  ~CallbackLayerAnimationObserver() override;

  bool active() const { return active_; }

  // The callbacks will not be invoked until SetActive() has been called. This
  // allows each sequence to be attached before checking if the sequences have
  // finished.
  void SetActive();

  int aborted_count() const { return aborted_count_; }
  int successful_count() const { return successful_count_; }

  // ui::LayerAnimationObserver:
  void OnLayerAnimationStarted(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationScheduled(ui::LayerAnimationSequence* sequence) override;

 protected:
  // ui::LayerAnimationObserver:
  bool RequiresNotificationWhenAnimatorDestroyed() const override;
  void OnAttachedToSequence(ui::LayerAnimationSequence* sequence) override;
  void OnDetachedFromSequence(ui::LayerAnimationSequence* sequence) override;

 private:
  int GetNumSequencesCompleted();

  // Checks if all sequences have been started,
  // then if the started callback did not delete the observer,
  // checks if all sequences have completed.
  void CheckAllSequencesStartedAndCompleted();

  // Checks if all attached sequences have been started and invokes
  // |animation_started_callback_| if |active_| is true.
  void CheckAllSequencesStarted();

  // Checks if all attached sequences have completed and invokes
  // |animation_ended_callback_| if |active_| is true.
  void CheckAllSequencesCompleted();

  // Allows the callbacks to be invoked when true.
  bool active_ = false;

  // The total number of animation sequences that have been attached.
  int attached_sequence_count_ = 0;

  // The total number of animation sequences that have been detached.
  int detached_sequence_count_ = 0;

  // The number of animation sequences that have been started.
  int started_count_ = 0;

  // The number of animation sequences that were aborted.
  int aborted_count_ = 0;

  // The number of animation sequences that completed successfully.
  int successful_count_ = 0;

  // The callback to invoke once all the animation sequences have been started.
  AnimationStartedCallback animation_started_callback_;

  // The callback to invoke once all the animation sequences have finished.
  AnimationEndedCallback animation_ended_callback_;

  // Used to detect deletion while calling out.
  base::WeakPtrFactory<CallbackLayerAnimationObserver> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_COMPOSITOR_CALLBACK_LAYER_ANIMATION_OBSERVER_H_
