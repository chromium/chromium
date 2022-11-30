// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_utils.h"

#include <windows.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/win/window_impl.h"

namespace ui {

namespace {

class TestWindow : public gfx::WindowImpl {
 public:
  TestWindow() {}

  TestWindow(const TestWindow&) = delete;
  TestWindow& operator=(const TestWindow&) = delete;

  ~TestWindow() override {}

  BOOL ProcessWindowMessage(HWND window,
                            UINT message,
                            WPARAM w_param,
                            LPARAM l_param,
                            LRESULT& result,
                            DWORD msg_map_id = 0) override {
    // Handle WM_NCCALCSIZE because the test below assumes there is no
    // non-client area, affecting EventSystemLocationFromNative's client to
    // screen coordinate transform.
    return message == WM_NCCALCSIZE;
  }
};

CHROME_MSG CreateEvent(UINT type, WORD x, WORD y, HWND hwnd) {
  CHROME_MSG event;
  event.message = type;
  event.hwnd = hwnd;
  event.lParam = MAKELPARAM(x, y);
  return event;
}

TEST(EventWinTest, EventSystemLocationFromNative) {
  TestWindow test_window;
  const WORD x_coord = 10;
  const WORD y_coord = 20;
  const WORD x_window_offset = 100;
  const WORD y_window_offset = 100;
  test_window.Init(nullptr,
                   gfx::Rect(x_window_offset, y_window_offset, 100, 100));
  EXPECT_TRUE(test_window.hwnd() != nullptr);

  {
    CHROME_MSG event =
        CreateEvent(WM_MOUSEWHEEL, x_coord, y_coord, test_window.hwnd());
    // Mouse wheel events already have screen coordinates so they should not be
    // converted.
    EXPECT_EQ(gfx::Point(x_coord, y_coord),
              EventSystemLocationFromNative(event));
  }

  CHROME_MSG event =
      CreateEvent(WM_LBUTTONDOWN, x_coord, y_coord, test_window.hwnd());
  EXPECT_EQ(gfx::Point(x_coord + x_window_offset, y_coord + y_window_offset),
            EventSystemLocationFromNative(event));
}

}  // namespace

}  // namespace ui
