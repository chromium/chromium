// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/scoped_animation_duration_scale_mode.h"

#include "base/check_op.h"

namespace ui {

// static
constexpr float ScopedAnimationDurationScaleMode::NORMAL_DURATION;
// static
constexpr float ScopedAnimationDurationScaleMode::FAST_DURATION;
// static
constexpr float ScopedAnimationDurationScaleMode::SLOW_DURATION;
// static
constexpr float ScopedAnimationDurationScaleMode::NON_ZERO_DURATION;
// static
constexpr float ScopedAnimationDurationScaleMode::ZERO_DURATION;

// static
float ScopedAnimationDurationScaleMode::duration_multiplier_ = 1;

ScopedAnimationDurationScaleMode::ScopedAnimationDurationScaleMode(
    float scoped_multiplier)
    : old_duration_multiplier_(duration_multiplier_) {
  // Sanity checks.
  DCHECK_GE(scoped_multiplier, 0);
  DCHECK_LE(scoped_multiplier, 10);
  duration_multiplier_ = scoped_multiplier;
}

ScopedAnimationDurationScaleMode::~ScopedAnimationDurationScaleMode() {
  duration_multiplier_ = old_duration_multiplier_;
}

}  // namespace ui
