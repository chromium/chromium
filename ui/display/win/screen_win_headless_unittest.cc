// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/screen_win_headless.h"

#include <windows.h>

#include <algorithm>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "components/headless/screen_info/headless_screen_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/win/screen_win_display.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/native_widget_types.h"

using headless::HeadlessScreenInfo;

namespace display::win {

namespace {

class TestScreenWinHeadless : public ScreenWinHeadless {
 public:
  explicit TestScreenWinHeadless(
      const std::vector<HeadlessScreenInfo>& screen_infos)
      : ScreenWinHeadless(screen_infos) {}

  TestScreenWinHeadless(const TestScreenWinHeadless&) = delete;
  TestScreenWinHeadless& operator=(const TestScreenWinHeadless&) = delete;

  ~TestScreenWinHeadless() override = default;

  gfx::NativeWindow AddWindow(const gfx::Rect& bounds) {
    HWND hwnd = reinterpret_cast<HWND>(++last_hwnd_);
    windows_.insert({hwnd, bounds});
    return GetNativeWindowFromHWND(hwnd);
  }

  // win::ScreenWin:
  HWND GetHWNDFromNativeWindow(gfx::NativeWindow window) const override {
    return reinterpret_cast<HWND>(window);
  }

  gfx::NativeWindow GetNativeWindowFromHWND(HWND hwnd) const override {
    return reinterpret_cast<gfx::NativeWindow>(hwnd);
  }

  // win::ScreenWinHeadless:
  gfx::NativeWindow GetNativeWindowAtScreenPoint(
      const gfx::Point& point,
      const std::set<gfx::NativeWindow>& ignore) const override {
    gfx::NativeWindow result = nullptr;
    // Assume that more recently created windows are higher in z-order.
    for (const auto& [hwnd, bounds] : windows_) {
      gfx::NativeWindow window = GetNativeWindowFromHWND(hwnd);
      if (bounds.Contains(point) && ignore.find(window) == ignore.cend()) {
        result = window;
      }
    }

    return result;
  }

  gfx::Rect GetNativeWindowBoundsInScreen(
      gfx::NativeWindow window) const override {
    return GetWindowBounds(GetHWNDFromNativeWindow(window));
  }

  gfx::Rect GetHeadlessWindowBounds(
      gfx::AcceleratedWidget window) const override {
    return GetWindowBounds(window);
  }

  gfx::NativeWindow GetRootWindow(gfx::NativeWindow window) const override {
    return window;
  }

 private:
  gfx::Rect GetWindowBounds(HWND hwnd) const {
    CHECK(hwnd);
    auto it = windows_.find(hwnd);
    CHECK(it != windows_.cend());
    return it->second;
  }

  // A sequentially increasing integer value used as a substitute for a window
  // handle.
  int last_hwnd_ = 0;

  base::flat_map<HWND, gfx::Rect> windows_;
};

class ScreenWinHeadlessTest : public testing::Test {
 public:
  ScreenWinHeadlessTest(const ScreenWinHeadlessTest&) = delete;
  ScreenWinHeadlessTest& operator=(const ScreenWinHeadlessTest&) = delete;

 protected:
  ScreenWinHeadlessTest() = default;
  ~ScreenWinHeadlessTest() override = default;

  // Return specified or default headless screen configuration.
  std::vector<HeadlessScreenInfo> GetScreenInfos(
      std::string_view screen_info_spec) {
    std::vector<HeadlessScreenInfo> screen_infos;

    if (!screen_info_spec.empty()) {
      auto screen_info_or_error =
          HeadlessScreenInfo::FromString(screen_info_spec);
      CHECK(screen_info_or_error.has_value()) << screen_info_or_error.error();
      screen_infos = screen_info_or_error.value();
    } else {
      screen_infos.push_back(HeadlessScreenInfo());
    }

    return screen_infos;
  }

