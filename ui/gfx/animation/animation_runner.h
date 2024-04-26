// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANIMATION_ANIMATION_RUNNER_H_
#define UI_GFX_ANIMATION_ANIMATION_RUNNER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "ui/gfx/animation/animation_export.h"

namespace gfx {

// Interface for custom animation runner. CompositorAnimationRunner can control
// animation tick with this.
class ANIMATION_EXPORT AnimationRunner {
 public:
  // Creates a default AnimationRunner based on base::Timer. Ideally,
  // we should prefer the compositor-based animation runner to this.
  // TODO(crbug.com/41453351): Remove this altogether.
  static std::unique_ptr<AnimationRunner> CreateDefaultAnimationRunner();

  AnimationRunner(const AnimationRunner&) = delete;
  AnimationRunner& operator=(const AnimationRunner&) = delete;
  virtual ~AnimationRunner();

  // Sets the provided |step| callback, then calls OnStart() with the provided
  // |min_interval| and |elapsed| time to allow the subclass to actually begin
  // animating. Subclasses are expected to call Step() periodically to drive the
  // animation.
  void Start(base::TimeDelta min_interval,
             base::TimeDelta elapsed,
             base::RepeatingCallback<void(base::TimeTicks)> step);

  // Called when subclasses don't need to call Step() anymore.
  virtual void Stop() = 0;

  bool step_is_null_for_testing() const { return step_.is_null(); }

 protected:
  AnimationRunner();

  // Called when subclasses should start calling Step() periodically to
  // drive the animation.
  virtual void OnStart(base::TimeDelta min_interval,
                       base::TimeDelta elapsed) = 0;

  // Advances the animation based on |tick|.
  void Step(base::TimeTicks tick);

 private:
  friend class AnimationContainerTestApi;

  // Advances the animation manually for testing.
  void SetAnimationTimeForTesting(base::TimeTicks time);

  base::RepeatingCallback<void(base::TimeTicks)> step_;
};

}  // namespace gfx

#endif  // UI_GFX_ANIMATION_ANIMATION_RUNNER_H_
