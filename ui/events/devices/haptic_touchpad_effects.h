// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_HAPTIC_TOUCHPAD_EFFECTS_H_
#define UI_EVENTS_DEVICES_HAPTIC_TOUCHPAD_EFFECTS_H_

namespace ui {

enum class HapticTouchpadEffect {
  kSnap = 0,         // UI feedback for snapping into place.
  kSnapReverse = 1,  // UI feedback for snapping out of place.
  kKnock = 2,        // UI feedback for reaching a limit or boundary.
  kTick = 3,         // UI feedback for discrete state changes.
  kToggleOn = 4,     // UI feedback for activating a feature.
  kToggleOff = 5,    // UI feedback for deactivating a feature.
  kPress = 6,        // Standard touchpad button down effect.
  kRelease = 7,      // Standard touchpad button up effect.
  kDeepPress = 8,    // Deeper (more force) touchpad button down effect.
  kDeepRelease = 9   // Deeper (more force) touchpad button up effect.
};

// Modifies the intensity of effects.
enum class HapticTouchpadEffectStrength { kLow = 1, kMedium = 3, kHigh = 5 };

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_HAPTIC_TOUCHPAD_EFFECTS_H_
