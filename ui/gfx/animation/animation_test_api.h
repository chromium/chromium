// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANIMATION_ANIMATION_TEST_API_H_
#define UI_GFX_ANIMATION_ANIMATION_TEST_API_H_

#include <memory>

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_container.h"
#include "ui/gfx/animation/animation_export.h"

namespace gfx {

// Class to provide access to Animation internals for testing.
class AnimationTestApi {
 public:
  using RenderModeResetter =
      std::unique_ptr<base::AutoReset<Animation::RichAnimationRenderMode>>;

  // Sets the rich animation rendering mode, if it is currently set to PLATFORM.
  // Allows rich animations to be force enabled/disabled during tests.
  [[nodiscard]] static RenderModeResetter SetRichAnimationRenderMode(
      Animation::RichAnimationRenderMode mode);

  explicit AnimationTestApi(Animation* animation);

  AnimationTestApi(const AnimationTestApi&) = delete;
  AnimationTestApi& operator=(const AnimationTestApi&) = delete;

  ~AnimationTestApi();

  // Sets the start of the animation.
  void SetStartTime(base::TimeTicks ticks);

  // Manually steps the animation forward
  void Step(base::TimeTicks ticks);

 private:
  raw_ptr<Animation> animation_;
};

// For manual animation time control in tests. Creating this object will
// pause the AnimationRunner of |container| immediately.
class AnimationContainerTestApi {
 public:
  explicit AnimationContainerTestApi(AnimationContainer* container);
  AnimationContainerTestApi(const AnimationContainerTestApi&) = delete;
  AnimationContainerTestApi& operator=(const AnimationContainerTestApi&) =
      delete;
  ~AnimationContainerTestApi();

  void IncrementTime(base::TimeDelta delta);

 private:
  raw_ptr<AnimationContainer, DanglingUntriaged> container_;
};

}  // namespace gfx

#endif  // UI_GFX_ANIMATION_ANIMATION_TEST_API_H_
