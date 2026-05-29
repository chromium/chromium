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
  // Specifies the direction of the animation.
  enum class AnimationDirection {
    // Play animation in forward direction 0.0 -> 1.0
    kForward,
    // Play animation in reverse direction 1.0 -> 0.0
    kBackward,
  };

  // Specifies the end behavior of the animation.
  enum class AnimationEndBehavior {
    // Pauses the state at the end of animation and shows
    // images corresponding to the last frame.
    kPause,
    // Resets the state of the animation and reverts back
    // to showing the image corresponding to the button state.
    kReset,
  };

  // Defines the animation that should be played.
  struct AnimationDefinition {
    int resource_id;
    SkColor color;
  };

  // Defines the configuration of the animation to play.
  struct AnimationConfig {
    AnimationDirection direction;
    AnimationEndBehavior end_behavior;
  };

  SingleAnimatedImageContainer(
      LabelButton* button,
      base::TimeDelta animation_duration = base::Milliseconds(250));
  SingleAnimatedImageContainer(const SingleAnimatedImageContainer&) = delete;
  SingleAnimatedImageContainer& operator=(const SingleAnimatedImageContainer&) =
      delete;
  ~SingleAnimatedImageContainer() override;

  // Play the animation based on the provided definition and the configuration.
  void PlayAnimation(AnimationDefinition definition,
                     AnimationConfig config = AnimationConfig());

  // Stops the animation and resets it back to using static images.
  void ResetAnimation();

  void ClearAnimatedImages();

  // LabelButtonImageContainer:
  void UpdateImage(const LabelButton* button) override;

 private:
  void AddAnimatedImage(int resource_id);

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  struct AnimationState {
    AnimationDefinition definition;
    AnimationEndBehavior end_behavior;
  };

  raw_ptr<LabelButton> button_;
  gfx::SlideAnimation slide_animation_;
  std::optional<AnimationState> playing_animation_;
  base::flat_map<int, std::unique_ptr<lottie::Animation>> animated_images_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_SINGLE_ANIMATED_IMAGE_CONTAINER_H_
