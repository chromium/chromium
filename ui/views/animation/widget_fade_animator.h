// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_WIDGET_FADE_ANIMATOR_H_
#define UI_VIEWS_ANIMATION_WIDGET_FADE_ANIMATOR_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {

class Widget;

// Animates a widget's opacity between fully hidden and fully shown, providing
// a fade-in/fade-out effect.
class VIEWS_EXPORT WidgetFadeAnimator : public AnimationDelegateViews,
                                        public WidgetObserver {
 public:
  // Describes the current fade animation.
  enum class FadeType {
    kNone,
    kFadeIn,
    kFadeOut,
  };

  // Defines a callback for when a fade completes. Not called on cancel. The
  // |animation_type| of the completed animation is specified (it will never be
  // kNone).
  using FadeCompleteCallbackSignature = void(WidgetFadeAnimator*,
                                             FadeType animation_type);
  using FadeCompleteCallback =
      base::RepeatingCallback<FadeCompleteCallbackSignature>;

  // Creates a new fade animator for the specified widget. If the widget closes
  // the animator will no longer be valid and should not be used.
  explicit WidgetFadeAnimator(Widget* widget);
  WidgetFadeAnimator(const WidgetFadeAnimator&) = delete;
  WidgetFadeAnimator& operator=(const WidgetFadeAnimator&) = delete;
  ~WidgetFadeAnimator() override;

  void set_fade_in_duration(base::TimeDelta fade_in_duration) {
    fade_in_duration_ = fade_in_duration;
  }
  base::TimeDelta fade_in_duration() const { return fade_in_duration_; }

  void set_fade_out_duration(base::TimeDelta fade_out_duration) {
    fade_out_duration_ = fade_out_duration;
  }
  base::TimeDelta fade_out_duration() const { return fade_out_duration_; }

  void set_tween_type(gfx::Tween::Type tween_type) { tween_type_ = tween_type; }
  gfx::Tween::Type tween_type() const { return tween_type_; }

  void set_close_on_hide(bool close_on_hide) { close_on_hide_ = close_on_hide; }
  bool close_on_hide() const { return close_on_hide_; }

  Widget* widget() { return widget_; }

  bool IsFadingIn() const { return animation_type_ == FadeType::kFadeIn; }

  bool IsFadingOut() const { return animation_type_ == FadeType::kFadeOut; }

  // Plays the fade-in animation. If the widget is not currently visible, it
  // will be made visible.
  void FadeIn();

  // Cancels any pending fade-in, leaves the widget at the current opacity to
  // avoid abrupt visual changes. CancelFadeIn() should be followed with
  // something, either another FadeIn(), or widget closing. It has no effect
  // if the widget is not fading in.
  void CancelFadeIn();

  // Plays the fade-out animation. At the end of the fade, the widget will be
  // hidden or closed, as per |close_on_hide|. If the widget is already hidden
  // or closed, completes immediately.
  void FadeOut();

  // Cancels any pending fade-out, returning the widget to 100% opacity. Has no
  // effect if the widget is not fading out.
  void CancelFadeOut();

  // Adds a listener for fade complete events.
  base::CallbackListSubscription AddFadeCompleteCallback(
      FadeCompleteCallback callback);

 private:
  // AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // WidgetObserver:
  void OnWidgetDestroying(Widget* widget) override;

  raw_ptr<Widget> widget_;
  base::ScopedObservation<Widget, WidgetObserver> widget_observation_{this};
  gfx::LinearAnimation fade_animation_{this};
  FadeType animation_type_ = FadeType::kNone;

  // Duration for fade-in animations. The default should be visually pleasing
  // for most applications.
  base::TimeDelta fade_in_duration_ = base::Milliseconds(200);

  // Duration for fade-out animations. The default should be visually pleasing
  // for most applications.
  base::TimeDelta fade_out_duration_ = base::Milliseconds(150);

  // The tween type to use. The default value should be pleasing for most
  // applications.
  gfx::Tween::Type tween_type_ = gfx::Tween::FAST_OUT_SLOW_IN;

  // Whether the widget should be closed at the end of a fade-out animation.
  bool close_on_hide_ = false;

  base::RepeatingCallbackList<FadeCompleteCallbackSignature>
      fade_complete_callbacks_;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_WIDGET_FADE_ANIMATOR_H_
