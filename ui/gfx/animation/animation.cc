// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/animation.h"

#include <memory>

#include "base/command_line.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/gfx/animation/animation_container.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/switches.h"

namespace gfx {

// static
Animation::RichAnimationRenderMode Animation::rich_animation_rendering_mode_ =
    RichAnimationRenderMode::PLATFORM;

// static
std::optional<bool> Animation::prefers_reduced_motion_;

Animation::Animation(base::TimeDelta timer_interval)
    : timer_interval_(timer_interval),
      is_animating_(false),
      delegate_(nullptr) {}

Animation::~Animation() {
  // Don't send out notification from the destructor. Chances are the delegate
  // owns us and is being deleted as well.
  if (is_animating_)
    container_->Stop(this);
}

void Animation::Start() {
  if (is_animating_)
    return;

  if (!container_) {
    container_ = base::MakeRefCounted<AnimationContainer>();
    if (delegate_)
      delegate_->AnimationContainerWasSet(container_.get());
  }

  is_animating_ = true;

  container_->Start(this);

  AnimationStarted();
}

void Animation::Stop() {
  if (!is_animating_)
    return;

  is_animating_ = false;

  // Notify the container first as the delegate may delete us.
  container_->Stop(this);

  AnimationStopped();

  if (delegate_) {
    if (ShouldSendCanceledFromStop())
      delegate_->AnimationCanceled(this);
    else
      delegate_->AnimationEnded(this);
  }
}

double Animation::CurrentValueBetween(double start, double target) const {
  return Tween::DoubleValueBetween(GetCurrentValue(), start, target);
}

int Animation::CurrentValueBetween(int start, int target) const {
  return Tween::IntValueBetween(GetCurrentValue(), start, target);
}

gfx::Rect Animation::CurrentValueBetween(const gfx::Rect& start_bounds,
                                         const gfx::Rect& target_bounds) const {
  return Tween::RectValueBetween(GetCurrentValue(), start_bounds,
                                 target_bounds);
}

void Animation::SetContainer(AnimationContainer* container) {
  if (container == container_.get())
    return;

  if (is_animating_)
    container_->Stop(this);

  if (container)
    container_ = container;
  else
    container_ = new AnimationContainer();

  if (delegate_)
    delegate_->AnimationContainerWasSet(container_.get());

  if (is_animating_)
    container_->Start(this);
}

bool Animation::ShouldRenderRichAnimation() {
  if (rich_animation_rendering_mode_ == RichAnimationRenderMode::PLATFORM)
    return ShouldRenderRichAnimationImpl();
  return rich_animation_rendering_mode_ ==
         RichAnimationRenderMode::FORCE_ENABLED;
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS) || BUILDFLAG(IS_FUCHSIA)
// static
bool Animation::ShouldRenderRichAnimationImpl() {
  return true;
  // Defined in platform specific file for Windows, OSX, ChromeOS and Linux.
}

// static
bool Animation::ScrollAnimationsEnabledBySystem() {
  return true;
  // Defined in platform specific file for Windows, OSX, ChromeOS and Linux.
}

#if !BUILDFLAG(IS_ANDROID)
// static
void Animation::UpdatePrefersReducedMotion() {
  // prefers_reduced_motion_ should only be modified on the UI thread.
  // TODO(crbug.com/40611878): DCHECK this assertion once tests are
  // well-behaved.

  // By default, we assume that animations are enabled, to avoid impacting the
  // experience for users on systems that don't have APIs for reduced motion.
  prefers_reduced_motion_ = false;
}
#endif  // !BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS) || BUILDFLAG(IS_FUCHSIA)

// static
bool Animation::PrefersReducedMotion() {
  // --force-prefers-reduced-motion and --force-prefers-no-reduced-motion
  // must always override |prefers_reduced_motion_|, so check them first.
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kForcePrefersReducedMotion)) {
    return true;
  } else if (command_line->HasSwitch(switches::kForcePrefersNoReducedMotion)) {
    return false;
  }

  if (!prefers_reduced_motion_.has_value())
    UpdatePrefersReducedMotion();
  return prefers_reduced_motion_.value();
}

bool Animation::ShouldSendCanceledFromStop() {
  return false;
}

void Animation::SetStartTime(base::TimeTicks start_time) {
  start_time_ = start_time;
}

base::TimeDelta Animation::GetTimerInterval() const {
  return timer_interval_;
}

}  // namespace gfx
