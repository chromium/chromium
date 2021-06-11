// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TEST_TEST_SCREEN_H_
#define UI_DISPLAY_TEST_TEST_SCREEN_H_

#include "ui/display/screen_base.h"
#include "ui/gfx/geometry/rect.h"

namespace display {

namespace test {

// A fake and simplified Screen implementation that contains a single
// Display only with the default size.
class TestScreen : public ScreenBase {
 public:
  static constexpr gfx::Rect kDefaultScreenBounds = gfx::Rect(0, 0, 800, 600);

  // TODO(weili): Split this into a protected no-argument constructor for
  // subclass uses and the public one with gfx::Size argument.
  explicit TestScreen(bool create_display = true);
  TestScreen(const TestScreen&) = delete;
  TestScreen& operator=(const TestScreen&) = delete;
  ~TestScreen() override;

  // ScreenBase:
  gfx::Point GetCursorScreenPoint() override;
  bool IsWindowUnderCursor(gfx::NativeWindow window) override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  Display GetDisplayNearestWindow(gfx::NativeWindow window) const override;
  Display GetDisplayMatching(const gfx::Rect& match_rect) const override;
  void SetCursorScreenPointForTesting(const gfx::Point& point) override;

 private:
  gfx::Point cursor_screen_point_;
};

}  // namespace test
}  // namespace display

#endif  // UI_DISPLAY_TEST_TEST_SCREEN_H_
