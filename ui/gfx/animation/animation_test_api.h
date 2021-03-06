// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANIMATION_ANIMATION_TEST_API_H_
#define UI_GFX_ANIMATION_ANIMATION_TEST_API_H_

#include <memory>

#include "base/auto_reset.h"
#include "base/macros.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_container.h"
#include "ui/gfx/animation/animation_export.h"

namespace gfx {

// Class to provide access to Animation internals for testing.
class AnimationTestApi {
 public:
  // Sets the rich animation rendering mode. Allows rich animations to be force
  // enabled/disabled during tests.
  static std::unique_ptr<base::AutoReset<Animation::RichAnimationRenderMode>>
  SetRichAnimationRenderMode(Animation::RichAnimationRenderMode mode);

  explicit AnimationTestApi(Animation* animation);
  ~AnimationTestApi();

  // Sets the start of the animation.
  void SetStartTime(base::TimeTicks ticks);

  // Manually steps the animation forward
  void Step(base::TimeTicks ticks);

 private:
  Animation* animation_;

  DISALLOW_COPY_AND_ASSIGN(AnimationTestApi);
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
  AnimationContainer* container_;
};

}  // namespace gfx

#endif  // UI_GFX_ANIMATION_ANIMATION_TEST_API_H_
