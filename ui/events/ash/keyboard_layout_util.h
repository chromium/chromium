// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ASH_KEYBOARD_LAYOUT_UTIL_H_
#define UI_EVENTS_ASH_KEYBOARD_LAYOUT_UTIL_H_

namespace ui {

// TODO(dpad): Remove this function once `KeyboardCapability` fully supports the
// HasAssistantKey API. Returns true if one of the keyboards currently connected
// to the device has an Assistant key.
bool DeviceKeyboardHasAssistantKey();

}  // namespace ui

#endif  // UI_EVENTS_ASH_KEYBOARD_LAYOUT_UTIL_H_
