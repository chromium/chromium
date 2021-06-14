// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ui/display/test/test_screen.h"

namespace display {
namespace test {

TestScreen::TestScreen() {
  Display display(1, gfx::Rect(0, 0, 100, 100));
  ProcessDisplayChanged(display, true /* is_primary */);
}

TestScreen::~TestScreen() {}

void TestScreen::set_cursor_screen_point(const gfx::Point& point) {
  cursor_screen_point_ = point;
}

gfx::Point TestScreen::GetCursorScreenPoint() {
  return cursor_screen_point_;
}

bool TestScreen::IsWindowUnderCursor(gfx::NativeWindow window) {
  return false;
}

gfx::NativeWindow TestScreen::GetWindowAtScreenPoint(const gfx::Point& point) {
  return nullptr;
}

Display TestScreen::GetDisplayNearestWindow(gfx::NativeWindow window) const {
  return GetPrimaryDisplay();
}

}  // namespace test
}  // namespace display
