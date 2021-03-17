// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANIMATION_SLIDE_ANIMATION_H_
#define UI_GFX_ANIMATION_SLIDE_ANIMATION_H_

#include "base/macros.h"
#include "base/optional.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/tween.h"

namespace gfx {

// Slide Animation
//
// Used for reversible animations and as a general helper class. Typical usage:
//
// #include "ui/gfx/animation/slide_animation.h"
//
// class MyClass : public AnimationDelegate {
//  public:
//   MyClass() {
//     animation_ = std::make_unique<SlideAnimation>(this);
//     animation_->SetSlideDuration(base::TimeDelta::FromMilliseconds(500));
//   }
//   void OnMouseOver() {
//     animation_->Show();
//   }
//   void OnMouseOut() {
//     animation_->Hide();
//   }
//   void AnimationProgressed(const Animation* animation) {
//     if (animation == animation_.get()) {
//       Layout();
//       SchedulePaint();
//     } else if (animation == other_animation_.get()) {
//       ...
//     }
//   }
//   void Layout() {
//     if (animation_->is_animating()) {
//       hover_image_.SetOpacity(animation_->GetCurrentValue());
//     }
//   }
//  private:
//   std::unique_ptr<SlideAnimation> animation_;
// }
class ANIMATION_EXPORT SlideAnimation : public LinearAnimation {
 public:
  explicit SlideAnimation(AnimationDelegate* target);
  ~SlideAnimation() override;

  // Set the animation to some state.
  virtual void Reset(double value = 0);

  // Begin a showing animation or reverse a hiding animation in progress.
  // Animates GetCurrentValue() towards 1.
  virtual void Show();

  // Begin a hiding animation or reverse a showing animation in progress.
  // Animates GetCurrentValue() towards 0.
  virtual void Hide();

  // Sets the time a slide will take. Note that this isn't actually
  // the amount of time an animation will take as the current value of
  // the slide is considered.
  virtual void SetSlideDuration(base::TimeDelta duration);
  base::TimeDelta GetSlideDuration() const { return slide_duration_; }
  void SetTweenType(Tween::Type tween_type) { tween_type_ = tween_type; }

  // Dampens the reduction in duration for an animation which starts partway.
  // The default value of 1 has no effect.
  void SetDampeningValue(double dampening_value);

  double GetCurrentValue() const override;
  // TODO(bruthig): Fix IsShowing() and IsClosing() to be consistent. e.g.
  // IsShowing() will currently return true after the 'show' animation has been
  // completed however IsClosing() will return false after the 'hide' animation
  // has been completed.
  bool IsShowing() const {
    return direction_ == Direction::kShowing ||
           (!direction_ && value_current_ == 1);
  }
  bool IsClosing() const {
    return direction_ == Direction::kHiding && value_end_ < value_current_;
  }

  class TestApi;

 private:
  // Gets the duration based on the dampening factor and whether the animation
  // is showing or hiding.
  base::TimeDelta GetDuration();

  enum class Direction {
    kShowing,
    kHiding,
  };

  // Implementation of Show() and Hide().
  void BeginAnimating(Direction direction);

  // Overridden from Animation.
  void AnimateToState(double state) override;

  AnimationDelegate* target_;

  Tween::Type tween_type_ = Tween::EASE_OUT;

  // Current animation direction, or nullopt if not animating.
  base::Optional<Direction> direction_;

  // Animation values. These are a layer on top of Animation::state_ to
  // provide the reversability.
  double value_start_ = 0;
  double value_end_ = 0;
  double value_current_ = 0;

  // How long a hover in/out animation will last for. This can be overridden
  // with SetSlideDuration().
  base::TimeDelta slide_duration_ = base::TimeDelta::FromMilliseconds(120);

  // Dampens the reduction in duration for animations which start partway.
  double dampening_value_ = 1.0;

  DISALLOW_COPY_AND_ASSIGN(SlideAnimation);
};

}  // namespace gfx

#endif  // UI_GFX_ANIMATION_SLIDE_ANIMATION_H_
