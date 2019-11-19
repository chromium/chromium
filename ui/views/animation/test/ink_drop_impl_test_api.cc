// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/test/ink_drop_impl_test_api.h"

#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/animation/test/ink_drop_highlight_test_api.h"
#include "ui/views/animation/test/ink_drop_ripple_test_api.h"

namespace views {
namespace test {

//
// AccessFactoryOnExitHighlightState
//

void InkDropImplTestApi::AccessFactoryOnExitHighlightState::Install(
    InkDropImpl::HighlightStateFactory* state_factory) {
  state_factory->ink_drop()->SetHighlightState(
      std::make_unique<InkDropImplTestApi::AccessFactoryOnExitHighlightState>(
          state_factory));
}

InkDropImplTestApi::AccessFactoryOnExitHighlightState::
    AccessFactoryOnExitHighlightState(
        InkDropImpl::HighlightStateFactory* state_factory)
    : HighlightState(state_factory) {}

void InkDropImplTestApi::AccessFactoryOnExitHighlightState::Exit() {
  state_factory()->ink_drop()->SetHovered(false);
}

void InkDropImplTestApi::AccessFactoryOnExitHighlightState::
    ShowOnHoverChanged() {}

void InkDropImplTestApi::AccessFactoryOnExitHighlightState::OnHoverChanged() {}

void InkDropImplTestApi::AccessFactoryOnExitHighlightState::
    ShowOnFocusChanged() {}

void InkDropImplTestApi::AccessFactoryOnExitHighlightState::OnFocusChanged() {}

void InkDropImplTestApi::AccessFactoryOnExitHighlightState::AnimationStarted(
    InkDropState ink_drop_state) {}

void InkDropImplTestApi::AccessFactoryOnExitHighlightState::AnimationEnded(
    InkDropState ink_drop_state,
    InkDropAnimationEndedReason reason) {}

//
// AccessFactoryOnExitHighlightState
//

void InkDropImplTestApi::SetStateOnExitHighlightState::Install(
    InkDropImpl::HighlightStateFactory* state_factory) {
  state_factory->ink_drop()->SetHighlightState(
      std::make_unique<InkDropImplTestApi::SetStateOnExitHighlightState>(
          state_factory));
}

InkDropImplTestApi::SetStateOnExitHighlightState::SetStateOnExitHighlightState(
    InkDropImpl::HighlightStateFactory* state_factory)
    : HighlightState(state_factory) {}

void InkDropImplTestApi::SetStateOnExitHighlightState::Exit() {
  InkDropImplTestApi::AccessFactoryOnExitHighlightState::Install(
      state_factory());
}

void InkDropImplTestApi::SetStateOnExitHighlightState::ShowOnHoverChanged() {}

void InkDropImplTestApi::SetStateOnExitHighlightState::OnHoverChanged() {}

void InkDropImplTestApi::SetStateOnExitHighlightState::ShowOnFocusChanged() {}

void InkDropImplTestApi::SetStateOnExitHighlightState::OnFocusChanged() {}

void InkDropImplTestApi::SetStateOnExitHighlightState::AnimationStarted(
    InkDropState ink_drop_state) {}

void InkDropImplTestApi::SetStateOnExitHighlightState::AnimationEnded(
    InkDropState ink_drop_state,
    InkDropAnimationEndedReason reason) {}

//
// InkDropImplTestApi
//

InkDropImplTestApi::InkDropImplTestApi(InkDropImpl* ink_drop)
    : ui::test::MultiLayerAnimatorTestController(this), ink_drop_(ink_drop) {}

InkDropImplTestApi::~InkDropImplTestApi() = default;

void InkDropImplTestApi::SetShouldHighlight(bool should_highlight) {
  ink_drop_->SetShowHighlightOnHover(should_highlight);
  ink_drop_->SetHovered(should_highlight);
  ink_drop_->SetShowHighlightOnFocus(should_highlight);
  ink_drop_->SetFocused(should_highlight);
  DCHECK_EQ(should_highlight, ink_drop_->ShouldHighlight());
}

void InkDropImplTestApi::SetHighlightState(
    std::unique_ptr<InkDropImpl::HighlightState> highlight_state) {
  ink_drop_->highlight_state_ = std::move(highlight_state);
}

const InkDropHighlight* InkDropImplTestApi::highlight() const {
  return ink_drop_->highlight_.get();
}

bool InkDropImplTestApi::IsHighlightFadingInOrVisible() const {
  return ink_drop_->IsHighlightFadingInOrVisible();
}

bool InkDropImplTestApi::ShouldHighlight() const {
  return ink_drop_->ShouldHighlight();
}

std::vector<ui::LayerAnimator*> InkDropImplTestApi::GetLayerAnimators() {
  std::vector<ui::LayerAnimator*> animators;

  if (ink_drop_->highlight_) {
    InkDropHighlightTestApi* ink_drop_highlight_test_api =
        ink_drop_->highlight_->GetTestApi();
    std::vector<ui::LayerAnimator*> ink_drop_highlight_animators =
        ink_drop_highlight_test_api->GetLayerAnimators();
    animators.insert(animators.end(), ink_drop_highlight_animators.begin(),
                     ink_drop_highlight_animators.end());
  }

  if (ink_drop_->ink_drop_ripple_) {
    InkDropRippleTestApi* ink_drop_ripple_test_api =
        ink_drop_->ink_drop_ripple_->GetTestApi();
    std::vector<ui::LayerAnimator*> ink_drop_ripple_animators =
        ink_drop_ripple_test_api->GetLayerAnimators();
    animators.insert(animators.end(), ink_drop_ripple_animators.begin(),
                     ink_drop_ripple_animators.end());
  }

  return animators;
}

}  // namespace test
}  // namespace views
