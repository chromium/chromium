// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_LAYOUT_KEYBOARD_LAYOUT_ENGINE_TEST_UTILS_H_
#define UI_EVENTS_OZONE_LAYOUT_KEYBOARD_LAYOUT_ENGINE_TEST_UTILS_H_

namespace ui {

// Helper for tests that require the layout to be ready when handling key
// events.
void WaitUntilLayoutEngineIsReadyForTest();

}  // namespace ui

#endif  // UI_EVENTS_OZONE_LAYOUT_KEYBOARD_LAYOUT_ENGINE_TEST_UTILS_H_
