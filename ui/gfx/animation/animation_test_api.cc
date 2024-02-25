// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/animation_test_api.h"

#include "base/time/time.h"
#include "ui/gfx/animation/animation.h"

namespace gfx {

// static
std::unique_ptr<base::AutoReset<Animation::RichAnimationRenderMode>>
AnimationTestApi::SetRichAnimationRenderMode(
    Animation::RichAnimationRenderMode mode) {
  // If the mode has already been forced, don't update it further; this prevents
  // overlapping-but-not-nested scopers from having surprising effects. In
  // theory we could support this case robustly, but in practice it seems
  // unnecessary.
  if (Animation::rich_animation_rendering_mode_ !=
      Animation::RichAnimationRenderMode::PLATFORM) {
    return nullptr;
  }
  return std::make_unique<base::AutoReset<Animation::RichAnimationRenderMode>>(
      &Animation::rich_animation_rendering_mode_, mode);
}

AnimationTestApi::AnimationTestApi(Animation* animation)
    : animation_(animation) {}

AnimationTestApi::~AnimationTestApi() {}

void AnimationTestApi::SetStartTime(base::TimeTicks ticks) {
  animation_->SetStartTime(ticks);
}

void AnimationTestApi::Step(base::TimeTicks ticks) {
  animation_->Step(ticks);
}

AnimationContainerTestApi::AnimationContainerTestApi(
    AnimationContainer* container)
    : container_(container) {
  container_->runner_->Stop();
}

AnimationContainerTestApi::~AnimationContainerTestApi() = default;

void AnimationContainerTestApi::IncrementTime(base::TimeDelta delta) {
  container_->runner_->SetAnimationTimeForTesting(container_->last_tick_time() +
                                                  delta);
}

}  // namespace gfx
