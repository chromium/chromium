// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/animation.h"

#include <windows.h>

#include "base/check.h"
#include "base/features.h"
#include "base/win/win_util.h"

namespace gfx {

// static
std::optional<bool> Animation::has_reduced_motion_platform_parameter_{};

// static
bool Animation::ShouldRenderRichAnimationImpl() {
  // WM_SETTINGCHANGE messages are broadcasted when SystemParametersInfo()
  // causes modifications. UpdatePrefersReducedMotion() is called when such an
  // event is received.
  if (base::features::IsReducePPMsEnabled()) {
    if (!has_reduced_motion_platform_parameter_.has_value()) {
      UpdatePrefersReducedMotion();
    }
    if (*has_reduced_motion_platform_parameter_) {
      return !*prefers_reduced_motion_;
    }
  } else {
    BOOL result;
    // Get "Turn off all unnecessary animations" value.
    if (::SystemParametersInfo(SPI_GETCLIENTAREAANIMATION, 0, &result, 0)) {
      return !!result;
    }
  }
  return !base::win::IsCurrentSessionRemote();
}

// static
bool Animation::ScrollAnimationsEnabledBySystem() {
  return ShouldRenderRichAnimation();
}

// static
void Animation::UpdatePrefersReducedMotion() {
  // prefers_reduced_motion_ should only be modified on the UI thread.
  // TODO(crbug.com/40611878): DCHECK this assertion once tests are
  // well-behaved.

  // We default to assuming that animations are enabled, to avoid impacting the
  // experience for users on systems that don't have SPI_GETCLIENTAREAANIMATION.
  BOOL win_anim_enabled = true;
  has_reduced_motion_platform_parameter_.emplace(::SystemParametersInfo(
      SPI_GETCLIENTAREAANIMATION, 0, &win_anim_enabled, 0));
  prefers_reduced_motion_ = !win_anim_enabled;
}

}  // namespace gfx
