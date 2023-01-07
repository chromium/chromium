// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/widget_fade_animator.h"

#include "ui/gfx/animation/linear_animation.h"
#include "ui/views/widget/widget.h"

namespace views {

WidgetFadeAnimator::WidgetFadeAnimator(Widget* widget)
    : AnimationDelegateViews(widget->GetRootView()), widget_(widget) {
  widget_observation_.Observe(widget);
}

WidgetFadeAnimator::~WidgetFadeAnimator() = default;

void WidgetFadeAnimator::FadeIn() {
  if (IsFadingIn())
    return;

  DCHECK(widget_);

  animation_type_ = FadeType::kFadeIn;
  fade_animation_.SetDuration(fade_in_duration_);

  // Widgets cannot be shown when visible and fully transparent.
  widget_->SetOpacity(0.01f);
  widget_->Show();
  fade_animation_.Start();
}

void WidgetFadeAnimator::CancelFadeIn() {
  if (!IsFadingIn())
    return;

  fade_animation_.Stop();
  animation_type_ = FadeType::kNone;
}

void WidgetFadeAnimator::FadeOut() {
  if (IsFadingOut() || !widget_)
    return;

  // If the widget is already hidden, then there is no current animation and
  // nothing to do. If the animation is close-on-hide, however, we should still
  // close the widget.
  if (!widget_->IsVisible()) {
    DCHECK(!IsFadingIn());
    if (close_on_hide_)
      widget_->Close();
    fade_complete_callbacks_.Notify(this, FadeType::kFadeOut);
    return;
  }

  animation_type_ = FadeType::kFadeOut;
  fade_animation_.SetDuration(fade_out_duration_);
  fade_animation_.Start();
}

void WidgetFadeAnimator::CancelFadeOut() {
  if (!IsFadingOut())
    return;

  fade_animation_.Stop();
  animation_type_ = FadeType::kNone;
  widget_->SetOpacity(1.0f);
}

base::CallbackListSubscription WidgetFadeAnimator::AddFadeCompleteCallback(
    FadeCompleteCallback callback) {
  return fade_complete_callbacks_.Add(callback);
}

void WidgetFadeAnimator::AnimationProgressed(const gfx::Animation* animation) {
  // Get the value of the animation with a material ease applied.
  double value =
      gfx::Tween::CalculateValue(tween_type_, animation->GetCurrentValue());
  float opacity = 0.0f;
  if (IsFadingOut())
    opacity = gfx::Tween::FloatValueBetween(value, 1.0f, 0.0f);
  else if (IsFadingIn())
    opacity = gfx::Tween::FloatValueBetween(value, 0.0f, 1.0f);

  if (IsFadingOut() && opacity == 0.0f) {
    if (close_on_hide_)
      widget_->Close();
    else
      widget_->Hide();
  } else {
    widget_->SetOpacity(opacity);
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
  fade_animation_.End();
  animation_type_ = FadeType::kNone;
  widget_ = nullptr;
}

}  // namespace views
