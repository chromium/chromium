// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SCOPED_ANIMATION_DURATION_SCALE_MODE_H_
#define UI_GFX_SCOPED_ANIMATION_DURATION_SCALE_MODE_H_

#include <optional>

#include "base/component_export.h"

namespace gfx {

// Speed up or slow down animations.
//
// DISABLE_ANIMATION is treated as a special request to disable animations.
// Multiple DISABLE_ANIMATION scopes can be nested or active concurrently; the
// animation remains disabled (ZERO_DURATION) until all active DISABLE_ANIMATION
// scopes are destroyed. Once all DISABLE_ANIMATION scopes exit, the duration
// multiplier is restored to the value active before entering disabled mode.
//
// Any attempts to create a non-disable duration scope while DISABLE_ANIMATION
// is active are ignored. If an existing scope exits while DISABLE_ANIMATION is
// active, the pre-disabled duration is updated so that the multiplier
// correctly falls back when all DISABLE_ANIMATION scopes complete.
class COMPONENT_EXPORT(ANIMATION_SCALE) ScopedAnimationDurationScaleMode {
 public:
  // TODO(crbug.com/513396054): Change DURATION constants to an enum while
  // leaving an option for custom duration multipliers.
  // Animation duration multipliers.
  static constexpr float NORMAL_DURATION = 1.0;
  static constexpr float FAST_DURATION = 1.0 / 4;    // 4 times faster
  static constexpr float SLOW_DURATION = 1.0 * 4.0;  // 4 times slower
  // A very short but guaranteed non-zero duration for individual tests that
  // need to assert things about animations after creating them.
  static constexpr float NON_ZERO_DURATION = 1.0 / 20;  // 20 times faster
  // Animations complete immediately after being created. Used by most tests.
  static constexpr float ZERO_DURATION = 0;
  // Completely disables animations and ignores attempts to change the duration
  // until all active DISABLE_ANIMATION scopes are destroyed.
  static constexpr float DISABLE_ANIMATION = -1.0;

  explicit ScopedAnimationDurationScaleMode(float scoped_multiplier);
  ScopedAnimationDurationScaleMode(const ScopedAnimationDurationScaleMode&) =
      delete;
  ScopedAnimationDurationScaleMode& operator=(
      const ScopedAnimationDurationScaleMode&) = delete;

  ~ScopedAnimationDurationScaleMode();

  static float duration_multiplier() { return duration_multiplier_; }

  static bool is_zero() { return duration_multiplier_ == ZERO_DURATION; }

 private:
  // Stores the previous multiplier to restore it upon destruction. Set to
  // nullopt if this scope was created while animations are disabled via
  // DISABLE_ANIMATION and should be ignored.
  const std::optional<float> previous_duration_multiplier_;

  const bool disable_;

  // This is the active global multiplier.
  static float duration_multiplier_;
};

}  // namespace gfx

#endif  // UI_GFX_SCOPED_ANIMATION_DURATION_SCALE_MODE_H_
