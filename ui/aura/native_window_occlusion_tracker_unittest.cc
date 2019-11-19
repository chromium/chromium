// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/native_window_occlusion_tracker_win.h"

#include <winuser.h>

#include "base/win/scoped_gdi_object.h"
#include "base/win/windows_version.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/win/window_impl.h"

#include "dwmapi.h"

namespace aura {

// Test wrapper around native window HWND.
class TestNativeWindow : public gfx::WindowImpl {
 public:
  TestNativeWindow() {}
  ~TestNativeWindow() override;

 private:
  // Overridden from gfx::WindowImpl:
  BOOL ProcessWindowMessage(HWND window,
                            UINT message,
                            WPARAM w_param,
                            LPARAM l_param,
                            LRESULT& result,
                            DWORD msg_map_id) override {
    return FALSE;  // Results in DefWindowProc().
  }

  DISALLOW_COPY_AND_ASSIGN(TestNativeWindow);
};

TestNativeWindow::~TestNativeWindow() {
  if (hwnd())
    DestroyWindow(hwnd());
}

// This class currently tests the behavior of
// NativeWindowOcclusionTrackerWin::IsWindowVisibleAndFullyOpaque with hwnds
// with various attributes (e.g., minimized, transparent, etc).
class NativeWindowOcclusionTrackerTest : public test::AuraTestBase {
 public:
  NativeWindowOcclusionTrackerTest() {}

  TestNativeWindow* native_win() { return native_win_.get(); }

  HWND CreateNativeWindow(DWORD ex_style) {
    native_win_ = std::make_unique<TestNativeWindow>();
    native_win_->set_window_style(WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN);
    native_win_->set_window_ex_style(ex_style);
    gfx::Rect bounds(0, 0, 100, 100);
    native_win_->Init(nullptr, bounds);
    HWND hwnd = native_win_->hwnd();
    base::win::ScopedRegion region(CreateRectRgn(0, 0, 0, 0));
    if (GetWindowRgn(hwnd, region.get()) == COMPLEXREGION) {
      // On Windows 7, the newly created window has a complex region, which
      // means it will be ignored during the occlusion calculation. So, force
      // it to have a simple region so that we get test coverage on win 7.
      RECT bounding_rect;
      EXPECT_TRUE(GetWindowRect(hwnd, &bounding_rect));
      base::win::ScopedRegion rectangular_region(
          CreateRectRgnIndirect(&bounding_rect));
      SetWindowRgn(hwnd, rectangular_region.get(), /*redraw=*/TRUE);
    }
    ShowWindow(hwnd, SW_SHOWNORMAL);
    EXPECT_TRUE(UpdateWindow(hwnd));
    return hwnd;
  }

  // Wrapper around IsWindowVisibleAndFullyOpaque so only the test class
  // needs to be a friend of NativeWindowOcclusionTrackerWin.
  bool CheckWindowVisibleAndFullyOpaque(HWND hwnd, gfx::Rect* win_rect) {
    bool ret = NativeWindowOcclusionTrackerWin::IsWindowVisibleAndFullyOpaque(
        hwnd, win_rect);
    // In general, if IsWindowVisibleAndFullyOpaque returns false, the
    // returned rect should not be altered.
    if (!ret)
      EXPECT_EQ(*win_rect, gfx::Rect(0, 0, 0, 0));
    return ret;
  }

 private:
  std::unique_ptr<TestNativeWindow> native_win_;

