// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/screen_ozone.h"
#include "ui/display/linux/test/virtual_display_util_linux.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"

// This test suite requires the host to have special Xorg/Xrandr configuration.
// See: //docs/ui/display/multiscreen_testing.md
class VirtualDisplayUtilLinuxInteractiveUitest : public testing::Test {
 public:
  VirtualDisplayUtilLinuxInteractiveUitest(
      const VirtualDisplayUtilLinuxInteractiveUitest&) = delete;
  VirtualDisplayUtilLinuxInteractiveUitest& operator=(
      const VirtualDisplayUtilLinuxInteractiveUitest&) = delete;

 protected:
  VirtualDisplayUtilLinuxInteractiveUitest() = default;
  ~VirtualDisplayUtilLinuxInteractiveUitest() override = default;
  void SetUp() override {
    if (!display::test::VirtualDisplayUtilLinux::IsAPIAvailable()) {
      GTEST_SKIP() << "Host does not support virtual displays.";
    }
    CHECK(!display::Screen::HasScreen());
    screen_ = std::make_unique<aura::ScreenOzone>();
    virtual_display_util_ =
        std::make_unique<display::test::VirtualDisplayUtilLinux>(screen());
    testing::Test::SetUp();
  }

  display::Screen* screen() { return screen_.get(); }
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  std::unique_ptr<aura::ScreenOzone> screen_;
  std::unique_ptr<display::test::VirtualDisplayUtilLinux> virtual_display_util_;
};

TEST_F(VirtualDisplayUtilLinuxInteractiveUitest, IsAPIAvailable) {
  EXPECT_TRUE(virtual_display_util_->IsAPIAvailable());
}

TEST_F(VirtualDisplayUtilLinuxInteractiveUitest, AddRemove) {
  int64_t display_id[3];
  int initial_display_count = screen()->GetNumDisplays();
  display_id[0] = virtual_display_util_->AddDisplay(
      display::test::VirtualDisplayUtilLinux::k1920x1080);
  EXPECT_NE(display_id[0], display::kInvalidDisplayId);
  display::Display d;
  EXPECT_EQ(screen()->GetNumDisplays(), initial_display_count + 1);
  EXPECT_TRUE(screen()->GetDisplayWithDisplayId(display_id[0], &d));
  EXPECT_EQ(d.size(), gfx::Size(1920, 1080));

  display_id[1] = virtual_display_util_->AddDisplay(
      display::test::VirtualDisplayUtilLinux::k1024x768);
  EXPECT_NE(display_id[1], display::kInvalidDisplayId);
  EXPECT_EQ(screen()->GetNumDisplays(), initial_display_count + 2);
  EXPECT_TRUE(screen()->GetDisplayWithDisplayId(display_id[1], &d));
  EXPECT_EQ(d.size(), gfx::Size(1024, 768));

  display_id[2] = virtual_display_util_->AddDisplay(
      display::test::VirtualDisplayUtilLinux::k1920x1080);
  EXPECT_NE(display_id[2], display::kInvalidDisplayId);
  EXPECT_EQ(screen()->GetNumDisplays(), initial_display_count + 3);
  EXPECT_TRUE(screen()->GetDisplayWithDisplayId(display_id[2], &d));
  EXPECT_EQ(d.size(), gfx::Size(1920, 1080));

  virtual_display_util_->RemoveDisplay(display_id[1]);
  EXPECT_EQ(screen()->GetNumDisplays(), initial_display_count + 2);
  // Only virtual display 2 should no longer exist.
  EXPECT_TRUE(screen()->GetDisplayWithDisplayId(display_id[0], &d));
  EXPECT_EQ(d.size(), gfx::Size(1920, 1080));
  EXPECT_FALSE(screen()->GetDisplayWithDisplayId(display_id[1], &d));
  EXPECT_TRUE(screen()->GetDisplayWithDisplayId(display_id[2], &d));
  EXPECT_EQ(d.size(), gfx::Size(1920, 1080));

  virtual_display_util_->ResetDisplays();
  EXPECT_FALSE(screen()->GetDisplayWithDisplayId(display_id[0], &d));
  EXPECT_FALSE(screen()->GetDisplayWithDisplayId(display_id[1], &d));
  EXPECT_FALSE(screen()->GetDisplayWithDisplayId(display_id[2], &d));
  EXPECT_EQ(screen()->GetNumDisplays(), initial_display_count);
}
