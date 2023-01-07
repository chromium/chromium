// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/mac/test/virtual_display_mac_util.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/size.h"

class VirtualDisplayMacUtilInteractiveUitest : public testing::Test {
 protected:
  VirtualDisplayMacUtilInteractiveUitest() = default;

  VirtualDisplayMacUtilInteractiveUitest(
      const VirtualDisplayMacUtilInteractiveUitest&) = delete;
  VirtualDisplayMacUtilInteractiveUitest& operator=(
      const VirtualDisplayMacUtilInteractiveUitest&) = delete;

  void SetUp() override {
    if (!display::test::VirtualDisplayMacUtil::IsAPIAvailable()) {
      GTEST_SKIP() << "Skipping test for unsupported MacOS version.";
    }

    testing::Test::SetUp();
  }

 private:
  display::ScopedNativeScreen screen_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
};

TEST_F(VirtualDisplayMacUtilInteractiveUitest, AddDisplay) {
  display::test::VirtualDisplayMacUtil virtual_display_mac_util;

  int64_t id = virtual_display_mac_util.AddDisplay(
      1, display::test::VirtualDisplayMacUtil::k1920x1080);
  EXPECT_NE(id, display::kInvalidDisplayId);

  display::Display d;
  bool found = display::Screen::GetScreen()->GetDisplayWithDisplayId(id, &d);
  EXPECT_TRUE(found);
}

TEST_F(VirtualDisplayMacUtilInteractiveUitest, RemoveDisplay) {
  display::test::VirtualDisplayMacUtil virtual_display_mac_util;

  int64_t id = virtual_display_mac_util.AddDisplay(
      1, display::test::VirtualDisplayMacUtil::k1920x1080);
  int display_count = display::Screen::GetScreen()->GetNumDisplays();
  EXPECT_GT(display_count, 1);

  virtual_display_mac_util.RemoveDisplay(id);
  EXPECT_EQ(display::Screen::GetScreen()->GetNumDisplays(), display_count - 1);

  display::Display d;
  bool found = display::Screen::GetScreen()->GetDisplayWithDisplayId(id, &d);
  EXPECT_FALSE(found);
}

TEST_F(VirtualDisplayMacUtilInteractiveUitest, IsAPIAvailable) {
  EXPECT_TRUE(display::test::VirtualDisplayMacUtil::IsAPIAvailable());
}

TEST_F(VirtualDisplayMacUtilInteractiveUitest, HotPlug) {
  int display_count = display::Screen::GetScreen()->GetNumDisplays();

  std::unique_ptr<display::test::VirtualDisplayMacUtil>
      virtual_display_mac_util =
          std::make_unique<display::test::VirtualDisplayMacUtil>();

  virtual_display_mac_util->AddDisplay(
      1, display::test::VirtualDisplayMacUtil::k1920x1080);
  EXPECT_EQ(display::Screen::GetScreen()->GetNumDisplays(), display_count + 1);

  virtual_display_mac_util->AddDisplay(
      2, display::test::VirtualDisplayMacUtil::k1920x1080);
  EXPECT_EQ(display::Screen::GetScreen()->GetNumDisplays(), display_count + 2);

  virtual_display_mac_util.reset();
  EXPECT_EQ(display::Screen::GetScreen()->GetNumDisplays(), display_count);
}

TEST_F(VirtualDisplayMacUtilInteractiveUitest, EnsureDisplayWithResolution) {
  display::test::VirtualDisplayMacUtil virtual_display_mac_util;

  int64_t id = virtual_display_mac_util.AddDisplay(
      1, display::test::VirtualDisplayMacUtil::k1920x1080);

  display::Display d;
  display::Screen::GetScreen()->GetDisplayWithDisplayId(id, &d);
  EXPECT_EQ(d.size(), gfx::Size(1920, 1080));
}
