// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MAC_TEST_TEST_SCREEN_MAC_H_
#define UI_DISPLAY_MAC_TEST_TEST_SCREEN_MAC_H_

#include "ui/display/test/test_screen.h"

namespace gfx {
class Size;
}

namespace display {

namespace test {

// A test screen implementation for Mac.
// It implements the minimal functionalities such as using a valid display id to
// create a display for the screen.
class TestScreenMac : public TestScreen {
 public:
  TestScreenMac(const gfx::Size& size);
  TestScreenMac(const TestScreenMac&) = delete;
  TestScreenMac& operator=(const TestScreenMac&) = delete;
  ~TestScreenMac() override;
};

}  // namespace test
}  // namespace display

#endif  // UI_DISPLAY_MAC_TEST_TEST_SCREEN_MAC_H_
