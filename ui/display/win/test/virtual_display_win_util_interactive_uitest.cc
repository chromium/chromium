// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/win/screen_win.h"
#include "ui/display/win/test/virtual_display_win_util.h"

// Flag passed to use the custom display driver to make virtual displays.
static constexpr char kSwitchWindowsVirtualDisplayDriver[] =
    "windows-virtual-display-driver";

class VirtualDisplayWinUtilInteractiveUitest : public testing::Test {
 public:
  VirtualDisplayWinUtilInteractiveUitest(
      const VirtualDisplayWinUtilInteractiveUitest&) = delete;
  VirtualDisplayWinUtilInteractiveUitest& operator=(
      const VirtualDisplayWinUtilInteractiveUitest&) = delete;

 protected:
  VirtualDisplayWinUtilInteractiveUitest()
      : virtual_display_win_util_(&screen_) {}

  void SetUp() override {
    if (!virtual_display_win_util_.IsAPIAvailable()) {
      if (base::CommandLine::ForCurrentProcess()->HasSwitch(
              kSwitchWindowsVirtualDisplayDriver)) {
        GTEST_FAIL() << "Driver not detected to be installed, but "
                     << kSwitchWindowsVirtualDisplayDriver
                     << " switch was passed. This indicates a possible "
                        "misconfiguration.";
      }
      GTEST_SKIP()
          << "Host does not support virtual displays (driver is not detected).";
    }
    display::Screen::SetScreenInstance(&screen_);
    testing::Test::SetUp();
  }
  display::Screen* screen() { return &screen_; }
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  display::win::ScreenWin screen_;
  display::test::VirtualDisplayWinUtil virtual_display_win_util_;
};

TEST_F(VirtualDisplayWinUtilInteractiveUitest, IsAPIAvailable) {
  EXPECT_TRUE(virtual_display_win_util_.IsAPIAvailable());
}

TEST_F(VirtualDisplayWinUtilInteractiveUitest, AddDisplay) {
  int64_t display_id = virtual_display_win_util_.AddDisplay(
      1, display::test::VirtualDisplayWinUtil::k1920x1080);
  EXPECT_NE(display_id, display::kInvalidDisplayId);
  EXPECT_GT(screen()->GetNumDisplays(), 1);
  display::Display d;
  EXPECT_TRUE(screen()->GetDisplayWithDisplayId(display_id, &d));
  EXPECT_EQ(d.size(), gfx::Size(1920, 1080));

  // Expect failure when adding a duplicate index.
  EXPECT_EQ(virtual_display_win_util_.AddDisplay(
                1, display::test::VirtualDisplayWinUtil::k1920x1080),
            display::kInvalidDisplayId);

  virtual_display_win_util_.RemoveAllDisplays();
  EXPECT_FALSE(screen()->GetDisplayWithDisplayId(display_id, &d));
}

TEST_F(VirtualDisplayWinUtilInteractiveUitest, AddRemove) {
  int64_t display_id[3];
  int initial_display_count = screen()->GetNumDisplays();
  display_id[0] = virtual_display_win_util_.AddDisplay(
      0, display::test::VirtualDisplayWinUtil::k1920x1080);
  EXPECT_NE(display_id[0], display::kInvalidDisplayId);
  display::Display d;
  EXPECT_TRUE(screen()->GetDisplayWithDisplayId(display_id[0], &d));

  display_id[1] = virtual_display_win_util_.AddDisplay(
      1, display::test::VirtualDisplayWinUtil::k1024x768);
  EXPECT_NE(display_id[1], display::kInvalidDisplayId);
  EXPECT_TRUE(screen()->GetDisplayWithDisplayId(display_id[1], &d));

  display_id[2] = virtual_display_win_util_.AddDisplay(
      2, display::test::VirtualDisplayWinUtil::k1920x1080);
  EXPECT_NE(display_id[2], display::kInvalidDisplayId);
  EXPECT_TRUE(screen()->GetDisplayWithDisplayId(display_id[2], &d));

  EXPECT_EQ(screen()->GetNumDisplays(), initial_display_count+3);
  virtual_display_win_util_.RemoveDisplay(display_id[1]);
  EXPECT_EQ(screen()->GetNumDisplays(), initial_display_count+2);
  // Only virtual display 2 should no longer exist.
  EXPECT_TRUE(screen()->GetDisplayWithDisplayId(display_id[0], &d));
  EXPECT_EQ(d.size(), gfx::Size(1920, 1080));
  EXPECT_FALSE(screen()->GetDisplayWithDisplayId(display_id[1], &d));
  EXPECT_TRUE(screen()->GetDisplayWithDisplayId(display_id[2], &d));
  EXPECT_EQ(d.size(), gfx::Size(1920, 1080));

  virtual_display_win_util_.RemoveAllDisplays();
  EXPECT_FALSE(screen()->GetDisplayWithDisplayId(display_id[0], &d));
  EXPECT_FALSE(screen()->GetDisplayWithDisplayId(display_id[1], &d));
  EXPECT_FALSE(screen()->GetDisplayWithDisplayId(display_id[2], &d));
  EXPECT_EQ(screen()->GetNumDisplays(), initial_display_count);

}
