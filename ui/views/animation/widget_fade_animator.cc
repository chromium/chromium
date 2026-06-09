// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/widget_fade_animator.h"

#include "base/i18n/rtl.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/views/widget/widget.h"

namespace views {

WidgetFadeAnimator::WidgetFadeAnimator(Widget* widget)
    : AnimationDelegateViews(widget->GetRootView()), widget_(widget) {
  widget_observation_.Observe(widget);
}

WidgetFadeAnimator::~WidgetFadeAnimator() = default;

void WidgetFadeAnimator::FadeIn(int slide_distance,
                                SlideDirection slide_direction) {
  if (IsFadingIn()) {
    return;
  }

  DCHECK(widget_);

  animation_type_ = FadeType::kFadeIn;
  fade_animation_.SetDuration(fade_in_duration_);

  // Widgets cannot be shown when visible and fully transparent.
  widget_->SetOpacity(0.01f);

  switch (show_type_) {
    case WidgetShowType::kNone:
      break;
    case WidgetShowType::kShowActive:
      widget_->Show();
      break;
    case WidgetShowType::kShowInactive:
      widget_->ShowInactive();
      break;
  }

  SetBoundsForSliding(slide_distance, slide_direction);
  fade_animation_.Start();
}

void WidgetFadeAnimator::CancelFadeIn() {
  if (!IsFadingIn()) {
    return;
  }

  fade_animation_.Stop();
  animation_type_ = FadeType::kNone;

  CancelSlide(false);
}

void WidgetFadeAnimator::FadeOut(int slide_distance,
                                 SlideDirection slide_direction) {
  if (IsFadingOut() || !widget_) {
    return;
  }

  // If the widget is already hidden, then there is no current animation and
  // nothing to do. If the animation is close-on-hide, however, we should still
  // close the widget.
  if (!widget_->IsVisible()) {
    DCHECK(!IsFadingIn());
    if (close_on_hide_) {
      widget_->Close();
    }
    fade_complete_callbacks_.Notify(this, FadeType::kFadeOut);
    return;
  }

  animation_type_ = FadeType::kFadeOut;
  SetBoundsForSliding(slide_distance, slide_direction);
  fade_animation_.SetDuration(fade_out_duration_);
  fade_animation_.Start();
}

void WidgetFadeAnimator::CancelFadeOut() {
  if (!IsFadingOut()) {
    return;
  }

  fade_animation_.Stop();
  animation_type_ = FadeType::kNone;
  widget_->SetOpacity(1.0f);

  CancelSlide(false);
}

base::CallbackListSubscription WidgetFadeAnimator::AddFadeCompleteCallback(
    FadeCompleteCallback callback) {
  return fade_complete_callbacks_.Add(callback);
}

void WidgetFadeAnimator::CancelSlide(bool snap_to_target) {
  if (snap_to_target && IsSliding()) {
    widget_->SetBounds(target_bounds_);
  }
  start_bounds_ = target_bounds_ = gfx::Rect();
}

void WidgetFadeAnimator::AnimationProgressed(const gfx::Animation* animation) {
  // Get the value of the animation with a material ease applied.
  double value =
      gfx::Tween::CalculateValue(tween_type_, animation->GetCurrentValue());
  float opacity = 0.0f;
  if (IsFadingOut()) {
    opacity = gfx::Tween::FloatValueBetween(value, 1.0f, 0.0f);
  } else if (IsFadingIn()) {
    opacity = gfx::Tween::FloatValueBetween(value, 0.0f, 1.0f);
  }

  if (IsFadingOut() && opacity == 0.0f) {
    if (close_on_hide_) {
      widget_->Close();
    } else {
      widget_->Hide();
    }
  } else {
    widget_->SetOpacity(opacity);
  }
  if (IsSliding()) {
    widget_->SetBounds(
        gfx::Tween::RectValueBetween(value, start_bounds_, target_bounds_));
  }
}

void WidgetFadeAnimator::AnimationEnded(const gfx::Animation* animation) {
  const FadeType animation_type = animation_type_;
  AnimationProgressed(animation);
  animation_type_ = FadeType::kNone;
  fade_complete_callbacks_.Notify(this, animation_type);
}

void WidgetFadeAnimator::OnWidgetDestroying(Widget* widget) {
  widget_observation_.Reset();
  fade_animation_.Stop();
  animation_type_ = FadeType::kNone;
  widget_ = nullptr;
}

bool WidgetFadeAnimator::IsSliding() const {
  return start_bounds_ != target_bounds_;
}

void WidgetFadeAnimator::SetBoundsForSliding(int slide_distance,
                                             SlideDirection slide_direction) {
  CHECK(animation_type_ != FadeType::kNone);

  if (slide_direction == SlideDirection::kNone) {
    start_bounds_ = target_bounds_ = gfx::Rect();
    return;
  }

  start_bounds_ = target_bounds_ = widget_->GetWindowBoundsInScreen();

  // On fade in we displace the starting bounds to slide to the
  // current window bounds. We reverse this on fade out.
  gfx::Rect& displaced =
      animation_type_ == FadeType::kFadeIn ? start_bounds_ : target_bounds_;

  switch (slide_direction) {
    case SlideDirection::kUp:
      displaced.Offset(0, slide_distance);
      break;
    case SlideDirection::kDown:
      displaced.Offset(0, -slide_distance);
      break;
    case SlideDirection::kLeading:
      displaced.Offset((base::i18n::IsRTL() ? -1 : 1) * slide_distance, 0);
      break;
    case SlideDirection::kTrailing:
      displaced.Offset((base::i18n::IsRTL() ? 1 : -1) * slide_distance, 0);
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace views
