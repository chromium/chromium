// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_impl.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/timer/timer.h"
#include "ui/compositor/layer.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/ink_drop_util.h"
#include "ui/views/animation/square_ink_drop_ripple.h"
#include "ui/views/style/platform_style.h"

namespace views {

namespace {

// The duration for the highlight state fade in/out animations when they are
// triggered by a hover changed event.
constexpr auto kHighlightFadeInOnHoverChangeDuration = base::Milliseconds(250);
constexpr auto kHighlightFadeOutOnHoverChangeDuration = base::Milliseconds(250);

// The duration for the highlight state fade in/out animations when they are
// triggered by a focus changed event.
constexpr auto kHighlightFadeInOnFocusChangeDuration = base::TimeDelta();
constexpr auto kHighlightFadeOutOnFocusChangeDuration = base::TimeDelta();

// The duration for showing/hiding the highlight when triggered by ripple
// visibility changes for the HIDE_ON_RIPPLE AutoHighlightMode.
constexpr auto kHighlightFadeInOnRippleHidingDuration = base::Milliseconds(250);
constexpr auto kHighlightFadeOutOnRippleShowingDuration =
    base::Milliseconds(120);

// The duration for showing/hiding the highlight when triggered by ripple
// visibility changes for the SHOW_ON_RIPPLE AutoHighlightMode.
constexpr auto kHighlightFadeInOnRippleShowingDuration =
    base::Milliseconds(250);
constexpr auto kHighlightFadeOutOnRippleHidingDuration =
    base::Milliseconds(120);

// The amount of time that |highlight_| should delay after a ripple animation
// before fading in, for highlight due to mouse hover.
constexpr auto kHoverFadeInAfterRippleDelay = base::Milliseconds(1000);

// Returns true if an ink drop with the given |ink_drop_state| should
// automatically transition to the InkDropState::HIDDEN state.
bool ShouldAnimateToHidden(InkDropState ink_drop_state) {
  switch (ink_drop_state) {
    case views::InkDropState::ACTION_TRIGGERED:
    case views::InkDropState::ALTERNATE_ACTION_TRIGGERED:
    case views::InkDropState::DEACTIVATED:
      return true;
    default:
      return false;
  }
}

}  // namespace

// HighlightState definition

InkDropImpl* InkDropImpl::HighlightState::GetInkDrop() {
  return state_factory_->ink_drop();
}

// A HighlightState to be used during InkDropImpl destruction. All event
// handlers are no-ops so as to avoid triggering animations during tear down.
class InkDropImpl::DestroyingHighlightState
    : public InkDropImpl::HighlightState {
 public:
  DestroyingHighlightState() : HighlightState(nullptr) {}

  DestroyingHighlightState(const DestroyingHighlightState&) = delete;
  DestroyingHighlightState& operator=(const DestroyingHighlightState&) = delete;

  // InkDropImpl::HighlightState:
  void Enter() override {}
  void ShowOnHoverChanged() override {}
  void OnHoverChanged() override {}
  void ShowOnFocusChanged() override {}
  void OnFocusChanged() override {}
  void AnimationStarted(InkDropState ink_drop_state) override {}
  void AnimationEnded(InkDropState ink_drop_state,
                      InkDropAnimationEndedReason reason) override {}
};

//
// AutoHighlightMode::NONE states
//

// Animates the highlight to hidden upon entering this state. Transitions to a
// visible state based on hover/focus changes.
class InkDropImpl::NoAutoHighlightHiddenState
    : public InkDropImpl::HighlightState {
 public:
  NoAutoHighlightHiddenState(HighlightStateFactory* state_factory,
                             base::TimeDelta animation_duration);

  NoAutoHighlightHiddenState(const NoAutoHighlightHiddenState&) = delete;
  NoAutoHighlightHiddenState& operator=(const NoAutoHighlightHiddenState&) =
      delete;

  // InkDropImpl::HighlightState:
  void Enter() override;
  void ShowOnHoverChanged() override;
  void OnHoverChanged() override;
  void ShowOnFocusChanged() override;
  void OnFocusChanged() override;
  void AnimationStarted(InkDropState ink_drop_state) override;
  void AnimationEnded(InkDropState ink_drop_state,
                      InkDropAnimationEndedReason reason) override;

 private:
  // Handles all changes to the hover/focus status and transitions to a visible
  // state if necessary.
  void HandleHoverAndFocusChangeChanges(base::TimeDelta animation_duration);

  // The fade out animation duration.
  base::TimeDelta animation_duration_;
};

// Animates the highlight to visible upon entering this state. Transitions to a
// hidden state based on hover/focus changes.
class InkDropImpl::NoAutoHighlightVisibleState
    : public InkDropImpl::HighlightState {
 public:
  NoAutoHighlightVisibleState(HighlightStateFactory* state_factory,
                              base::TimeDelta animation_duration);

  NoAutoHighlightVisibleState(const NoAutoHighlightVisibleState&) = delete;
  NoAutoHighlightVisibleState& operator=(const NoAutoHighlightVisibleState&) =
      delete;

  // InkDropImpl::HighlightState:
  void Enter() override;
  void Exit() override {}
  void ShowOnHoverChanged() override;
  void OnHoverChanged() override;
  void ShowOnFocusChanged() override;
  void OnFocusChanged() override;
  void AnimationStarted(InkDropState ink_drop_state) override;
  void AnimationEnded(InkDropState ink_drop_state,
                      InkDropAnimationEndedReason reason) override;

 private:
  // Handles all changes to the hover/focus status and transitions to a hidden
  // state if necessary.
  void HandleHoverAndFocusChangeChanges(base::TimeDelta animation_duration);

  // The fade in animation duration.
  base::TimeDelta animation_duration_;
};

// NoAutoHighlightHiddenState definition

InkDropImpl::NoAutoHighlightHiddenState::NoAutoHighlightHiddenState(
    HighlightStateFactory* state_factory,
    base::TimeDelta animation_duration)
    : InkDropImpl::HighlightState(state_factory),
      animation_duration_(animation_duration) {}

void InkDropImpl::NoAutoHighlightHiddenState::Enter() {
  GetInkDrop()->SetHighlight(false, animation_duration_);
}

void InkDropImpl::NoAutoHighlightHiddenState::ShowOnHoverChanged() {
  HandleHoverAndFocusChangeChanges(
      GetInkDrop()->hover_highlight_fade_duration().value_or(
          kHighlightFadeInOnHoverChangeDuration));
}

void InkDropImpl::NoAutoHighlightHiddenState::OnHoverChanged() {
  HandleHoverAndFocusChangeChanges(
      GetInkDrop()->hover_highlight_fade_duration().value_or(
          kHighlightFadeInOnHoverChangeDuration));
}

void InkDropImpl::NoAutoHighlightHiddenState::ShowOnFocusChanged() {
  HandleHoverAndFocusChangeChanges(kHighlightFadeInOnFocusChangeDuration);
}

void InkDropImpl::NoAutoHighlightHiddenState::OnFocusChanged() {
  HandleHoverAndFocusChangeChanges(kHighlightFadeInOnFocusChangeDuration);
}

void InkDropImpl::NoAutoHighlightHiddenState::HandleHoverAndFocusChangeChanges(
    base::TimeDelta animation_duration) {
  if (GetInkDrop()->ShouldHighlight()) {
    GetInkDrop()->SetHighlightState(
        state_factory()->CreateVisibleState(animation_duration));
  }
}

void InkDropImpl::NoAutoHighlightHiddenState::AnimationStarted(
    InkDropState ink_drop_state) {}

void InkDropImpl::NoAutoHighlightHiddenState::AnimationEnded(
    InkDropState ink_drop_state,
    InkDropAnimationEndedReason reason) {}

// NoAutoHighlightVisibleState definition

InkDropImpl::NoAutoHighlightVisibleState::NoAutoHighlightVisibleState(
    HighlightStateFactory* state_factory,
    base::TimeDelta animation_duration)
    : InkDropImpl::HighlightState(state_factory),
      animation_duration_(animation_duration) {}

void InkDropImpl::NoAutoHighlightVisibleState::Enter() {
  GetInkDrop()->SetHighlight(true, animation_duration_);
}

void InkDropImpl::NoAutoHighlightVisibleState::ShowOnHoverChanged() {
  HandleHoverAndFocusChangeChanges(
      GetInkDrop()->hover_highlight_fade_duration().value_or(
          kHighlightFadeOutOnHoverChangeDuration));
}

void InkDropImpl::NoAutoHighlightVisibleState::OnHoverChanged() {
  HandleHoverAndFocusChangeChanges(
      GetInkDrop()->hover_highlight_fade_duration().value_or(
          kHighlightFadeOutOnHoverChangeDuration));
}

void InkDropImpl::NoAutoHighlightVisibleState::ShowOnFocusChanged() {
  HandleHoverAndFocusChangeChanges(kHighlightFadeOutOnFocusChangeDuration);
}

void InkDropImpl::NoAutoHighlightVisibleState::OnFocusChanged() {
  HandleHoverAndFocusChangeChanges(kHighlightFadeOutOnFocusChangeDuration);
}

void InkDropImpl::NoAutoHighlightVisibleState::HandleHoverAndFocusChangeChanges(
    base::TimeDelta animation_duration) {
  if (!GetInkDrop()->ShouldHighlight()) {
    GetInkDrop()->SetHighlightState(
        state_factory()->CreateHiddenState(animation_duration));
  }
}

void InkDropImpl::NoAutoHighlightVisibleState::AnimationStarted(
    InkDropState ink_drop_state) {}

void InkDropImpl::NoAutoHighlightVisibleState::AnimationEnded(
    InkDropState ink_drop_state,
    InkDropAnimationEndedReason reason) {}

//
// AutoHighlightMode::HIDE_ON_RIPPLE states
//

// Extends the base hidden state to re-show the highlight after the ripple
// becomes hidden.
class InkDropImpl::HideHighlightOnRippleHiddenState
    : public InkDropImpl::NoAutoHighlightHiddenState {
 public:
  HideHighlightOnRippleHiddenState(HighlightStateFactory* state_factory,
                                   base::TimeDelta animation_duration);

  HideHighlightOnRippleHiddenState(const HideHighlightOnRippleHiddenState&) =
      delete;
  HideHighlightOnRippleHiddenState& operator=(
      const HideHighlightOnRippleHiddenState&) = delete;

  // InkDropImpl::NoAutoHighlightHiddenState:
  void ShowOnHoverChanged() override;
  void OnHoverChanged() override;
  void ShowOnFocusChanged() override;
  void OnFocusChanged() override;
  void AnimationStarted(InkDropState ink_drop_state) override;
  void AnimationEnded(InkDropState ink_drop_state,
                      InkDropAnimationEndedReason reason) override;

 private:
  // Starts the |highlight_after_ripple_timer_|. This will stop the current
  // |highlight_after_ripple_timer_| instance if it exists.
  void StartHighlightAfterRippleTimer();

  // Callback for when the |highlight_after_ripple_timer_| fires. Transitions to
  // a visible state if the ink drop should be highlighted.
  void HighlightAfterRippleTimerFired();

  // The timer used to delay the highlight fade in after an ink drop ripple
  // animation.
  std::unique_ptr<base::OneShotTimer> highlight_after_ripple_timer_;
};

// Extends the base visible state to hide the highlight when the ripple becomes
// visible.
class InkDropImpl::HideHighlightOnRippleVisibleState
    : public InkDropImpl::NoAutoHighlightVisibleState {
 public:
  HideHighlightOnRippleVisibleState(HighlightStateFactory* state_factory,
                                    base::TimeDelta animation_duration);

  HideHighlightOnRippleVisibleState(const HideHighlightOnRippleVisibleState&) =
      delete;
  HideHighlightOnRippleVisibleState& operator=(
      const HideHighlightOnRippleVisibleState&) = delete;

  // InkDropImpl::NoAutoHighlightVisibleState:
  void AnimationStarted(InkDropState ink_drop_state) override;
};

// HideHighlightOnRippleHiddenState definition

InkDropImpl::HideHighlightOnRippleHiddenState::HideHighlightOnRippleHiddenState(
    HighlightStateFactory* state_factory,
    base::TimeDelta animation_duration)
    : InkDropImpl::NoAutoHighlightHiddenState(state_factory,
                                              animation_duration),
      highlight_after_ripple_timer_(nullptr) {}

void InkDropImpl::HideHighlightOnRippleHiddenState::ShowOnHoverChanged() {
  if (GetInkDrop()->GetTargetInkDropState() != InkDropState::HIDDEN)
    return;
  NoAutoHighlightHiddenState::ShowOnHoverChanged();
}

void InkDropImpl::HideHighlightOnRippleHiddenState::OnHoverChanged() {
  if (GetInkDrop()->GetTargetInkDropState() != InkDropState::HIDDEN)
    return;
  NoAutoHighlightHiddenState::OnHoverChanged();
}

void InkDropImpl::HideHighlightOnRippleHiddenState::ShowOnFocusChanged() {
  if (GetInkDrop()->GetTargetInkDropState() != InkDropState::HIDDEN)
    return;
  NoAutoHighlightHiddenState::ShowOnFocusChanged();
}

void InkDropImpl::HideHighlightOnRippleHiddenState::OnFocusChanged() {
  if (GetInkDrop()->GetTargetInkDropState() != InkDropState::HIDDEN)
    return;
  NoAutoHighlightHiddenState::OnFocusChanged();
}

void InkDropImpl::HideHighlightOnRippleHiddenState::AnimationStarted(
    InkDropState ink_drop_state) {
  if (ink_drop_state == views::InkDropState::DEACTIVATED &&
      GetInkDrop()->ShouldHighlightBasedOnFocus()) {
    // It's possible to get animation started events when destroying the
    // |ink_drop_ripple_|.
    // TODO(bruthig): Investigate if the animation framework can address this
    // issue instead. See https://crbug.com/663335.
    InkDropImpl* ink_drop = GetInkDrop();
    HighlightStateFactory* highlight_state_factory = state_factory();
    if (ink_drop->ink_drop_ripple_)
      ink_drop->ink_drop_ripple_->SnapToHidden();
    // |this| may be destroyed after SnapToHidden(), so be sure not to access
    // |any members.
    ink_drop->SetHighlightState(
        highlight_state_factory->CreateVisibleState(base::TimeDelta()));
  }
}

void InkDropImpl::HideHighlightOnRippleHiddenState::AnimationEnded(
    InkDropState ink_drop_state,
    InkDropAnimationEndedReason reason) {
  if (ink_drop_state == InkDropState::HIDDEN) {
    // Re-highlight, as necessary. For hover, there's a delay; for focus, jump
    // straight into the animation.
    if (GetInkDrop()->ShouldHighlightBasedOnFocus()) {
      GetInkDrop()->SetHighlightState(
          state_factory()->CreateVisibleState(base::TimeDelta()));
      return;
    } else {
      StartHighlightAfterRippleTimer();
    }
  }
}

void InkDropImpl::HideHighlightOnRippleHiddenState::
    StartHighlightAfterRippleTimer() {
  highlight_after_ripple_timer_ = std::make_unique<base::OneShotTimer>();
  highlight_after_ripple_timer_->Start(
      FROM_HERE, kHoverFadeInAfterRippleDelay,
      base::BindOnce(&InkDropImpl::HideHighlightOnRippleHiddenState::
                         HighlightAfterRippleTimerFired,
                     base::Unretained(this)));
}

void InkDropImpl::HideHighlightOnRippleHiddenState::
    HighlightAfterRippleTimerFired() {
  highlight_after_ripple_timer_.reset();
  if (GetInkDrop()->GetTargetInkDropState() == InkDropState::HIDDEN &&
      GetInkDrop()->ShouldHighlight()) {
    GetInkDrop()->SetHighlightState(state_factory()->CreateVisibleState(
        kHighlightFadeInOnRippleHidingDuration));
  }
}

// HideHighlightOnRippleVisibleState definition

InkDropImpl::HideHighlightOnRippleVisibleState::
    HideHighlightOnRippleVisibleState(HighlightStateFactory* state_factory,
                                      base::TimeDelta animation_duration)
    : InkDropImpl::NoAutoHighlightVisibleState(state_factory,
                                               animation_duration) {}

void InkDropImpl::HideHighlightOnRippleVisibleState::AnimationStarted(
    InkDropState ink_drop_state) {
  if (ink_drop_state != InkDropState::HIDDEN) {
    GetInkDrop()->SetHighlightState(state_factory()->CreateHiddenState(
        kHighlightFadeOutOnRippleShowingDuration));
  }
}

//
// AutoHighlightMode::SHOW_ON_RIPPLE states
//

// Extends the base hidden state to show the highlight when the ripple becomes
// visible.
class InkDropImpl::ShowHighlightOnRippleHiddenState
    : public InkDropImpl::NoAutoHighlightHiddenState {
 public:
  ShowHighlightOnRippleHiddenState(HighlightStateFactory* state_factory,
                                   base::TimeDelta animation_duration);

  ShowHighlightOnRippleHiddenState(const ShowHighlightOnRippleHiddenState&) =
      delete;
  ShowHighlightOnRippleHiddenState& operator=(
      const ShowHighlightOnRippleHiddenState&) = delete;

  // InkDropImpl::NoAutoHighlightHiddenState:
  void AnimationStarted(InkDropState ink_drop_state) override;
};

// Extends the base visible state to hide the highlight when the ripple becomes
// hidden.
class InkDropImpl::ShowHighlightOnRippleVisibleState
    : public InkDropImpl::NoAutoHighlightVisibleState {
 public:
  ShowHighlightOnRippleVisibleState(HighlightStateFactory* state_factory,
                                    base::TimeDelta animation_duration);

  ShowHighlightOnRippleVisibleState(const ShowHighlightOnRippleVisibleState&) =
      delete;
  ShowHighlightOnRippleVisibleState& operator=(
      const ShowHighlightOnRippleVisibleState&) = delete;

  // InkDropImpl::NoAutoHighlightVisibleState:
  void ShowOnHoverChanged() override;
  void OnHoverChanged() override;
  void ShowOnFocusChanged() override;
  void OnFocusChanged() override;
  void AnimationStarted(InkDropState ink_drop_state) override;
};

// ShowHighlightOnRippleHiddenState definition

InkDropImpl::ShowHighlightOnRippleHiddenState::ShowHighlightOnRippleHiddenState(
    HighlightStateFactory* state_factory,
    base::TimeDelta animation_duration)
    : InkDropImpl::NoAutoHighlightHiddenState(state_factory,
                                              animation_duration) {}

void InkDropImpl::ShowHighlightOnRippleHiddenState::AnimationStarted(
    InkDropState ink_drop_state) {
  if (ink_drop_state != views::InkDropState::HIDDEN) {
    GetInkDrop()->SetHighlightState(state_factory()->CreateVisibleState(
        kHighlightFadeInOnRippleShowingDuration));
  }
}

// ShowHighlightOnRippleVisibleState definition

InkDropImpl::ShowHighlightOnRippleVisibleState::
    ShowHighlightOnRippleVisibleState(HighlightStateFactory* state_factory,
                                      base::TimeDelta animation_duration)
    : InkDropImpl::NoAutoHighlightVisibleState(state_factory,
                                               animation_duration) {}

void InkDropImpl::ShowHighlightOnRippleVisibleState::ShowOnHoverChanged() {
  if (GetInkDrop()->GetTargetInkDropState() != InkDropState::HIDDEN)
    return;
  NoAutoHighlightVisibleState::ShowOnHoverChanged();
}

void InkDropImpl::ShowHighlightOnRippleVisibleState::OnHoverChanged() {
  if (GetInkDrop()->GetTargetInkDropState() != InkDropState::HIDDEN)
    return;
  NoAutoHighlightVisibleState::OnHoverChanged();
}

void InkDropImpl::ShowHighlightOnRippleVisibleState::ShowOnFocusChanged() {
  if (GetInkDrop()->GetTargetInkDropState() != InkDropState::HIDDEN)
    return;
  NoAutoHighlightVisibleState::ShowOnFocusChanged();
}

void InkDropImpl::ShowHighlightOnRippleVisibleState::OnFocusChanged() {
  if (GetInkDrop()->GetTargetInkDropState() != InkDropState::HIDDEN)
    return;
  NoAutoHighlightVisibleState::OnFocusChanged();
}

void InkDropImpl::ShowHighlightOnRippleVisibleState::AnimationStarted(
    InkDropState ink_drop_state) {
  if (ink_drop_state == InkDropState::HIDDEN &&
      !GetInkDrop()->ShouldHighlight()) {
    GetInkDrop()->SetHighlightState(state_factory()->CreateHiddenState(
        kHighlightFadeOutOnRippleHidingDuration));
  }
}

InkDropImpl::HighlightStateFactory::HighlightStateFactory(
    InkDropImpl::AutoHighlightMode highlight_mode,
    InkDropImpl* ink_drop)
    : highlight_mode_(highlight_mode), ink_drop_(ink_drop) {}

std::unique_ptr<InkDropImpl::HighlightState>
InkDropImpl::HighlightStateFactory::CreateStartState() {
  switch (highlight_mode_) {
    case InkDropImpl::AutoHighlightMode::NONE:
      return std::make_unique<NoAutoHighlightHiddenState>(this,
                                                          base::TimeDelta());
    case InkDropImpl::AutoHighlightMode::HIDE_ON_RIPPLE:
      return std::make_unique<HideHighlightOnRippleHiddenState>(
          this, base::TimeDelta());
    case InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE:
      return std::make_unique<ShowHighlightOnRippleHiddenState>(
          this, base::TimeDelta());
  }
  NOTREACHED();
}

std::unique_ptr<InkDropImpl::HighlightState>
InkDropImpl::HighlightStateFactory::CreateHiddenState(
    base::TimeDelta animation_duration) {
  switch (highlight_mode_) {
    case InkDropImpl::AutoHighlightMode::NONE:
      return std::make_unique<NoAutoHighlightHiddenState>(this,
                                                          animation_duration);
    case InkDropImpl::AutoHighlightMode::HIDE_ON_RIPPLE:
      return std::make_unique<HideHighlightOnRippleHiddenState>(
          this, animation_duration);
    case InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE:
      return std::make_unique<ShowHighlightOnRippleHiddenState>(
          this, animation_duration);
  }
  // Required for some compilers.
  NOTREACHED();
}

std::unique_ptr<InkDropImpl::HighlightState>
InkDropImpl::HighlightStateFactory::CreateVisibleState(
    base::TimeDelta animation_duration) {
  switch (highlight_mode_) {
    case InkDropImpl::AutoHighlightMode::NONE:
      return std::make_unique<NoAutoHighlightVisibleState>(this,
                                                           animation_duration);
    case InkDropImpl::AutoHighlightMode::HIDE_ON_RIPPLE:
      return std::make_unique<HideHighlightOnRippleVisibleState>(
          this, animation_duration);
    case InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE:
      return std::make_unique<ShowHighlightOnRippleVisibleState>(
          this, animation_duration);
  }
  // Required for some compilers.
  NOTREACHED();
}

InkDropImpl::InkDropImpl(InkDropHost* ink_drop_host,
                         const gfx::Size& host_size,
                         AutoHighlightMode auto_highlight_mode)
    : ink_drop_host_(ink_drop_host),
      highlight_state_factory_(auto_highlight_mode, this),
      root_layer_(new ui::Layer(ui::LAYER_NOT_DRAWN)) {
  root_layer_->SetBounds(gfx::Rect(host_size));
  root_layer_->SetName("InkDropImpl:RootLayer");
  SetHighlightState(highlight_state_factory_.CreateStartState());
}

InkDropImpl::~InkDropImpl() {
  destroying_ = true;
  // Setting a no-op state prevents animations from being triggered on a null
  // |ink_drop_ripple_| as a side effect of the tear down.
  SetHighlightState(std::make_unique<DestroyingHighlightState>());

  // Explicitly destroy the InkDropRipple so that this still exists if
  // views::InkDropRippleObserver methods are called on this.
  DestroyInkDropRipple();
  DestroyInkDropHighlight();
}

void InkDropImpl::HostSizeChanged(const gfx::Size& new_size) {
  // |root_layer_| should fill the entire host because it affects the clipping
  // when a mask layer is applied to it. This will not affect clipping if no
  // mask layer is set.
  root_layer_->SetBounds(gfx::Rect(new_size) +
                         root_layer_->bounds().OffsetFromOrigin());
  RecreateRippleAndHighlight();
}

void InkDropImpl::HostViewThemeChanged() {
  RecreateRippleAndHighlight();
}

void InkDropImpl::HostTransformChanged(const gfx::Transform& new_transform) {
  // If the host has a transform applied, the root and its children layers
  // should be affected too.
  root_layer_->SetTransform(new_transform);
}

InkDropState InkDropImpl::GetTargetInkDropState() const {
  if (!ink_drop_ripple_)
    return InkDropState::HIDDEN;
  return ink_drop_ripple_->target_ink_drop_state();
}

void InkDropImpl::AnimateToState(InkDropState ink_drop_state) {
  // Never animate hidden -> hidden, since that will add layers which may never
  // be needed. Other same-state transitions may restart animations.
  if (ink_drop_state == InkDropState::HIDDEN &&
      GetTargetInkDropState() == InkDropState::HIDDEN)
    return;

  DestroyHiddenTargetedAnimations();
  if (!ink_drop_ripple_)
    CreateInkDropRipple();
  ink_drop_ripple_->AnimateToState(ink_drop_state);
}

void InkDropImpl::SetHoverHighlightFadeDuration(base::TimeDelta duration) {
  hover_highlight_fade_duration_ = duration;
}

void InkDropImpl::UseDefaultHoverHighlightFadeDuration() {
  hover_highlight_fade_duration_.reset();
}

void InkDropImpl::SnapToActivated() {
  DestroyHiddenTargetedAnimations();
  if (!ink_drop_ripple_)
    CreateInkDropRipple();
  ink_drop_ripple_->SnapToActivated();
}

void InkDropImpl::SnapToHidden() {
  DestroyHiddenTargetedAnimations();
  if (!ink_drop_ripple_)
    return;
  ink_drop_ripple_->SnapToHidden();
}

void InkDropImpl::SetHovered(bool is_hovered) {
  is_hovered_ = is_hovered;
  highlight_state_->OnHoverChanged();
}

void InkDropImpl::SetFocused(bool is_focused) {
  is_focused_ = is_focused;
  highlight_state_->OnFocusChanged();
}

bool InkDropImpl::IsHighlightFadingInOrVisible() const {
  return highlight_ && highlight_->IsFadingInOrVisible();
}

void InkDropImpl::SetShowHighlightOnHover(bool show_highlight_on_hover) {
  show_highlight_on_hover_ = show_highlight_on_hover;
  highlight_state_->ShowOnHoverChanged();
}

void InkDropImpl::SetShowHighlightOnFocus(bool show_highlight_on_focus) {
  show_highlight_on_focus_ = show_highlight_on_focus;
  highlight_state_->ShowOnFocusChanged();
}

void InkDropImpl::DestroyHiddenTargetedAnimations() {
  if (ink_drop_ripple_ &&
      (ink_drop_ripple_->target_ink_drop_state() == InkDropState::HIDDEN ||
       ShouldAnimateToHidden(ink_drop_ripple_->target_ink_drop_state()))) {
    DestroyInkDropRipple();
  }
}

void InkDropImpl::CreateInkDropRipple() {
  DCHECK(!destroying_);

  DestroyInkDropRipple();
  ink_drop_ripple_ = ink_drop_host_->CreateInkDropRipple();
  ink_drop_ripple_->set_observer(this);
  root_layer_->Add(ink_drop_ripple_->GetRootLayer());
  AddRootLayerToHostIfNeeded();
}

void InkDropImpl::DestroyInkDropRipple() {
  if (!ink_drop_ripple_)
    return;

  // Ensures no observer callback happens from removing from |root_layer_|
  // or destroying |ink_drop_ripple_|. Speculative fix for crashes in
  // https://crbug.com/1088432 and https://crbug.com/1099844.
  ink_drop_ripple_->set_observer(nullptr);
  root_layer_->Remove(ink_drop_ripple_->GetRootLayer());
  ink_drop_ripple_.reset();
  RemoveRootLayerFromHostIfNeeded();
}

void InkDropImpl::CreateInkDropHighlight() {
  DCHECK(!destroying_);

  DestroyInkDropHighlight();

  highlight_ = ink_drop_host_->CreateInkDropHighlight();
  DCHECK(highlight_);

  // If the platform provides HC colors, we need to show them fully on hover and
  // press.
  if (views::UsingPlatformHighContrastInkDrop(ink_drop_host_->host_view()))
    highlight_->set_visible_opacity(1.0f);

  highlight_->set_observer(this);
  root_layer_->Add(highlight_->layer());
  AddRootLayerToHostIfNeeded();
}

void InkDropImpl::DestroyInkDropHighlight() {
  if (!highlight_)
    return;

  // Ensures no observer callback happens from removing from |root_layer_|
  // or destroying |highlight_|. Speculative fix for crashes in
  // https://crbug.com/1088432 and https://crbug.com/1099844.
  highlight_->set_observer(nullptr);
  root_layer_->Remove(highlight_->layer());
  highlight_.reset();
  RemoveRootLayerFromHostIfNeeded();
}

void InkDropImpl::AddRootLayerToHostIfNeeded() {
  DCHECK(highlight_ || ink_drop_ripple_);
  DCHECK(!root_layer_->children().empty());
  if (!root_layer_added_to_host_) {
    root_layer_added_to_host_ = true;
    ink_drop_host_->AddInkDropLayer(root_layer_.get());
  }
}

void InkDropImpl::RemoveRootLayerFromHostIfNeeded() {
  if (root_layer_added_to_host_ && !highlight_ && !ink_drop_ripple_) {
    root_layer_added_to_host_ = false;
    ink_drop_host_->RemoveInkDropLayer(root_layer_.get());
  }
}

// -----------------------------------------------------------------------------
// views::InkDropRippleObserver:

void InkDropImpl::AnimationStarted(InkDropState ink_drop_state) {
  // AnimationStarted should only be called from |ink_drop_ripple_|.
  DCHECK(ink_drop_ripple_);

  highlight_state_->AnimationStarted(ink_drop_state);
  NotifyInkDropAnimationStarted();
}

void InkDropImpl::AnimationEnded(InkDropState ink_drop_state,
                                 InkDropAnimationEndedReason reason) {
  highlight_state_->AnimationEnded(ink_drop_state, reason);
  NotifyInkDropRippleAnimationEnded(ink_drop_state);
  if (reason != InkDropAnimationEndedReason::SUCCESS)
    return;
  // |ink_drop_ripple_| might be null during destruction.
  if (!ink_drop_ripple_)
    return;
  if (ShouldAnimateToHidden(ink_drop_state)) {
    ink_drop_ripple_->AnimateToState(views::InkDropState::HIDDEN);
  } else if (ink_drop_state == views::InkDropState::HIDDEN) {
    // TODO(bruthig): Investigate whether creating and destroying
    // InkDropRipples is expensive and consider creating an
    // InkDropRipplePool. See www.crbug.com/522175.
    DestroyInkDropRipple();
  }
}

// -----------------------------------------------------------------------------
// views::InkDropHighlightObserver:

void InkDropImpl::AnimationStarted(
    InkDropHighlight::AnimationType animation_type) {
  NotifyInkDropAnimationStarted();
}

void InkDropImpl::AnimationEnded(InkDropHighlight::AnimationType animation_type,
                                 InkDropAnimationEndedReason reason) {
  if (animation_type == InkDropHighlight::AnimationType::kFadeOut &&
      reason == InkDropAnimationEndedReason::SUCCESS) {
    DestroyInkDropHighlight();
  }
}

void InkDropImpl::SetHighlight(bool should_highlight,
                               base::TimeDelta animation_duration) {
  if (IsHighlightFadingInOrVisible() == should_highlight)
    return;

  if (should_highlight) {
    CreateInkDropHighlight();
    highlight_->FadeIn(animation_duration);
  } else {
    highlight_->FadeOut(animation_duration);
  }

  ink_drop_host_->OnInkDropHighlightedChanged();
}

bool InkDropImpl::ShouldHighlight() const {
  return ShouldHighlightBasedOnFocus() ||
         (show_highlight_on_hover_ && is_hovered_);
}

bool InkDropImpl::ShouldHighlightBasedOnFocus() const {
  return show_highlight_on_focus_ && is_focused_;
}

void InkDropImpl::SetHighlightState(
    std::unique_ptr<HighlightState> highlight_state) {
  ExitHighlightState();
  highlight_state_ = std::move(highlight_state);
  highlight_state_->Enter();
}

void InkDropImpl::ExitHighlightState() {
  DCHECK(!exiting_highlight_state_) << "HighlightStates should not be changed "
                                       "within a call to "
                                       "HighlightState::Exit().";
  if (highlight_state_) {
    base::AutoReset<bool> exit_guard(&exiting_highlight_state_, true);
    highlight_state_->Exit();
  }
  highlight_state_ = nullptr;
}

void InkDropImpl::RecreateRippleAndHighlight() {
  const bool create_ink_drop_ripple = !!ink_drop_ripple_;
  InkDropState state = GetTargetInkDropState();
  if (ShouldAnimateToHidden(state)) {
    state = views::InkDropState::HIDDEN;
  }
  DestroyInkDropRipple();

  if (highlight_) {
    bool visible = highlight_->IsFadingInOrVisible();
    DestroyInkDropHighlight();
    // Both the ripple and the highlight must have been destroyed before
    // recreating either of them otherwise the mask will not get recreated.
    CreateInkDropHighlight();
    if (visible) {
      highlight_->FadeIn(base::TimeDelta());
    }
  }

  if (create_ink_drop_ripple) {
    CreateInkDropRipple();
    ink_drop_ripple_->SnapToState(state);
  }
}

}  // namespace views
