// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_SINGLE_ANIMATED_IMAGE_CONTAINER_H_
#define UI_VIEWS_CONTROLS_BUTTON_SINGLE_ANIMATED_IMAGE_CONTAINER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/lottie/animation.h"
#include "ui/views/controls/button/label_button_image_container.h"
#include "ui/views/views_export.h"

namespace views {

// A LabelButtonImageContainer that displays a single Lottie animation and
// manages its playback state.
class VIEWS_EXPORT SingleAnimatedImageContainer : public SingleImageContainer,
                                                  gfx::AnimationDelegate {
 public:
  SingleAnimatedImageContainer(
      LabelButton* button,
      base::TimeDelta animation_duration = base::Milliseconds(250));
  SingleAnimatedImageContainer(const SingleAnimatedImageContainer&) = delete;
  SingleAnimatedImageContainer& operator=(const SingleAnimatedImageContainer&) =
      delete;
  ~SingleAnimatedImageContainer() override;

  // Sets parameters to display ImageModel objects generated from the
  // lottie resource, with the given `color`.
  // TODO(crbug.com/517231960): Use SkottieColorMap to set the colors
  // when making the lottie animation.
  void SetAnimatedImage(int lottie_resource_id, SkColor color);
  void ClearAnimatedImage();

  // Play the animation from start to the end.
  // Animation is considered to be finished on end if `reset_on_completion`
  // is true. Otherwise, the animation is considered to still be running
  // until the animation rewinds to the start position by calling
  // `HideAnimation` or `ResetAnimation` is called. While the
  // animation is running, image updates not coming from the
  // animation will be ignored.
  void ShowAnimation(bool reset_on_completion = false);

  // Rewind the animation, and on completion set the image model
  // according to the button's current state. If `start_from_end`
  // is true, then we rewind the animation starting from the end
  // of the animation
  void HideAnimation();

  // Stops the animation and resets it to the start position.
  void ResetAnimation();

  const lottie::Animation* animated_image() const {
    return animated_image_.get();
  }

  // LabelButtonImageContainer:
  void UpdateImage(const LabelButton* button) override;

 private:
  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  raw_ptr<LabelButton> button_;
  gfx::SlideAnimation slide_animation_;
  std::unique_ptr<lottie::Animation> animated_image_;
  SkColor color_;
  bool reset_on_completion_ = false;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_SINGLE_ANIMATED_IMAGE_CONTAINER_H_
