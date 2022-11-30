// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_LAYOUT_KEYBOARD_LAYOUT_ENGINE_TEST_UTILS_H_
#define UI_EVENTS_OZONE_LAYOUT_KEYBOARD_LAYOUT_ENGINE_TEST_UTILS_H_

namespace ui {

// Helper for tests that require the keyboard layout to be fully initialised.
//
// The platform may set the keyboard layout asynchronously, but the layout is
// required when handling key events.  Tests that do not manipulate the keyboard
// layout configuration directly may use this helper.
//
// See crbug.com/1186996
void WaitUntilLayoutEngineIsReadyForTest();

}  // namespace ui

#endif  // UI_EVENTS_OZONE_LAYOUT_KEYBOARD_LAYOUT_ENGINE_TEST_UTILS_H_
