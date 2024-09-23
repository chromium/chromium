// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/managed_display_info.h"

#include "base/test/gtest_util.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_change_observer.h"
#include "ui/display/manager/test/fake_display_snapshot.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

#include "ui/display/manager/touch_device_manager.h"

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
  EXPECT_EQ(gfx::RoundedCornersF(0.0), info.panel_corners_radii());

  info = ManagedDisplayInfo::CreateFromSpecWithID("10+20-300x400*2/o", 10);
  EXPECT_EQ(gfx::Rect(10, 20, 300, 400), info.bounds_in_native());
  EXPECT_EQ(gfx::Size(288, 380), info.size_in_pixel());
  EXPECT_EQ(Display::ROTATE_0, info.GetActiveRotation());
  EXPECT_EQ(gfx::DisplayColorSpaces(), info.display_color_spaces());
  EXPECT_EQ(gfx::Insets::TLBR(5, 3, 5, 3), info.overscan_insets_in_dip());
  EXPECT_EQ(gfx::RoundedCornersF(0.0), info.panel_corners_radii());

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

  info =
      ManagedDisplayInfo::CreateFromSpecWithID("10+20-300x400*2/or@1.5~16", 10);
  EXPECT_EQ(gfx::Rect(10, 20, 300, 400), info.bounds_in_native());
  EXPECT_EQ(gfx::Size(380, 288), info.size_in_pixel());
  EXPECT_EQ(Display::ROTATE_90, info.GetActiveRotation());
  EXPECT_EQ(gfx::DisplayColorSpaces(), info.display_color_spaces());
  EXPECT_EQ(gfx::Insets::TLBR(5, 3, 5, 3), info.overscan_insets_in_dip());
  EXPECT_EQ(gfx::Insets::TLBR(10, 6, 10, 6), info.GetOverscanInsetsInPixel());
  EXPECT_EQ(gfx::RoundedCornersF(16.0), info.panel_corners_radii());

  info = ManagedDisplayInfo::CreateFromSpecWithID(
      "10+20-300x400*2/l@1.5~16|16|10|10", 10);
  EXPECT_EQ(gfx::Rect(10, 20, 300, 400), info.bounds_in_native());
  EXPECT_EQ(Display::ROTATE_270, info.GetActiveRotation());
  EXPECT_EQ(1.5f, info.zoom_factor());
  EXPECT_EQ(gfx::DisplayColorSpaces(), info.display_color_spaces());
  EXPECT_EQ(gfx::RoundedCornersF(16.0, 16.0, 10.0, 10.0),
            info.panel_corners_radii());

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

  info = ManagedDisplayInfo::CreateFromSpecWithID(
      "250x200*2#300x200|250x200*1.25|150x100~16|16|10|10", 10);
  EXPECT_EQ(gfx::Size(150, 100), info.display_modes()[0].size());
  EXPECT_EQ(gfx::Size(300, 200), info.display_modes()[1].size());
  EXPECT_EQ(gfx::Size(250, 200), info.display_modes()[2].size());
  EXPECT_EQ(2.0f, info.display_modes()[0].device_scale_factor());
  EXPECT_EQ(2.0f, info.display_modes()[1].device_scale_factor());
  EXPECT_EQ(1.25f, info.display_modes()[2].device_scale_factor());
  EXPECT_EQ(gfx::RoundedCornersF(16.0, 16.0, 10.0, 10.0),
            info.panel_corners_radii());
}

TEST_F(DisplayInfoTest, ExpectDeathWhenInvalidNumberOfRadiiProvided) {
  EXPECT_DCHECK_DEATH(
      ManagedDisplayInfo::CreateFromSpecWithID("200x100~10|15", 10));

  EXPECT_DCHECK_DEATH(
      ManagedDisplayInfo::CreateFromSpecWithID("200x100~10|10|15", 10));
}

TEST_F(DisplayInfoTest, ExpectDeathWhenInvalidDisplayRadiusProvided) {
  EXPECT_DCHECK_DEATH(
      ManagedDisplayInfo::CreateFromSpecWithID("200x100~1f", 10));

  EXPECT_DCHECK_DEATH(
      ManagedDisplayInfo::CreateFromSpecWithID("200x100~10.5", 10));
}

TEST_F(DisplayInfoTest, TestToStringFormat) {
  ManagedDisplayInfo info =
      ManagedDisplayInfo::CreateFromSpecWithID("200x100", 10);

  EXPECT_EQ(info.ToString(),
            "ManagedDisplayInfo[10] port_display_id=10, edid_display_id=20, "
            "native bounds=0,0 200x100, size=200x100, "
            "device-scale=1, display-zoom=1, overscan=x:0,0 y:0,0, rotation=0, "
            "touchscreen=unknown, "
            "panel_corners_radii=0.000000,0.000000,0.000000,0.000000, "
            "panel_orientation=Normal, detected=true, "
            "color_space="
            "{primaries:BT709, transfer:SRGB, matrix:RGB, range:FULL}");

  EXPECT_EQ(info.ToFullString(),
            "ManagedDisplayInfo[10] port_display_id=10, edid_display_id=20, "
            "native bounds=0,0 200x100, size=200x100, "
            "device-scale=1, display-zoom=1, overscan=x:0,0 y:0,0, rotation=0, "
            "touchscreen=unknown, "
            "panel_corners_radii=0.000000,0.000000,0.000000,0.000000, "
            "panel_orientation=Normal, detected=true, "
            "color_space="
            "{primaries:BT709, transfer:SRGB, matrix:RGB, range:FULL}, "
            "display_modes==(200x100@60P(N) 1)");
}

}  // namespace display