  std::unique_ptr<TestScreenWinHeadless> CreateHeadlessScreen(
      std::string_view screen_info_spec) {
    return std::make_unique<TestScreenWinHeadless>(
        GetScreenInfos(screen_info_spec));
  }
};

TEST_F(ScreenWinHeadlessTest, DefaultScreen) {
  auto screen = CreateHeadlessScreen("");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  const Display& display = screen->GetAllDisplays()[0];
  EXPECT_THAT(display.bounds(), testing::Eq(gfx::Rect(0, 0, 800, 600)));
  EXPECT_THAT(display.work_area(), testing::Eq(gfx::Rect(0, 0, 800, 600)));
  EXPECT_THAT(display.device_scale_factor(), testing::Eq(1.0f));
  EXPECT_THAT(display.rotation(), testing::Eq(Display::Rotation::ROTATE_0));
  EXPECT_THAT(display.color_depth(), testing::Eq(24));
  EXPECT_TRUE(display.label().empty());
  EXPECT_FALSE(display.IsInternal());
}

TEST_F(ScreenWinHeadlessTest, SpecifiedScreen) {
  auto screen = CreateHeadlessScreen("{1600x1200 label='Primary Screen'}");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  const Display& display = screen->GetAllDisplays()[0];
  EXPECT_THAT(display.bounds(), testing::Eq(gfx::Rect(0, 0, 1600, 1200)));
  EXPECT_THAT(display.work_area(), testing::Eq(gfx::Rect(0, 0, 1600, 1200)));
  EXPECT_THAT(display.device_scale_factor(), testing::Eq(1.0f));
  EXPECT_THAT(display.rotation(), testing::Eq(Display::Rotation::ROTATE_0));
  EXPECT_THAT(display.color_depth(), testing::Eq(24));
  EXPECT_THAT(display.label(), testing::Eq("Primary Screen"));
  EXPECT_FALSE(display.IsInternal());
}

TEST_F(ScreenWinHeadlessTest, MultipleScreens) {
  auto screen = CreateHeadlessScreen("{label=#1}{600x800 label=#2}");
  ASSERT_EQ(screen->GetNumDisplays(), 2);

  const Display& display1 = screen->GetAllDisplays()[0];
  EXPECT_THAT(display1.bounds(), testing::Eq(gfx::Rect(0, 0, 800, 600)));
  EXPECT_THAT(display1.rotation(), testing::Eq(Display::Rotation::ROTATE_0));
  EXPECT_THAT(display1.label(), testing::Eq("#1"));

  const Display& display2 = screen->GetAllDisplays()[1];
  EXPECT_THAT(display2.bounds(), testing::Eq(gfx::Rect(800, 0, 600, 800)));
  EXPECT_THAT(display2.rotation(), testing::Eq(Display::Rotation::ROTATE_0));
  EXPECT_THAT(display2.label(), testing::Eq("#2"));
}

// ScreenWin::MonitorInfoFrom*() overrides tests ----------------------

TEST_F(ScreenWinHeadlessTest, MonitorInfoFromScreenPoint) {
  auto screen = CreateHeadlessScreen("{}{}");
  ASSERT_EQ(screen->GetNumDisplays(), 2);

  EXPECT_EQ(screen->MonitorInfoFromScreenPoint(gfx::Point(-1, -1))->dwFlags,
            static_cast<DWORD>(MONITORINFOF_PRIMARY));

  EXPECT_EQ(screen->MonitorInfoFromScreenPoint(gfx::Point(0, 0))->dwFlags,
            static_cast<DWORD>(MONITORINFOF_PRIMARY));

  EXPECT_EQ(screen->MonitorInfoFromScreenPoint(gfx::Point(799, 300))->dwFlags,
            static_cast<DWORD>(MONITORINFOF_PRIMARY));

  EXPECT_EQ(screen->MonitorInfoFromScreenPoint(gfx::Point(1599, 599))->dwFlags,
            static_cast<DWORD>(0));

  EXPECT_EQ(screen->MonitorInfoFromScreenPoint(gfx::Point(1601, 601))->dwFlags,
            static_cast<DWORD>(0));
}

TEST_F(ScreenWinHeadlessTest, MonitorInfoFromScreenRect) {
  auto screen = CreateHeadlessScreen("{}{}");
  ASSERT_EQ(screen->GetNumDisplays(), 2);

  EXPECT_EQ(screen->MonitorInfoFromScreenRect(gfx::Rect(-200, -100, 200, 100))
                ->dwFlags,
            static_cast<DWORD>(MONITORINFOF_PRIMARY));

  EXPECT_EQ(
      screen->MonitorInfoFromScreenRect(gfx::Rect(0, 0, 200, 100))->dwFlags,
      static_cast<DWORD>(MONITORINFOF_PRIMARY));

  EXPECT_EQ(
      screen->MonitorInfoFromScreenRect(gfx::Rect(400, 300, 200, 100))->dwFlags,
      static_cast<DWORD>(MONITORINFOF_PRIMARY));

  EXPECT_EQ(screen->MonitorInfoFromScreenRect(gfx::Rect(1500, 500, 200, 100))
                ->dwFlags,
            static_cast<DWORD>(0));

  EXPECT_EQ(screen->MonitorInfoFromScreenRect(gfx::Rect(1601, 601, 200, 100))
                ->dwFlags,
            static_cast<DWORD>(0));
}

TEST_F(ScreenWinHeadlessTest, MonitorInfoFromWindow) {
  auto screen = CreateHeadlessScreen("{}{}");
  ASSERT_EQ(screen->GetNumDisplays(), 2);

  // Window is on the primary screen.
  HWND hwnd1 = screen->GetHWNDFromNativeWindow(
      screen->AddWindow(gfx::Rect(0, 0, 400, 300)));
  EXPECT_EQ(
      screen->MonitorInfoFromWindow(hwnd1, MONITOR_DEFAULTTONULL)->dwFlags,
      static_cast<DWORD>(MONITORINFOF_PRIMARY));

  // Window is on the secondary screen.
  HWND hwnd2 = screen->GetHWNDFromNativeWindow(
      screen->AddWindow(gfx::Rect(800, 0, 400, 300)));
  EXPECT_EQ(
      screen->MonitorInfoFromWindow(hwnd2, MONITOR_DEFAULTTONULL)->dwFlags,
      static_cast<DWORD>(0));

  // Window is north west of the primary screen.
  HWND hwnd3 = screen->GetHWNDFromNativeWindow(
      screen->AddWindow(gfx::Rect(-400, -300, 400, 300)));
  EXPECT_FALSE(
      screen->MonitorInfoFromWindow(hwnd3, MONITOR_DEFAULTTONULL).has_value());
  EXPECT_EQ(
      screen->MonitorInfoFromWindow(hwnd3, MONITOR_DEFAULTTONEAREST)->dwFlags,
      static_cast<DWORD>(MONITORINFOF_PRIMARY));

  // Window is south east of the secondary screen.
  HWND hwnd4 = screen->GetHWNDFromNativeWindow(
      screen->AddWindow(gfx::Rect(1600, 600, 400, 300)));
  EXPECT_FALSE(
      screen->MonitorInfoFromWindow(hwnd4, MONITOR_DEFAULTTONULL).has_value());
  EXPECT_EQ(
      screen->MonitorInfoFromWindow(hwnd4, MONITOR_DEFAULTTONEAREST)->dwFlags,
      static_cast<DWORD>(0));
  EXPECT_EQ(
      screen->MonitorInfoFromWindow(hwnd4, MONITOR_DEFAULTTOPRIMARY)->dwFlags,
      static_cast<DWORD>(MONITORINFOF_PRIMARY));
}

// display::win::ScreenWin static methods tests -----------------------

TEST_F(ScreenWinHeadlessTest, ScreenToDIPPoint) {
  auto screen = CreateHeadlessScreen("{devicePixelRatio=1.0}");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  EXPECT_EQ(GetScreenWin()->ScreenToDIPPoint(gfx::PointF(100, 200)),
            gfx::PointF(100, 200));
}

TEST_F(ScreenWinHeadlessTest, ScreenToDIPPoint2x) {
  auto screen = CreateHeadlessScreen("{devicePixelRatio=2.0}");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  EXPECT_EQ(GetScreenWin()->ScreenToDIPPoint(gfx::PointF(100, 200)),
            gfx::PointF(50, 100));
}

TEST_F(ScreenWinHeadlessTest, DIPToScreenPoint) {
  auto screen = CreateHeadlessScreen("{devicePixelRatio=1.0}");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  EXPECT_EQ(GetScreenWin()->DIPToScreenPoint(gfx::Point(100, 200)),
            gfx::Point(100, 200));
}

TEST_F(ScreenWinHeadlessTest, DIPToScreenPoint2x) {
  auto screen = CreateHeadlessScreen("{devicePixelRatio=2.0}");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  EXPECT_EQ(GetScreenWin()->DIPToScreenPoint(gfx::Point(100, 200)),
            gfx::Point(200, 400));
}

TEST_F(ScreenWinHeadlessTest, ClientToDIPPoint) {
  auto screen = CreateHeadlessScreen("{devicePixelRatio=1.0}");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  HWND hwnd = screen->GetHWNDFromNativeWindow(
      screen->AddWindow(gfx::Rect(0, 0, 400, 300)));

  EXPECT_EQ(GetScreenWin()->ClientToDIPPoint(hwnd, gfx::Point(100, 200)),
            gfx::Point(100, 200));
}

TEST_F(ScreenWinHeadlessTest, ClientToDIPPoint2x) {
  auto screen = CreateHeadlessScreen("{devicePixelRatio=2.0}");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  HWND hwnd = screen->GetHWNDFromNativeWindow(
      screen->AddWindow(gfx::Rect(0, 0, 400, 300)));

  EXPECT_EQ(GetScreenWin()->ClientToDIPPoint(hwnd, gfx::Point(100, 200)),
            gfx::Point(50, 100));
}

TEST_F(ScreenWinHeadlessTest, DIPToClientPoint) {
  auto screen = CreateHeadlessScreen("{devicePixelRatio=1.0}");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  HWND hwnd = screen->GetHWNDFromNativeWindow(
      screen->AddWindow(gfx::Rect(0, 0, 400, 300)));

  EXPECT_EQ(GetScreenWin()->DIPToClientPoint(hwnd, gfx::Point(100, 200)),
            gfx::Point(100, 200));
}

TEST_F(ScreenWinHeadlessTest, DIPToClientPoint2x) {
  auto screen = CreateHeadlessScreen("{devicePixelRatio=2.0}");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  HWND hwnd = screen->GetHWNDFromNativeWindow(
      screen->AddWindow(gfx::Rect(0, 0, 400, 300)));

  EXPECT_EQ(GetScreenWin()->DIPToClientPoint(hwnd, gfx::Point(50, 100)),
            gfx::Point(100, 200));
}

TEST_F(ScreenWinHeadlessTest, ScreenToDIPRect) {
  auto screen = CreateHeadlessScreen("{devicePixelRatio=1.0}");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  HWND hwnd = screen->GetHWNDFromNativeWindow(
      screen->AddWindow(gfx::Rect(0, 0, 400, 300)));

  EXPECT_EQ(GetScreenWin()->ScreenToDIPRect(hwnd, gfx::Rect(0, 0, 100, 200)),
            gfx::Rect(0, 0, 100, 200));

  EXPECT_EQ(GetScreenWin()->ScreenToDIPRect(nullptr, gfx::Rect(0, 0, 100, 200)),
            gfx::Rect(0, 0, 100, 200));
}

TEST_F(ScreenWinHeadlessTest, ScreenToDIPRect2x) {
  auto screen = CreateHeadlessScreen("{devicePixelRatio=2.0}");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  HWND hwnd = screen->GetHWNDFromNativeWindow(
      screen->AddWindow(gfx::Rect(0, 0, 400, 300)));

  EXPECT_EQ(GetScreenWin()->ScreenToDIPRect(hwnd, gfx::Rect(0, 0, 100, 200)),
            gfx::Rect(0, 0, 50, 100));

  EXPECT_EQ(GetScreenWin()->ScreenToDIPRect(nullptr, gfx::Rect(0, 0, 100, 200)),
            gfx::Rect(0, 0, 50, 100));
}

TEST_F(ScreenWinHeadlessTest, DIPToScreenRect) {
  auto screen = CreateHeadlessScreen("{devicePixelRatio=1.0}");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  HWND hwnd = screen->GetHWNDFromNativeWindow(
      screen->AddWindow(gfx::Rect(0, 0, 400, 300)));

  EXPECT_EQ(GetScreenWin()->DIPToScreenRect(hwnd, gfx::Rect(0, 0, 100, 200)),
            gfx::Rect(0, 0, 100, 200));

  EXPECT_EQ(GetScreenWin()->DIPToScreenRect(nullptr, gfx::Rect(0, 0, 100, 200)),
            gfx::Rect(0, 0, 100, 200));
}

TEST_F(ScreenWinHeadlessTest, DIPToScreenRect2x) {
  auto screen = CreateHeadlessScreen("{devicePixelRatio=2.0}");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  HWND hwnd = screen->GetHWNDFromNativeWindow(
      screen->AddWindow(gfx::Rect(0, 0, 400, 300)));

  EXPECT_EQ(GetScreenWin()->DIPToScreenRect(hwnd, gfx::Rect(0, 0, 50, 100)),
            gfx::Rect(0, 0, 100, 200));

  EXPECT_EQ(GetScreenWin()->DIPToScreenRect(nullptr, gfx::Rect(0, 0, 50, 100)),
            gfx::Rect(0, 0, 100, 200));
}

TEST_F(ScreenWinHeadlessTest, ClientToDIPRect) {
  auto screen = CreateHeadlessScreen("{devicePixelRatio=1.0}");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  HWND hwnd = screen->GetHWNDFromNativeWindow(
      screen->AddWindow(gfx::Rect(0, 0, 400, 300)));

  EXPECT_EQ(GetScreenWin()->ClientToDIPRect(hwnd, gfx::Rect(0, 0, 100, 200)),
            gfx::Rect(0, 0, 100, 200));
}

TEST_F(ScreenWinHeadlessTest, ClientToDIPRect2x) {
  auto screen = CreateHeadlessScreen("{devicePixelRatio=2.0}");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  HWND hwnd = screen->GetHWNDFromNativeWindow(
      screen->AddWindow(gfx::Rect(0, 0, 400, 300)));

  EXPECT_EQ(GetScreenWin()->ClientToDIPRect(hwnd, gfx::Rect(0, 0, 100, 200)),
            gfx::Rect(0, 0, 50, 100));
}

TEST_F(ScreenWinHeadlessTest, DIPToClientRect) {
  auto screen = CreateHeadlessScreen("{devicePixelRatio=1.0}");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  HWND hwnd = screen->GetHWNDFromNativeWindow(
      screen->AddWindow(gfx::Rect(0, 0, 400, 300)));

  EXPECT_EQ(GetScreenWin()->DIPToClientRect(hwnd, gfx::Rect(0, 0, 100, 200)),
            gfx::Rect(0, 0, 100, 200));
}

TEST_F(ScreenWinHeadlessTest, DIPToClientRect2x) {
  auto screen = CreateHeadlessScreen("{devicePixelRatio=2.0}");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  HWND hwnd = screen->GetHWNDFromNativeWindow(
      screen->AddWindow(gfx::Rect(0, 0, 400, 300)));

  EXPECT_EQ(GetScreenWin()->DIPToClientRect(hwnd, gfx::Rect(0, 0, 50, 100)),
            gfx::Rect(0, 0, 100, 200));
}

TEST_F(ScreenWinHeadlessTest, ScreenToDIPSize) {
  auto screen = CreateHeadlessScreen("{devicePixelRatio=1.0}");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  HWND hwnd = screen->GetHWNDFromNativeWindow(
      screen->AddWindow(gfx::Rect(0, 0, 400, 300)));

  EXPECT_EQ(GetScreenWin()->ScreenToDIPSize(hwnd, gfx::Size(100, 200)),
            gfx::Size(100, 200));
}

TEST_F(ScreenWinHeadlessTest, ScreenToDIPSize2x) {
  auto screen = CreateHeadlessScreen("{devicePixelRatio=2.0}");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  HWND hwnd = screen->GetHWNDFromNativeWindow(
      screen->AddWindow(gfx::Rect(0, 0, 400, 300)));

  EXPECT_EQ(GetScreenWin()->ScreenToDIPSize(hwnd, gfx::Size(100, 200)),
            gfx::Size(50, 100));
}

TEST_F(ScreenWinHeadlessTest, DIPToScreenSize) {
  auto screen = CreateHeadlessScreen("{devicePixelRatio=1.0}");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  HWND hwnd = screen->GetHWNDFromNativeWindow(
      screen->AddWindow(gfx::Rect(0, 0, 400, 300)));

  EXPECT_EQ(GetScreenWin()->DIPToScreenSize(hwnd, gfx::Size(100, 200)),
            gfx::Size(100, 200));
}

TEST_F(ScreenWinHeadlessTest, DIPToScreenSize2x) {
  auto screen = CreateHeadlessScreen("{devicePixelRatio=2.0}");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  HWND hwnd = screen->GetHWNDFromNativeWindow(
      screen->AddWindow(gfx::Rect(0, 0, 400, 300)));

  EXPECT_EQ(GetScreenWin()->DIPToScreenSize(hwnd, gfx::Size(50, 100)),
            gfx::Size(100, 200));
}

TEST_F(ScreenWinHeadlessTest, GetPixelsPerInch) {
  auto screen = CreateHeadlessScreen("{devicePixelRatio=1.0}");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  EXPECT_EQ(GetScreenWin()->GetPixelsPerInch(gfx::PointF(400, 300)),
            gfx::Vector2dF(96, 96));
}

TEST_F(ScreenWinHeadlessTest, GetPixelsPerInch2x) {
  auto screen = CreateHeadlessScreen("{devicePixelRatio=2.0}");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  EXPECT_EQ(GetScreenWin()->GetPixelsPerInch(gfx::PointF(400, 300)),
            gfx::Vector2dF(192, 192));
}

TEST_F(ScreenWinHeadlessTest, GetScreenWinDisplayWithDisplayId) {
  auto screen = CreateHeadlessScreen("{label=#1}{label=#2}");
  ASSERT_EQ(screen->GetNumDisplays(), 2);

  const int64_t id1 = screen->GetAllDisplays()[0].id();
  EXPECT_EQ(
      GetScreenWin()->GetScreenWinDisplayWithDisplayId(id1).display().id(),
      id1);

  const int64_t id2 = screen->GetAllDisplays()[1].id();
  EXPECT_EQ(
      GetScreenWin()->GetScreenWinDisplayWithDisplayId(id2).display().id(),
      id2);

  // Unknown display id should result in primary display.
  EXPECT_EQ(GetScreenWin()->GetScreenWinDisplayWithDisplayId(-1).display().id(),
            id1);
}

TEST_F(ScreenWinHeadlessTest, DisplayIdFromMonitorInfo) {
  auto screen = CreateHeadlessScreen("{}{}");
  ASSERT_EQ(screen->GetNumDisplays(), 2);

  const int64_t id1 = screen->GetAllDisplays()[0].id();
  auto monitor_info1 = screen->GetMONITORINFOFromDisplayIdForTest(id1);
  ASSERT_TRUE(monitor_info1.has_value());
  EXPECT_EQ(GetScreenWin()->DisplayIdFromMonitorInfo(*monitor_info1), id1);

  const int64_t id2 = screen->GetAllDisplays()[1].id();
  auto monitor_info2 = screen->GetMONITORINFOFromDisplayIdForTest(id2);
  ASSERT_TRUE(monitor_info2.has_value());
  EXPECT_EQ(GetScreenWin()->DisplayIdFromMonitorInfo(*monitor_info2), id2);
}

TEST_F(ScreenWinHeadlessTest, GetScaleFactorForHWND) {
  auto screen = CreateHeadlessScreen("{devicePixelRatio=1.0}");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  HWND hwnd = screen->GetHWNDFromNativeWindow(
      screen->AddWindow(gfx::Rect(0, 0, 400, 300)));
  EXPECT_EQ(GetScreenWin()->GetScaleFactorForHWND(hwnd), 1.0);
}

TEST_F(ScreenWinHeadlessTest, GetScaleFactorForHWND2x) {
  auto screen = CreateHeadlessScreen("{devicePixelRatio=2.0}");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  HWND hwnd = screen->GetHWNDFromNativeWindow(
      screen->AddWindow(gfx::Rect(0, 0, 400, 300)));
  EXPECT_EQ(GetScreenWin()->GetScaleFactorForHWND(hwnd), 2.0);
}

// display::Screen interface methods tests ----------------------------

TEST_F(ScreenWinHeadlessTest, GetCursorScreenPoint) {
  auto screen = CreateHeadlessScreen("");

  EXPECT_EQ(screen->GetCursorScreenPoint(), gfx::Point());

  static constexpr gfx::Point kCursorScreenPoint(123, 456);
  screen->SetCursorScreenPointForTesting(kCursorScreenPoint);
  EXPECT_EQ(screen->GetCursorScreenPoint(), kCursorScreenPoint);
}

TEST_F(ScreenWinHeadlessTest, IsWindowUnderCursor) {
  auto screen = CreateHeadlessScreen("");

  gfx::NativeWindow window1 = screen->AddWindow(gfx::Rect(0, 0, 400, 300));
  EXPECT_TRUE(screen->IsWindowUnderCursor(window1));

  gfx::NativeWindow window2 = screen->AddWindow(gfx::Rect(800, 0, 400, 300));
  EXPECT_FALSE(screen->IsWindowUnderCursor(window2));
}

TEST_F(ScreenWinHeadlessTest, GetWindowAtScreenPoint) {
  auto screen = CreateHeadlessScreen("");

  EXPECT_EQ(screen->GetWindowAtScreenPoint(gfx::Point(0, 0)), nullptr);

  gfx::NativeWindow window1 = screen->AddWindow(gfx::Rect(0, 0, 400, 300));
  gfx::NativeWindow window2 = screen->AddWindow(gfx::Rect(10, 10, 400, 300));

  EXPECT_EQ(screen->GetWindowAtScreenPoint(gfx::Point(0, 0)), window1);
  EXPECT_EQ(screen->GetWindowAtScreenPoint(gfx::Point(10, 10)), window2);
}

TEST_F(ScreenWinHeadlessTest, GetLocalProcessWindowAtPoint) {
  auto screen = CreateHeadlessScreen("");

  gfx::NativeWindow window1 = screen->AddWindow(gfx::Rect(0, 0, 400, 300));
  gfx::NativeWindow window2 = screen->AddWindow(gfx::Rect(10, 10, 400, 300));

  EXPECT_EQ(screen->GetLocalProcessWindowAtPoint(gfx::Point(0, 0),
                                                 std::set<gfx::NativeWindow>()),
            window1);

  EXPECT_EQ(screen->GetLocalProcessWindowAtPoint(gfx::Point(10, 10),
                                                 std::set<gfx::NativeWindow>()),
            window2);

  EXPECT_EQ(screen->GetLocalProcessWindowAtPoint(
                gfx::Point(10, 10), std::set<gfx::NativeWindow>({window2})),
            window1);

  EXPECT_EQ(
      screen->GetLocalProcessWindowAtPoint(
          gfx::Point(10, 10), std::set<gfx::NativeWindow>({window1, window2})),
      nullptr);
}

TEST_F(ScreenWinHeadlessTest, GetNumDisplays) {
  auto screen = CreateHeadlessScreen("{label='#1'}{label='#2'}");
  EXPECT_EQ(screen->GetNumDisplays(), 2);
}

TEST_F(ScreenWinHeadlessTest, GetAllDisplays) {
  auto screen = CreateHeadlessScreen("{label='#1'}{label='#2'}");

  const std::vector<Display>& displays = screen->GetAllDisplays();
  ASSERT_THAT(displays, testing::SizeIs(2));

  EXPECT_THAT(displays[0].label(), testing::StrEq("#1"));
  EXPECT_THAT(displays[1].label(), testing::StrEq("#2"));
}

TEST_F(ScreenWinHeadlessTest, GetDisplayNearestWindow) {
  auto screen = CreateHeadlessScreen("{label='#1'}{label='#2'}");
  ASSERT_EQ(screen->GetNumDisplays(), 2);

  gfx::NativeWindow window1 = screen->AddWindow(gfx::Rect(0, 0, 400, 300));
  Display display1 = screen->GetDisplayNearestWindow(window1);
  EXPECT_THAT(display1.label(), testing::StrEq("#1"));

  gfx::NativeWindow window2 = screen->AddWindow(gfx::Rect(800, 0, 400, 300));
  Display display2 = screen->GetDisplayNearestWindow(window2);
  EXPECT_THAT(display2.label(), testing::StrEq("#2"));
}

TEST_F(ScreenWinHeadlessTest, GetDisplayNearestPoint) {
  auto screen = CreateHeadlessScreen(
      "{label='#1'}{label='#2'}{0,600 label='#3'}{label='#4'}");
  ASSERT_EQ(screen->GetNumDisplays(), 4);

  EXPECT_THAT(screen->GetDisplayNearestPoint(gfx::Point(1, 1)).label(),
              testing::StrEq("#1"));
  EXPECT_THAT(screen->GetDisplayNearestPoint(gfx::Point(801, 1)).label(),
              testing::StrEq("#2"));
  EXPECT_THAT(screen->GetDisplayNearestPoint(gfx::Point(1, 601)).label(),
              testing::StrEq("#3"));
  EXPECT_THAT(screen->GetDisplayNearestPoint(gfx::Point(801, 601)).label(),
              testing::StrEq("#4"));
}

TEST_F(ScreenWinHeadlessTest, GetDisplayMatching) {
  auto screen = CreateHeadlessScreen(
      "{label='#1'}{label='#2'}{0,600 label='#3'}{label='#4'}");
  ASSERT_EQ(screen->GetNumDisplays(), 4);

  EXPECT_THAT(screen->GetDisplayMatching(gfx::Rect(1, 1, 400, 300)).label(),
              testing::StrEq("#1"));
  EXPECT_THAT(screen->GetDisplayMatching(gfx::Rect(801, 1, 400, 300)).label(),
              testing::StrEq("#2"));
  EXPECT_THAT(screen->GetDisplayMatching(gfx::Rect(1, 601, 400, 300)).label(),
              testing::StrEq("#3"));
  EXPECT_THAT(screen->GetDisplayMatching(gfx::Rect(801, 601, 400, 300)).label(),
              testing::StrEq("#4"));
}

TEST_F(ScreenWinHeadlessTest, GetPrimaryDisplay) {
  auto screen =
      CreateHeadlessScreen("{label='#1'}{label='#2'}{label='#3'}{label='#4'}");
  ASSERT_EQ(screen->GetNumDisplays(), 4);

  Display display = screen->GetPrimaryDisplay();
  EXPECT_THAT(display.label(), testing::StrEq("#1"));
}

TEST_F(ScreenWinHeadlessTest, ScreenToDIPRectInWindow) {
  auto screen = CreateHeadlessScreen("{devicePixelRatio=1.0}");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  gfx::NativeWindow window = screen->AddWindow(gfx::Rect(10, 20, 400, 300));
  EXPECT_EQ(
      screen->ScreenToDIPRectInWindow(window, gfx::Rect(10, 20, 100, 200)),
      gfx::Rect(10, 20, 100, 200));
}

TEST_F(ScreenWinHeadlessTest, ScreenToDIPRectInWindow2x) {
  auto screen = CreateHeadlessScreen("{devicePixelRatio=2.0}");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  gfx::NativeWindow window = screen->AddWindow(gfx::Rect(10, 20, 400, 300));
  EXPECT_EQ(
      screen->ScreenToDIPRectInWindow(window, gfx::Rect(10, 20, 100, 200)),
      gfx::Rect(5, 10, 50, 100));
}

TEST_F(ScreenWinHeadlessTest, DIPToScreenRectInWindow) {
  auto screen = CreateHeadlessScreen("{devicePixelRatio=1.0}");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  gfx::NativeWindow window = screen->AddWindow(gfx::Rect(10, 20, 400, 300));
  EXPECT_EQ(
      screen->DIPToScreenRectInWindow(window, gfx::Rect(10, 20, 100, 200)),
      gfx::Rect(10, 20, 100, 200));
}

TEST_F(ScreenWinHeadlessTest, DIPToScreenRectInWindow2x) {
  auto screen = CreateHeadlessScreen("{devicePixelRatio=2.0}");
  ASSERT_EQ(screen->GetNumDisplays(), 1);

  gfx::NativeWindow window = screen->AddWindow(gfx::Rect(10, 20, 400, 300));
  EXPECT_EQ(screen->DIPToScreenRectInWindow(window, gfx::Rect(5, 10, 50, 100)),
            gfx::Rect(10, 20, 100, 200));
}

}  // namespace

}  // namespace display::win
