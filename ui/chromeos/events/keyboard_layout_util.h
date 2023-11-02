// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_EVENTS_KEYBOARD_LAYOUT_UTIL_H_
#define UI_CHROMEOS_EVENTS_KEYBOARD_LAYOUT_UTIL_H_

namespace ui {

// Returns true if the device is currently connected to any keyboard (internal
// or external) that is using the 2017 keyboard layout.
bool DeviceUsesKeyboardLayout2();

// Returns true if one of the keyboards currently connected to the device has
// an Assistant key.
bool DeviceKeyboardHasAssistantKey();

}  // namespace ui

#endif  // UI_CHROMEOS_EVENTS_KEYBOARD_LAYOUT_UTIL_H_
