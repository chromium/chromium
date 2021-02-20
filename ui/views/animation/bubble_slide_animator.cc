// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/bubble_slide_animator.h"

#include "base/time/time.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {

BubbleSlideAnimator::BubbleSlideAnimator(
    BubbleDialogDelegateView* bubble_delegate)
    : AnimationDelegateViews(bubble_delegate),
      bubble_delegate_(bubble_delegate) {
  Widget* widget = bubble_delegate->GetWidget();
  DCHECK(widget);
  widget_observation_.Observe(widget);

  constexpr base::TimeDelta kDefaultBubbleSlideAnimationTime =
      base::TimeDelta::FromMilliseconds(200);
  slide_animation_.SetDuration(kDefaultBubbleSlideAnimationTime);
}

BubbleSlideAnimator::~BubbleSlideAnimator() = default;

void BubbleSlideAnimator::SetSlideDuration(base::TimeDelta duration) {
  slide_animation_.SetDuration(duration);
}

void BubbleSlideAnimator::AnimateToAnchorView(View* desired_anchor_view) {
  desired_anchor_view_ = desired_anchor_view;
  starting_bubble_bounds_ =
      bubble_delegate_->GetWidget()->GetWindowBoundsInScreen();
  target_bubble_bounds_ = CalculateTargetBounds(desired_anchor_view);
  slide_animation_.SetCurrentValue(0);
  slide_animation_.Start();
}

void BubbleSlideAnimator::SnapToAnchorView(View* desired_anchor_view) {
  StopAnimation();
  target_bubble_bounds_ = CalculateTargetBounds(desired_anchor_view);
  starting_bubble_bounds_ = target_bubble_bounds_;
  bubble_delegate_->GetWidget()->SetBounds(target_bubble_bounds_);
  bubble_delegate_->SetAnchorView(desired_anchor_view);
  slide_progressed_callbacks_.Notify(this, 1.0);
  slide_complete_callbacks_.Notify(this);
}

void BubbleSlideAnimator::StopAnimation() {
  slide_animation_.Stop();
  desired_anchor_view_ = nullptr;
}

base::CallbackListSubscription BubbleSlideAnimator::AddSlideProgressedCallback(
    SlideProgressedCallback callback) {
  return slide_progressed_callbacks_.Add(callback);
}

base::CallbackListSubscription BubbleSlideAnimator::AddSlideCompleteCallback(
    SlideCompleteCallback callback) {
  return slide_complete_callbacks_.Add(callback);
}

void BubbleSlideAnimator::AnimationProgressed(const gfx::Animation* animation) {
  double value = gfx::Tween::CalculateValue(tween_type_,
                                            slide_animation_.GetCurrentValue());

  const gfx::Rect current_bounds = gfx::Tween::RectValueBetween(
      value, starting_bubble_bounds_, target_bubble_bounds_);
  if (current_bounds == target_bubble_bounds_ && desired_anchor_view_)
    bubble_delegate_->SetAnchorView(desired_anchor_view_);

  bubble_delegate_->GetWidget()->SetBounds(current_bounds);
  slide_progressed_callbacks_.Notify(this, value);
}

void BubbleSlideAnimator::AnimationEnded(const gfx::Animation* animation) {
  desired_anchor_view_ = nullptr;
  slide_complete_callbacks_.Notify(this);
}

void BubbleSlideAnimator::AnimationCanceled(const gfx::Animation* animation) {
  desired_anchor_view_ = nullptr;
}

void BubbleSlideAnimator::OnWidgetDestroying(Widget* widget) {
  widget_observation_.Reset();
  slide_animation_.Stop();
}

gfx::Rect BubbleSlideAnimator::CalculateTargetBounds(
    const View* desired_anchor_view) const {
  gfx::Rect anchor_bounds = desired_anchor_view->GetAnchorBoundsInScreen();
  anchor_bounds.Inset(bubble_delegate_->anchor_view_insets());
  return bubble_delegate_->GetBubbleFrameView()->GetUpdatedWindowBounds(
      anchor_bounds, bubble_delegate_->arrow(),
      bubble_delegate_->GetWidget()->client_view()->GetPreferredSize(), true);
}

}  // namespace views
