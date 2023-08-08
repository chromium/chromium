// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_KEYBOARD_MOUSE_COMBO_DEVICE_METRICS_H_
#define UI_EVENTS_OZONE_EVDEV_KEYBOARD_MOUSE_COMBO_DEVICE_METRICS_H_

namespace ui {

// Enum for combo device classification metrics.
// This enum should mirror the enum `ComboDeviceClassification` in
// tools/metrics/histograms/enums.xml and values should not be changed.
enum class ComboDeviceClassification {
  kKnownKeyboardImposter,
  kKnownMouseImposter,
  kKnownComboDevice,
  kUnknown,
  kMaxValue = kUnknown
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_KEYBOARD_MOUSE_COMBO_DEVICE_METRICS_H_
