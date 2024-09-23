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
  kFunction = 12,
  kRightAlt = 13,
  kMaxValue = kRightAlt
};

// Enum used to record the source of function key rewrites.
enum class InputKeyEventToFunctionKey : uint32_t {
  kDirectlyFromKeyboard = 0,    // The keyboard sends Function key and we do
                                // not touch it.
  kTopRowAutoTranslated = 1,    // The keyboard sends top row key and we
                                // automatically remap it to Function key.
  kSearchTopRowTranslated = 2,  // The keyboard sends top row key + search and
                                // we remap it to Function key with no search.
  kDirectlyWithSearch = 3,      // the keyboard sends Function key + Search and
                                // we remap it to Function key with no search.
  kSearchDigitTranslated = 4,   // The keyboard sends digit + Search and we
                                // remap it to Function key.
  kMaxValue = 4,
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
    DomCode dom_code,
    DomCode original_dom_code,
    bool is_right_alt_key);

}  // namespace ui

#endif  // UI_EVENTS_ASH_EVENT_REWRITER_METRICS_H_
