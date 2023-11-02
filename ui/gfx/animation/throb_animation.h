// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANIMATION_THROB_ANIMATION_H_
#define UI_GFX_ANIMATION_THROB_ANIMATION_H_

#include "ui/gfx/animation/slide_animation.h"

namespace gfx {

// A subclass of SlideAnimation that can continually slide. All of the Animation
// methods behave like that of SlideAnimation: transition to the next state.
// The StartThrobbing method causes the ThrobAnimation to cycle between hidden
// and shown for a set number of cycles.
//
// A ThrobAnimation has two durations: the duration used when behavior like
// a SlideAnimation, and the duration used when throbbing.
class ANIMATION_EXPORT ThrobAnimation : public SlideAnimation {
 public:
  explicit ThrobAnimation(AnimationDelegate* target);

  ThrobAnimation(const ThrobAnimation&) = delete;
  ThrobAnimation& operator=(const ThrobAnimation&) = delete;

  ~ThrobAnimation() override {}

  // Starts throbbing. cycles_til_stop gives the number of cycles to do before
  // stopping. A negative value means "throb indefinitely".
  void StartThrobbing(int cycles_til_stop);

  // Sets the duration of the slide animation when throbbing.
  void SetThrobDuration(base::TimeDelta duration) {
    throb_duration_ = duration;
  }

  // Overridden to reset to the slide duration.
  void Reset(double value = 0) override;
  void Show() override;
  void Hide() override;

  // Overridden to maintain the slide duration.
  void SetSlideDuration(base::TimeDelta duration) override;

  // The number of cycles remaining until the animation stops.
  void set_cycles_remaining(int value) { cycles_remaining_ = value; }
  int cycles_remaining() const { return cycles_remaining_; }

 protected:
  // Overriden to continually throb (assuming we're throbbing).
  void Step(base::TimeTicks time_now) override;

 private:
  // Stops throbbing; as a result this will behave like a SlideAnimation.
  void StopThrobbing();

  // Duration of the slide animation.
  base::TimeDelta slide_duration_ = GetSlideDuration();

  // Duration of the slide animation when throbbing.
  base::TimeDelta throb_duration_ = base::Milliseconds(400);

  // If throbbing, this is the number of cycles left.
  int cycles_remaining_ = 0;

  // Are we throbbing?
  bool throbbing_ = false;
};

}  // namespace gfx

#endif  // UI_GFX_ANIMATION_THROB_ANIMATION_H_
