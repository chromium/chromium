// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/scoped_animation_duration_scale_mode.h"

#include "base/check_op.h"
#include "base/logging.h"

namespace gfx {
namespace {

// Number of active DISABLE_ANIMATION requests.
int disable_count = 0;

// The duration multiplier before entering DISABLE_ANIMATION mode.
float pre_disable = 1.0f;

// Computes the duration multiplier to be saved for restoration upon
// destruction. If animations are currently disabled (DISABLE_ANIMATION) and
// this scope is attempting to set another duration, returns std::nullopt to
// ignore this request. Otherwise, returns the current duration multiplier.
std::optional<float> ComputeOldDurationMultiplier(
    float scoped_multiplier,
    float current_duration_multiplier) {
  if (disable_count > 0 &&
      scoped_multiplier !=
          ScopedAnimationDurationScaleMode::DISABLE_ANIMATION) {
    LOG(WARNING) << "Ignoring animation duration scale mode request ("
                 << scoped_multiplier
                 << ") while animations are disabled (DISABLE_ANIMATION).";
    return std::nullopt;
  }
  return current_duration_multiplier;
}

}  // namespace

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
constexpr float ScopedAnimationDurationScaleMode::DISABLE_ANIMATION;

// static
float ScopedAnimationDurationScaleMode::duration_multiplier_ = 1.0f;

ScopedAnimationDurationScaleMode::ScopedAnimationDurationScaleMode(
    float scoped_multiplier)
    : previous_duration_multiplier_(
          ComputeOldDurationMultiplier(scoped_multiplier,
                                       duration_multiplier_)),
      disable_(scoped_multiplier == DISABLE_ANIMATION) {
  // Sanity checks.
  DCHECK(scoped_multiplier == DISABLE_ANIMATION ||
         (scoped_multiplier >= 0 && scoped_multiplier <= 10));

  if (disable_) {
    if (disable_count == 0) {
      pre_disable = duration_multiplier_;
      duration_multiplier_ = ZERO_DURATION;
    }
    disable_count++;
  } else if (previous_duration_multiplier_) {
    duration_multiplier_ = scoped_multiplier;
  }
}

ScopedAnimationDurationScaleMode::~ScopedAnimationDurationScaleMode() {
  if (disable_) {
    if (disable_count > 0) {
      disable_count--;
      if (disable_count == 0) {
        duration_multiplier_ = pre_disable;
      }
    }
  } else if (previous_duration_multiplier_) {
    if (disable_count > 0) {
      pre_disable = *previous_duration_multiplier_;
    } else {
      duration_multiplier_ = *previous_duration_multiplier_;
    }
  }
}

}  // namespace gfx
