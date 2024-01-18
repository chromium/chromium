// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_RIPPLE_H_
#define UI_VIEWS_ANIMATION_INK_DROP_RIPPLE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop_ripple_observer.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/views_export.h"

namespace ui {
class CallbackLayerAnimationObserver;
class Layer;
class LayerAnimationObserver;
}  // namespace ui

namespace views {

class InkDropHost;

namespace test {
class InkDropRippleTestApi;
}  // namespace test

// Simple base class for animations that provide visual feedback for View state.
// Manages the attached InkDropRippleObservers.
//
// TODO(bruthig): Document the ink drop ripple on chromium.org and add a link to
// the doc here.
class VIEWS_EXPORT InkDropRipple {
 public:
  // The opacity of the ink drop when it is not visible.
  static const float kHiddenOpacity;

  explicit InkDropRipple(InkDropHost* ink_drop_host);
  InkDropRipple(const InkDropRipple&) = delete;
  InkDropRipple& operator=(const InkDropRipple&) = delete;
  virtual ~InkDropRipple();

  // In the event that an animation is in progress for ink drop state 's1' and
  // an animation to a new state 's2' is triggered, then
  // AnimationEnded(s1, PRE_EMPTED) will be called before
  // AnimationStarted(s2).
  void set_observer(InkDropRippleObserver* observer) { observer_ = observer; }

  // Animates from the current InkDropState to the new |ink_drop_state|.
  //
  // NOTE: GetTargetInkDropState() should return the new |ink_drop_state| value
  // to any observers being notified as a result of the call.
  void AnimateToState(InkDropState ink_drop_state);

  // Snaps from the current InkDropState to the new |ink_drop_state|.
  void SnapToState(InkDropState ink_drop_state);

  InkDropState target_ink_drop_state() const { return target_ink_drop_state_; }

  // Immediately aborts all in-progress animations and hides the ink drop.
  //
  // NOTE: This will NOT raise Animation(Started|Ended) events for the state
  // transition to HIDDEN!
  void SnapToHidden();

  // Immediately snaps the ink drop to the ACTIVATED target state. All pending
  // animations are aborted. Events will be raised for the pending animations
  // as well as the transition to the ACTIVATED state.
  virtual void SnapToActivated();

  // The root Layer that can be added in to a Layer tree.
  virtual ui::Layer* GetRootLayer() = 0;

  // Returns true when the ripple is visible. This is different from checking if
  // the ink_drop_state() == HIDDEN because the ripple may be visible while it
  // animates to the target HIDDEN state.
  bool IsVisible();

  // Returns a test api to access internals of this. Default implmentations
  // should return nullptr and test specific subclasses can override to return
  // an instance.
  virtual test::InkDropRippleTestApi* GetTestApi();

 protected:
  // Animates the ripple from the |old_ink_drop_state| to the
  // |new_ink_drop_state|. |observer| is added to all LayerAnimationSequence's
  // used if not null.
  virtual void AnimateStateChange(InkDropState old_ink_drop_state,
                                  InkDropState new_ink_drop_state) = 0;

  // Updates the transforms, opacity, and visibility to a ACTIVATED state.
  virtual void SetStateToActivated() = 0;

  // Updates the transforms, opacity, and visibility to a HIDDEN state.
  virtual void SetStateToHidden() = 0;

  virtual void AbortAllAnimations() = 0;

  // Get the current observer. CreateAnimationObserver must have already been
  // called.
  ui::LayerAnimationObserver* GetLayerAnimationObserver();

  // Get the InkDropHost associated this ripple.
  InkDropHost* GetInkDropHost() const;

 private:
  // The Callback invoked when all of the animation sequences for the specific
  // |ink_drop_state| animation have started. |observer| is the
  // ui::CallbackLayerAnimationObserver which is notifying the callback.
  void AnimationStartedCallback(
      InkDropState ink_drop_state,
      const ui::CallbackLayerAnimationObserver& observer);

  // The Callback invoked when all of the animation sequences for the specific
  // |ink_drop_state| animation have finished. |observer| is the
  // ui::CallbackLayerAnimationObserver which is notifying the callback.
  bool AnimationEndedCallback(
      InkDropState ink_drop_state,
      const ui::CallbackLayerAnimationObserver& observer);

  // Creates a new animation observer bound to AnimationStartedCallback() and
  // AnimationEndedCallback().
  std::unique_ptr<ui::CallbackLayerAnimationObserver> CreateAnimationObserver(
      InkDropState ink_drop_state);

  // The target InkDropState.
  InkDropState target_ink_drop_state_ = InkDropState::HIDDEN;

  raw_ptr<InkDropRippleObserver> observer_ = nullptr;

  std::unique_ptr<ui::CallbackLayerAnimationObserver> animation_observer_;

  // Reference to the host on which this ripple resides.
  raw_ptr<InkDropHost> ink_drop_host_ = nullptr;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_RIPPLE_H_
