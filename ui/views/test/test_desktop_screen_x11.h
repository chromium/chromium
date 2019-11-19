// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_TEST_DESKTOP_SCREEN_X11_H_
#define UI_VIEWS_TEST_TEST_DESKTOP_SCREEN_X11_H_

#include "ui/gfx/geometry/point.h"
#include "ui/views/widget/desktop_aura/desktop_screen_x11.h"

namespace base {
template<typename T> struct DefaultSingletonTraits;
}

namespace views {
namespace test {

// Replaces the screen instance in Linux (non-ChromeOS) tests.  Allows
// aura tests to manually set the cursor screen point to be reported
// by GetCursorScreenPoint().  Needed because of a limitation in the
// X11 protocol that restricts us from warping the pointer with the
// mouse button held down.
class TestDesktopScreenX11 : public DesktopScreenX11 {
 public:
  static TestDesktopScreenX11* GetInstance();

  // DesktopScreenX11:
  gfx::Point GetCursorScreenPoint() override;

  void set_cursor_screen_point(const gfx::Point& point) {
    cursor_screen_point_ = point;
  }

 private:
  friend struct base::DefaultSingletonTraits<TestDesktopScreenX11>;

  TestDesktopScreenX11();
  ~TestDesktopScreenX11() override;

  gfx::Point cursor_screen_point_;

  DISALLOW_COPY_AND_ASSIGN(TestDesktopScreenX11);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_TEST_TEST_DESKTOP_SCREEN_X11_H_
