// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/test/test_screen.h"

#include <vector>

#include "ui/display/display.h"

namespace display {
namespace test {

// static
constexpr gfx::Rect TestScreen::kDefaultScreenBounds;

TestScreen::TestScreen(bool create_display) {
  if (!create_display)
    return;
  Display display(1, kDefaultScreenBounds);
  ProcessDisplayChanged(display, /* is_primary = */ true);
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

void TestScreen::SetCursorScreenPointForTesting(const gfx::Point& point) {
  cursor_screen_point_ = point;
}

}  // namespace test
}  // namespace display
