// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ASH_EVENT_REWRITER_METRICS_H_
#define UI_EVENTS_ASH_EVENT_REWRITER_METRICS_H_

#include "ui/events/keycodes/dom/dom_code.h"

namespace ui {

class KeyboardCapability;

// Enum used to record the usage of the modifier keys on all devices. Do not
// edit the ordering of the values.
enum class ModifierKeyUsageMetric {
  kMetaLeft = 0,
  kMetaRight = 1,
  kControlLeft = 2,
  kControlRight = 3,
  kAltLeft = 4,
  kAltRight = 5,
  kShiftLeft = 6,
  kShiftRight = 7,
  kCapsLock = 8,
  kBackspace = 9,
  kEscape = 10,
  kAssistant = 11,
  kMaxValue = kAssistant
};

// Records when modifier keys are pressed to metrics for tracking usage of
// various metrics before and after remapping.
void RecordModifierKeyPressedBeforeRemapping(
    const KeyboardCapability& keyboard_capability,
    int device_id,
    DomCode dom_code);
void RecordModifierKeyPressedAfterRemapping(
    const KeyboardCapability& keyboard_capability,
    int device_id,
    DomCode dom_code);

}  // namespace ui

#endif  // UI_EVENTS_ASH_EVENT_REWRITER_METRICS_H_
