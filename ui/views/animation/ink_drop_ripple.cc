// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_ripple.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer.h"

namespace views {

const float InkDropRipple::kHiddenOpacity = 0.f;

InkDropRipple::InkDropRipple() = default;

InkDropRipple::~InkDropRipple() = default;

void InkDropRipple::AnimateToState(InkDropState ink_drop_state) {
  // Does not return early if |target_ink_drop_state_| == |ink_drop_state| for
  // two reasons.
  // 1. The attached observers must be notified of all animations started and
  // ended.
  // 2. Not all state transitions is are valid, especially no-op transitions,
  // and these invalid transitions will be logged as warnings in
  // AnimateStateChange().

  animation_observer_ = CreateAnimationObserver(ink_drop_state);

  InkDropState old_ink_drop_state = target_ink_drop_state_;
  // Assign to |target_ink_drop_state_| before calling AnimateStateChange() so
  // that any observers notified as a side effect of the AnimateStateChange()
  // will get the target InkDropState when calling GetInkDropState().
  target_ink_drop_state_ = ink_drop_state;

  if (old_ink_drop_state == InkDropState::HIDDEN &&
      target_ink_drop_state_ != InkDropState::HIDDEN) {
    GetRootLayer()->SetVisible(true);
  }

  AnimateStateChange(old_ink_drop_state, target_ink_drop_state_,
                     animation_observer_.get());
  animation_observer_->SetActive();
  // |this| may be deleted! |animation_observer_| might synchronously call
  // AnimationEndedCallback which can delete |this|.
}

void InkDropRipple::SnapToState(InkDropState ink_drop_state) {
  AbortAllAnimations();
  if (ink_drop_state == InkDropState::ACTIVATED)
    GetRootLayer()->SetVisible(true);
  else if (ink_drop_state == InkDropState::HIDDEN)
    SetStateToHidden();
  target_ink_drop_state_ = ink_drop_state;
  animation_observer_ = CreateAnimationObserver(ink_drop_state);
  animation_observer_->SetActive();
  // |this| may be deleted! |animation_observer_| might synchronously call
  // AnimationEndedCallback which can delete |this|.
}

void InkDropRipple::SnapToActivated() {
  SnapToState(InkDropState::ACTIVATED);
}

bool InkDropRipple::IsVisible() {
  return GetRootLayer()->visible();
}

void InkDropRipple::SnapToHidden() {
  SnapToState(InkDropState::HIDDEN);
}

test::InkDropRippleTestApi* InkDropRipple::GetTestApi() {
  return nullptr;
}

void InkDropRipple::AnimationStartedCallback(
    InkDropState ink_drop_state,
    const ui::CallbackLayerAnimationObserver& observer) {
  if (observer_)
    observer_->AnimationStarted(ink_drop_state);
}

bool InkDropRipple::AnimationEndedCallback(
    InkDropState ink_drop_state,
    const ui::CallbackLayerAnimationObserver& observer) {
  if (ink_drop_state == InkDropState::HIDDEN)
    SetStateToHidden();
  if (observer_)
    observer_->AnimationEnded(ink_drop_state,
                              observer.aborted_count()
                                  ? InkDropAnimationEndedReason::PRE_EMPTED
                                  : InkDropAnimationEndedReason::SUCCESS);
  // |this| may be deleted!
  return false;
}

std::unique_ptr<ui::CallbackLayerAnimationObserver>
InkDropRipple::CreateAnimationObserver(InkDropState ink_drop_state) {
  return std::make_unique<ui::CallbackLayerAnimationObserver>(
      base::BindRepeating(&InkDropRipple::AnimationStartedCallback,
                          base::Unretained(this), ink_drop_state),
      base::BindRepeating(&InkDropRipple::AnimationEndedCallback,
                          base::Unretained(this), ink_drop_state));
}

}  // namespace views