  DISALLOW_COPY_AND_ASSIGN(NativeWindowOcclusionTrackerTest);
};

TEST_F(NativeWindowOcclusionTrackerTest, VisibleOpaqueWindow) {
  HWND hwnd = CreateNativeWindow(/*ex_style=*/0);
  gfx::Rect returned_rect;
  // Normal windows should be visible.
  EXPECT_TRUE(CheckWindowVisibleAndFullyOpaque(hwnd, &returned_rect));

  // Check that the returned rect == the actual window rect of the hwnd.
  RECT win_rect;
  ASSERT_TRUE(GetWindowRect(hwnd, &win_rect));
  EXPECT_EQ(returned_rect, gfx::Rect(win_rect));
}

TEST_F(NativeWindowOcclusionTrackerTest, MinimizedWindow) {
  HWND hwnd = CreateNativeWindow(/*ex_style=*/0);
  gfx::Rect win_rect;
  ShowWindow(hwnd, SW_MINIMIZE);
  // Minimized windows are not considered visible.
  EXPECT_FALSE(CheckWindowVisibleAndFullyOpaque(hwnd, &win_rect));
}

TEST_F(NativeWindowOcclusionTrackerTest, TransparentWindow) {
  HWND hwnd = CreateNativeWindow(WS_EX_TRANSPARENT);
  gfx::Rect win_rect;
  // Transparent windows are not considered visible and opaque.
  EXPECT_FALSE(CheckWindowVisibleAndFullyOpaque(hwnd, &win_rect));
}

TEST_F(NativeWindowOcclusionTrackerTest, ToolWindow) {
  HWND hwnd = CreateNativeWindow(WS_EX_TOOLWINDOW);
  gfx::Rect win_rect;
  // Tool windows are not considered visible and opaque.
  EXPECT_FALSE(CheckWindowVisibleAndFullyOpaque(hwnd, &win_rect));
}

TEST_F(NativeWindowOcclusionTrackerTest, LayeredAlphaWindow) {
  HWND hwnd = CreateNativeWindow(WS_EX_LAYERED);
  gfx::Rect win_rect;
  BYTE alpha = 1;
  DWORD flags = LWA_ALPHA;
  COLORREF color_ref = RGB(1, 1, 1);
  SetLayeredWindowAttributes(hwnd, color_ref, alpha, flags);
  // Layered windows with alpha < 255 are not considered visible and opaque.
  EXPECT_FALSE(CheckWindowVisibleAndFullyOpaque(hwnd, &win_rect));
}

TEST_F(NativeWindowOcclusionTrackerTest, LayeredNonAlphaWindow) {
  HWND hwnd = CreateNativeWindow(WS_EX_LAYERED);
  gfx::Rect win_rect;
  BYTE alpha = 1;
  DWORD flags = 0;
  COLORREF color_ref = RGB(1, 1, 1);
  SetLayeredWindowAttributes(hwnd, color_ref, alpha, flags);
  // Layered non alpha windows are considered visible and opaque.
  EXPECT_TRUE(CheckWindowVisibleAndFullyOpaque(hwnd, &win_rect));
}

TEST_F(NativeWindowOcclusionTrackerTest, ComplexRegionWindow) {
  HWND hwnd = CreateNativeWindow(/*ex_style=*/0);
  gfx::Rect win_rect;
  // Create a region with rounded corners, which should be a complex region.
  base::win::ScopedRegion region(CreateRoundRectRgn(1, 1, 100, 100, 5, 5));
  SetWindowRgn(hwnd, region.get(), /*redraw=*/TRUE);
  // Windows with complex regions are not considered visible and fully opaque.
  EXPECT_FALSE(CheckWindowVisibleAndFullyOpaque(hwnd, &win_rect));
}

TEST_F(NativeWindowOcclusionTrackerTest, CloakedWindow) {
  // Cloaking is only supported in Windows 8 and above.
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;
  HWND hwnd = CreateNativeWindow(/*ex_style=*/0);
  gfx::Rect win_rect;
  BOOL cloak = TRUE;
  DwmSetWindowAttribute(hwnd, DWMWA_CLOAK, &cloak, sizeof(cloak));
  // Cloaked Windows are not considered visible.
  EXPECT_FALSE(CheckWindowVisibleAndFullyOpaque(hwnd, &win_rect));
}

}  // namespace aura
