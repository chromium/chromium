// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SCOPED_ANIMATION_DURATION_SCALE_MODE_H_
#define UI_GFX_SCOPED_ANIMATION_DURATION_SCALE_MODE_H_

#include "base/component_export.h"

namespace gfx {

// Speed up or slow down animations for testing or debugging.
class COMPONENT_EXPORT(GFX) ScopedAnimationDurationScaleMode {
 public:
  // Animation duration multipliers.
  static constexpr float NORMAL_DURATION = 1.0;
  static constexpr float FAST_DURATION = 1.0 / 4;    // 4 times faster
  static constexpr float SLOW_DURATION = 1.0 * 4.0;  // 4 times slower
  // A very short but guaranteed non-zero duration for individual tests that
  // need to assert things about animations after creating them.
  static constexpr float NON_ZERO_DURATION = 1.0 / 20;  // 20 times faster
  // Animations complete immediately after being created. Used by most tests.
  static constexpr float ZERO_DURATION = 0;

  explicit ScopedAnimationDurationScaleMode(float scoped_multiplier);
  ScopedAnimationDurationScaleMode(const ScopedAnimationDurationScaleMode&) =
      delete;
  ScopedAnimationDurationScaleMode& operator=(
      const ScopedAnimationDurationScaleMode&) = delete;

  ~ScopedAnimationDurationScaleMode();

  static float duration_multiplier() { return duration_multiplier_; }

  static bool is_zero() { return duration_multiplier_ == ZERO_DURATION; }

 private:
  // Stores the previous multiplier to restore it upon destruction.
  const float old_duration_multiplier_;

  // This is the active global multiplier.
  static float duration_multiplier_;
};

}  // namespace gfx

#endif  // UI_GFX_SCOPED_ANIMATION_DURATION_SCALE_MODE_H_
