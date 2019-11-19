// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/animation_runner.h"

#include <utility>

#include "base/timer/timer.h"

namespace {

// A default AnimationRunner based on base::Timer.
// TODO(https://crbug.com/953585): Remove this altogether.
class DefaultAnimationRunner : public gfx::AnimationRunner {
 public:
  DefaultAnimationRunner() = default;
  ~DefaultAnimationRunner() override = default;

  // gfx::AnimationRunner:
  void Stop() override;

 protected:
  // gfx::AnimationRunner:
  void OnStart(base::TimeDelta min_interval, base::TimeDelta elapsed) override;

 private:
  void OnTimerTick();

  base::OneShotTimer timer_;
  base::TimeDelta min_interval_;
};

void DefaultAnimationRunner::Stop() {
  timer_.Stop();
}

void DefaultAnimationRunner::OnStart(base::TimeDelta min_interval,
                                     base::TimeDelta elapsed) {
  min_interval_ = min_interval;
  timer_.Start(FROM_HERE, min_interval - elapsed, this,
               &DefaultAnimationRunner::OnTimerTick);
}

void DefaultAnimationRunner::OnTimerTick() {
  // This is effectively a RepeatingTimer.  It's possible to use a true
  // RepeatingTimer for this, but since OnStart() may need to use a OneShotTimer
  // anyway (when |elapsed| is nonzero), it's just more complicated.
  timer_.Start(FROM_HERE, min_interval_, this,
               &DefaultAnimationRunner::OnTimerTick);
  // Call Step() after timer_.Start() in case Step() calls Stop().
  Step(base::TimeTicks::Now());
}

}  // namespace

namespace gfx {

// static
std::unique_ptr<AnimationRunner>
AnimationRunner::CreateDefaultAnimationRunner() {
  return std::make_unique<DefaultAnimationRunner>();
}

AnimationRunner::~AnimationRunner() = default;

void AnimationRunner::Start(
    base::TimeDelta min_interval,
    base::TimeDelta elapsed,
    base::RepeatingCallback<void(base::TimeTicks)> step) {
  step_ = std::move(step);
  OnStart(min_interval, elapsed);
}

AnimationRunner::AnimationRunner() = default;

void AnimationRunner::Step(base::TimeTicks tick) {
  step_.Run(tick);
}

void AnimationRunner::SetAnimationTimeForTesting(base::TimeTicks time) {
  step_.Run(time);
}

}  // namespace gfx
