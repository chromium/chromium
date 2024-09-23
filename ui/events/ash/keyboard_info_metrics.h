// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ASH_KEYBOARD_INFO_METRICS_H_
#define UI_EVENTS_ASH_KEYBOARD_INFO_METRICS_H_

#include "ui/events/ash/keyboard_capability.h"

namespace ui {

class KeyboardCapability;

// Enum to represent the 1-indexed
// `ui::KeyboardCapability::KeyboardTopRowLayout` in histograms, since enum
// histograms are required to use 0-indexed enums. This enum also adds an
// additional value "kLayout2WithAssistant". Note that this enum has been
// reordered and should not be considered interchangeable with
// ui::KeyboardCapability::KeyboardTopRowLayout.
//
// This enum should mirror the enum `KeyboardTopRowLayout` in
// tools/metrics/histograms/enums.xml and values should not be changed.
enum class KeyboardTopRowLayoutForMetric {
  kLayoutUnknown = 0,
  kLayout1 = 1,
  kLayout2 = 2,
  kLayout2WithAssistant = 3,
  kLayout3 = 4,
  kLayout4 = 5,
  kLayoutCustom1 = 6,
  kLayoutCustom2 = 7,
  kMaxValue = kLayoutCustom2,
};

// This enum is used to track metrics around how keys are used and comparing it
// against their physical presence on the device.
enum class KeyUsageCategory {
  kPhysicallyPresent = 0,
  kVirtuallyPresent,
  kPhysicallyPressed,
  kVirtuallyPressed,
  kMaxValue = kVirtuallyPressed,
};

void RecordKeyboardInfoMetrics(
    const KeyboardCapability::KeyboardInfo& keyboard_info,
    bool has_assistant_key,
    bool has_right_alt_key);

}  // namespace ui

#endif  // UI_EVENTS_ASH_KEYBOARD_INFO_METRICS_H_
