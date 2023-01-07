// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TEST_KEYBOARD_HOOK_MONITOR_UTILS_H_
#define UI_EVENTS_TEST_KEYBOARD_HOOK_MONITOR_UTILS_H_

namespace ui {

// Simulates a KeyboardHook being registered by calling the appropriate method
// on the KeyboardHookMonitorImpl instance.
// This method is exposed to allow testing this behavior outside of //ui/events.
void SimulateKeyboardHookRegistered();

// Simulates a KeyboardHook being unregistered by calling the appropriate method
// on the KeyboardHookMonitorImpl instance.
// This method is exposed to allow testing this behavior outside of //ui/events.
void SimulateKeyboardHookUnregistered();

}  // namespace ui

#endif  // UI_EVENTS_TEST_KEYBOARD_HOOK_MONITOR_UTILS_H_
