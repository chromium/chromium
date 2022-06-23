// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/managed_display_info.h"

#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/display_color_spaces.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/display/manager/touch_device_manager.h"
#endif

namespace display {

using DisplayInfoTest = testing::Test;

TEST_F(DisplayInfoTest, CreateFromSpec) {
  ManagedDisplayInfo info =
      ManagedDisplayInfo::CreateFromSpecWithID("200x100", 10);
  EXPECT_EQ(10, info.id());
  EXPECT_EQ(gfx::Rect(0, 0, 200, 100), info.bounds_in_native());
  EXPECT_EQ(gfx::Size(200, 100), info.size_in_pixel());
  EXPECT_EQ(Display::ROTATE_0, info.GetActiveRotation());
  EXPECT_EQ(gfx::DisplayColorSpaces(), info.display_color_spaces());
  EXPECT_EQ(gfx::Insets(), info.overscan_insets_in_dip());

  info = ManagedDisplayInfo::CreateFromSpecWithID("10+20-300x400*2/o", 10);
  EXPECT_EQ(gfx::Rect(10, 20, 300, 400), info.bounds_in_native());
  EXPECT_EQ(gfx::Size(288, 380), info.size_in_pixel());
  EXPECT_EQ(Display::ROTATE_0, info.GetActiveRotation());
  EXPECT_EQ(gfx::DisplayColorSpaces(), info.display_color_spaces());
  EXPECT_EQ(gfx::Insets::TLBR(5, 3, 5, 3), info.overscan_insets_in_dip());

  info = ManagedDisplayInfo::CreateFromSpecWithID("10+20-300x400*2/oh", 10);
  EXPECT_EQ(gfx::Rect(10, 20, 300, 400), info.bounds_in_native());
  EXPECT_EQ(gfx::Size(288, 380), info.size_in_pixel());
  EXPECT_EQ(Display::ROTATE_0, info.GetActiveRotation());
  EXPECT_EQ(gfx::DisplayColorSpaces(gfx::ColorSpace::CreateHDR10(),
                                    gfx::BufferFormat::BGRA_1010102),
            info.display_color_spaces());
  EXPECT_EQ(gfx::Insets::TLBR(5, 3, 5, 3), info.overscan_insets_in_dip());

  info = ManagedDisplayInfo::CreateFromSpecWithID("10+20-300x400*2/ob", 10);
  EXPECT_EQ(gfx::Rect(10, 20, 300, 400), info.bounds_in_native());
  EXPECT_EQ(gfx::Size(288, 380), info.size_in_pixel());
  EXPECT_EQ(Display::ROTATE_0, info.GetActiveRotation());
  EXPECT_EQ(gfx::DisplayColorSpaces(), info.display_color_spaces());
  EXPECT_EQ(gfx::Insets::TLBR(5, 3, 5, 3), info.overscan_insets_in_dip());

  info = ManagedDisplayInfo::CreateFromSpecWithID("10+20-300x400*2/or", 10);
  EXPECT_EQ(gfx::Rect(10, 20, 300, 400), info.bounds_in_native());
  EXPECT_EQ(gfx::Size(380, 288), info.size_in_pixel());
  EXPECT_EQ(Display::ROTATE_90, info.GetActiveRotation());
  EXPECT_EQ(gfx::DisplayColorSpaces(), info.display_color_spaces());
  // TODO(oshima): This should be rotated too. Fix this.
  EXPECT_EQ(gfx::Insets::TLBR(5, 3, 5, 3), info.overscan_insets_in_dip());

  info = ManagedDisplayInfo::CreateFromSpecWithID("10+20-300x400*2/or@1.5", 10);
  EXPECT_EQ(gfx::Rect(10, 20, 300, 400), info.bounds_in_native());
  EXPECT_EQ(gfx::Size(380, 288), info.size_in_pixel());
  EXPECT_EQ(Display::ROTATE_90, info.GetActiveRotation());
  EXPECT_EQ(gfx::DisplayColorSpaces(), info.display_color_spaces());
  EXPECT_EQ(gfx::Insets::TLBR(5, 3, 5, 3), info.overscan_insets_in_dip());
  EXPECT_EQ(gfx::Insets::TLBR(10, 6, 10, 6), info.GetOverscanInsetsInPixel());

  info = ManagedDisplayInfo::CreateFromSpecWithID("10+20-300x400*2/l@1.5", 10);
  EXPECT_EQ(gfx::Rect(10, 20, 300, 400), info.bounds_in_native());
  EXPECT_EQ(Display::ROTATE_270, info.GetActiveRotation());
  EXPECT_EQ(1.5f, info.zoom_factor());
  EXPECT_EQ(gfx::DisplayColorSpaces(), info.display_color_spaces());

  info = ManagedDisplayInfo::CreateFromSpecWithID(
      "250x200#300x200|250x200%59.9|150x100%60|150x100*2|200x150*1.25%30", 10);

  EXPECT_EQ(gfx::Rect(0, 0, 250, 200), info.bounds_in_native());
  EXPECT_EQ(5u, info.display_modes().size());
  // Modes are sorted in DIP for external display.
  EXPECT_EQ(gfx::Size(150, 100), info.display_modes()[0].size());
  EXPECT_EQ(gfx::Size(150, 100), info.display_modes()[1].size());
  EXPECT_EQ(gfx::Size(200, 150), info.display_modes()[2].size());
  EXPECT_EQ(gfx::Size(250, 200), info.display_modes()[3].size());
  EXPECT_EQ(gfx::Size(300, 200), info.display_modes()[4].size());

  EXPECT_EQ(60.0f, info.display_modes()[0].refresh_rate());
  EXPECT_EQ(60.0f, info.display_modes()[1].refresh_rate());
  EXPECT_EQ(30.0f, info.display_modes()[2].refresh_rate());
  EXPECT_EQ(59.9f, info.display_modes()[3].refresh_rate());
  EXPECT_EQ(60.0f, info.display_modes()[4].refresh_rate());

  EXPECT_EQ(2.0f, info.display_modes()[0].device_scale_factor());
  EXPECT_EQ(1.0f, info.display_modes()[1].device_scale_factor());
  EXPECT_EQ(1.25f, info.display_modes()[2].device_scale_factor());
  EXPECT_EQ(1.0f, info.display_modes()[3].device_scale_factor());
  EXPECT_EQ(1.0f, info.display_modes()[4].device_scale_factor());

  EXPECT_FALSE(info.display_modes()[0].native());
  EXPECT_FALSE(info.display_modes()[1].native());
  EXPECT_FALSE(info.display_modes()[2].native());
  EXPECT_FALSE(info.display_modes()[3].native());
  EXPECT_TRUE(info.display_modes()[4].native());

  info = ManagedDisplayInfo::CreateFromSpecWithID(
      "250x200*2#300x200|250x200*1.25|150x100", 10);

  EXPECT_EQ(gfx::Size(150, 100), info.display_modes()[0].size());
  EXPECT_EQ(gfx::Size(300, 200), info.display_modes()[1].size());
  EXPECT_EQ(gfx::Size(250, 200), info.display_modes()[2].size());
  EXPECT_EQ(2.0f, info.display_modes()[0].device_scale_factor());
  EXPECT_EQ(2.0f, info.display_modes()[1].device_scale_factor());
  EXPECT_EQ(1.25f, info.display_modes()[2].device_scale_factor());
}

}  // namespace display
