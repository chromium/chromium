// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_HAPTIC_TOUCHPAD_EFFECTS_H_
#define UI_EVENTS_DEVICES_HAPTIC_TOUCHPAD_EFFECTS_H_

namespace ui {

enum class HapticTouchpadEffect {
  kSnap = 0,    // UI feedback for snapping into place.
  kKnock = 1,   // UI feedback for reaching a limit or boundary.
  kTick = 2,    // UI feedback for discrete state changes.
  kPress = 3,   // Standard touchpad button down effect.
  kRelease = 4  // Standard touchpad button up effect.
};

// Modifies the intensity of effects.
enum class HapticTouchpadEffectStrength { kLow = 1, kMedium = 3, kHigh = 5 };

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_HAPTIC_TOUCHPAD_EFFECTS_H_
