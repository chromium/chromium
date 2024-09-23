// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/mac/test/virtual_display_util_mac.h"
#include "ui/display/screen.h"
#include "ui/display/test/virtual_display_util.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/size.h"

class VirtualDisplayUtilMacInteractiveUitest : public testing::Test {
 public:
  VirtualDisplayUtilMacInteractiveUitest(
      const VirtualDisplayUtilMacInteractiveUitest&) = delete;
  VirtualDisplayUtilMacInteractiveUitest& operator=(
      const VirtualDisplayUtilMacInteractiveUitest&) = delete;

 protected:
  VirtualDisplayUtilMacInteractiveUitest() = default;

  void SetUp() override {
    virtual_display_util_mac_ =
        std::make_unique<display::test::VirtualDisplayUtilMac>(
            display::Screen::GetScreen());
    if (!virtual_display_util_mac_->IsAPIAvailable()) {
      GTEST_SKIP() << "Skipping test for unsupported MacOS version.";
    }

    testing::Test::SetUp();
  }

 private:
  display::ScopedNativeScreen screen_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};

 protected:
  std::unique_ptr<display::test::VirtualDisplayUtilMac>
      virtual_display_util_mac_;
};

// Tests that VirtualDisplayUtil::TryCreate returns a valid instance for this
// platform.
TEST_F(VirtualDisplayUtilMacInteractiveUitest, TryCreate) {
  EXPECT_NE(display::test::VirtualDisplayUtil::TryCreate(
                display::Screen::GetScreen()),
            nullptr);
}

TEST_F(VirtualDisplayUtilMacInteractiveUitest, AddDisplay) {
  int64_t id = virtual_display_util_mac_->AddDisplay(
      display::test::VirtualDisplayUtilMac::k1920x1080);
  EXPECT_NE(id, display::kInvalidDisplayId);

  display::Display d;
  bool found = display::Screen::GetScreen()->GetDisplayWithDisplayId(id, &d);
  EXPECT_TRUE(found);
}

TEST_F(VirtualDisplayUtilMacInteractiveUitest, RemoveDisplay) {
  int64_t id = virtual_display_util_mac_->AddDisplay(
      display::test::VirtualDisplayUtilMac::k1920x1080);
  int display_count = display::Screen::GetScreen()->GetNumDisplays();
  EXPECT_GT(display_count, 1);

  virtual_display_util_mac_->RemoveDisplay(id);
  EXPECT_EQ(display::Screen::GetScreen()->GetNumDisplays(), display_count - 1);

  display::Display d;
  bool found = display::Screen::GetScreen()->GetDisplayWithDisplayId(id, &d);
  EXPECT_FALSE(found);
}

TEST_F(VirtualDisplayUtilMacInteractiveUitest, IsAPIAvailable) {
  EXPECT_TRUE(virtual_display_util_mac_->IsAPIAvailable());
}

TEST_F(VirtualDisplayUtilMacInteractiveUitest, HotPlug) {
  int display_count = display::Screen::GetScreen()->GetNumDisplays();

  virtual_display_util_mac_->AddDisplay(
      display::test::VirtualDisplayUtilMac::k1920x1080);
  EXPECT_EQ(display::Screen::GetScreen()->GetNumDisplays(), display_count + 1);

  virtual_display_util_mac_->AddDisplay(
      display::test::VirtualDisplayUtilMac::k1920x1080);
  EXPECT_EQ(display::Screen::GetScreen()->GetNumDisplays(), display_count + 2);

  virtual_display_util_mac_->ResetDisplays();
  EXPECT_EQ(display::Screen::GetScreen()->GetNumDisplays(), display_count);
}

TEST_F(VirtualDisplayUtilMacInteractiveUitest, EnsureDisplayWithResolutionHD) {
  int64_t id = virtual_display_util_mac_->AddDisplay(
      display::test::VirtualDisplayUtil::k1920x1080);

  display::Display d;
  display::Screen::GetScreen()->GetDisplayWithDisplayId(id, &d);
  EXPECT_EQ(d.size(), gfx::Size(1920, 1080));
}

TEST_F(VirtualDisplayUtilMacInteractiveUitest, EnsureDisplayWithResolutionXGA) {
  int64_t id = virtual_display_util_mac_->AddDisplay(
      display::test::VirtualDisplayUtil::k1024x768);

  display::Display d;
  display::Screen::GetScreen()->GetDisplayWithDisplayId(id, &d);
  EXPECT_EQ(d.size(), gfx::Size(1024, 768));
}
