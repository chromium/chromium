// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_TEST_DESKTOP_SCREEN_OZONE_H_
#define UI_VIEWS_TEST_TEST_DESKTOP_SCREEN_OZONE_H_

#include <memory>

#include "ui/gfx/geometry/point.h"
#include "ui/views/widget/desktop_aura/desktop_screen_ozone.h"

namespace display {
class Screen;
}

namespace views::test {

// Replaces the screen instance in Linux/Ozone (non-ChromeOS) tests.  Allows
// aura tests to manually set the cursor screen point to be reported
// by GetCursorScreenPoint().  Needed because of a limitation in the
// X11 protocol that restricts us from warping the pointer with the
// mouse button held down.
class TestDesktopScreenOzone : public views::DesktopScreenOzone {
 public:
  TestDesktopScreenOzone(const TestDesktopScreenOzone&) = delete;
  TestDesktopScreenOzone& operator=(const TestDesktopScreenOzone&) = delete;

  static std::unique_ptr<display::Screen> Create();
  static TestDesktopScreenOzone* GetInstance();

  // DesktopScreenOzone:
  gfx::Point GetCursorScreenPoint() override;

  void set_cursor_screen_point(const gfx::Point& point) {
    cursor_screen_point_ = point;
  }

  TestDesktopScreenOzone();
  ~TestDesktopScreenOzone() override;

 private:
  gfx::Point cursor_screen_point_;
};

}  // namespace views::test

#endif  // UI_VIEWS_TEST_TEST_DESKTOP_SCREEN_OZONE_H_
