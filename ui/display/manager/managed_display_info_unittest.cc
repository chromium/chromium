// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/managed_display_info.h"

#include "base/test/gtest_util.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_change_observer.h"
#include "ui/display/manager/test/fake_display_snapshot.h"
#include "ui/display/manager/touch_device_manager.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

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
                                    viz::SinglePlaneFormat::kBGRA_1010102),
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
            "ManagedDisplayInfo[10] name=\"Display-10\", port_display_id=10, "
            "edid_display_id=20, "
            "native bounds=0,0 200x100, size=200x100, refresh-rate=60.000000, "
            "device-scale=1, display-zoom=1, overscan=x:0,0 y:0,0, rotation=0, "
            "touchscreen=unknown, "
            "panel_corners_radii=0.000000,0.000000,0.000000,0.000000, "
            "panel_orientation=Normal, detected=true, "
            "color_space="
            "{primaries:BT709, transfer:SRGB, matrix:RGB, range:FULL}");

  EXPECT_EQ(info.ToFullString(),
            "ManagedDisplayInfo[10] name=\"Display-10\", port_display_id=10, "
            "edid_display_id=20, "
            "native bounds=0,0 200x100, size=200x100, refresh-rate=60.000000, "
            "device-scale=1, display-zoom=1, overscan=x:0,0 y:0,0, rotation=0, "
            "touchscreen=unknown, "
            "panel_corners_radii=0.000000,0.000000,0.000000,0.000000, "
            "panel_orientation=Normal, detected=true, "
            "color_space="
            "{primaries:BT709, transfer:SRGB, matrix:RGB, range:FULL}, "
            "display_modes==(200x100@60P(N) 1)");
}

TEST_F(DisplayInfoTest, ClampOverscanInsets) {
  // 1x display: max overscan is 1/4 of pixel dimensions (200 horizontal,
  // 150 vertical), preserving at least 1/2 width and height.
  ManagedDisplayInfo info =
      ManagedDisplayInfo::CreateFromSpecWithID("800x600", 10);
  info.SetOverscanInsets(gfx::Insets(500));
  info.UpdateDisplaySize();
  EXPECT_EQ(gfx::Insets::VH(150, 200), info.overscan_insets_in_dip());
  EXPECT_EQ(gfx::Insets::VH(150, 200), info.GetOverscanInsetsInPixel());
  EXPECT_EQ((gfx::Size{400, 300}), info.size_in_pixel());

  // Negative and opposite-sign insets are clamped to >= 0 per edge.
  info.SetOverscanInsets(gfx::Insets::TLBR(-10, -20, 5, 10));
  info.UpdateDisplaySize();
  EXPECT_EQ(gfx::Insets::TLBR(0, 0, 5, 10), info.overscan_insets_in_dip());
  EXPECT_EQ(gfx::Insets::TLBR(0, 0, 5, 10), info.GetOverscanInsetsInPixel());
  EXPECT_EQ((gfx::Size{790, 595}), info.size_in_pixel());

  info.SetOverscanInsets(gfx::Insets::TLBR(-5, 0, 5, 0));
  info.UpdateDisplaySize();
  EXPECT_EQ(gfx::Insets::TLBR(0, 0, 5, 0), info.overscan_insets_in_dip());
  EXPECT_EQ((gfx::Size{800, 595}), info.size_in_pixel());

  info.SetOverscanInsets(gfx::Insets(-5));
  info.UpdateDisplaySize();
  EXPECT_EQ(gfx::Insets(), info.overscan_insets_in_dip());
  EXPECT_EQ((gfx::Size{800, 600}), info.size_in_pixel());

  // 2x display (1920x1080*2): max overscan is 480x270 px = 240x135 DIP.
  info = ManagedDisplayInfo::CreateFromSpecWithID("1920x1080*2", 10);
  info.SetOverscanInsets(gfx::Insets::TLBR(300, 400, 20, 30));
  info.UpdateDisplaySize();
  EXPECT_EQ(gfx::Insets::TLBR(135, 240, 20, 30), info.overscan_insets_in_dip());
  EXPECT_EQ(gfx::Insets::TLBR(270, 480, 40, 60),
            info.GetOverscanInsetsInPixel());
  EXPECT_EQ((gfx::Size{1380, 770}), info.size_in_pixel());

  // Fractional DSFs: valid small DIP insets are returned unchanged, oversized
  // insets floor in DIP while compensating for exact-quotient float division
  // error (e.g. 960 / 2.4f = 399.99997... -> 400 DIP).
  info = ManagedDisplayInfo::CreateFromSpecWithID("1366x768*1.25", 10);
  info.SetOverscanInsets(gfx::Insets(500));
  info.UpdateDisplaySize();
  EXPECT_EQ(gfx::Insets::VH(153, 272), info.overscan_insets_in_dip());

  info = ManagedDisplayInfo::CreateFromSpecWithID("3840x2160*2.4", 10);
  info.SetOverscanInsets(gfx::Insets(1000));
  info.UpdateDisplaySize();
  EXPECT_EQ(gfx::Insets::VH(225, 400), info.overscan_insets_in_dip());
  EXPECT_EQ(gfx::Insets::VH(540, 960), info.GetOverscanInsetsInPixel());

  info = ManagedDisplayInfo::CreateFromSpecWithID("3840x2160*2.66", 10);
  info.SetOverscanInsets(gfx::Insets(3));
  EXPECT_EQ(gfx::Insets(3), info.overscan_insets_in_dip());
  info.SetOverscanInsets(gfx::Insets(1000));
  info.UpdateDisplaySize();
  EXPECT_EQ(gfx::Insets::VH(202, 360), info.overscan_insets_in_dip());
  EXPECT_EQ(gfx::Insets::VH(538, 960), info.GetOverscanInsetsInPixel());
  EXPECT_EQ((gfx::Size{1920, 1084}), info.size_in_pixel());

  // Pre-SetBounds initialization clamps negative edges immediately and clamps
  // DIP and pixel overscan once SetBounds() or Copy() sets native bounds.
  ManagedDisplayInfo pre_bounds_info(20, "test", true);
  pre_bounds_info.SetOverscanInsets(gfx::Insets::TLBR(300, -10, 20, 400));
  EXPECT_EQ(gfx::Insets::TLBR(300, 0, 20, 400),
            pre_bounds_info.overscan_insets_in_dip());
  EXPECT_EQ(gfx::Insets::TLBR(300, 0, 20, 400),
            pre_bounds_info.GetOverscanInsetsInPixel());
  pre_bounds_info.SetBounds({800, 600});
  EXPECT_EQ(gfx::Insets::TLBR(150, 0, 20, 200),
            pre_bounds_info.overscan_insets_in_dip());
  EXPECT_EQ(gfx::Insets::TLBR(150, 0, 20, 200),
            pre_bounds_info.GetOverscanInsetsInPixel());
  EXPECT_EQ((gfx::Size{600, 430}), pre_bounds_info.size_in_pixel());

  pre_bounds_info.SetBounds({400, 300});
  EXPECT_EQ(gfx::Insets::TLBR(75, 0, 20, 100),
            pre_bounds_info.overscan_insets_in_dip());
  EXPECT_EQ(gfx::Insets::TLBR(75, 0, 20, 100),
            pre_bounds_info.GetOverscanInsetsInPixel());
  EXPECT_EQ((gfx::Size{300, 205}), pre_bounds_info.size_in_pixel());

  ManagedDisplayInfo pref_info(25, "pref", true);
  pref_info.SetOverscanInsets(gfx::Insets::TLBR(277, 29, 277, 29));
  ManagedDisplayInfo native_info =
      ManagedDisplayInfo::CreateFromSpecWithID("1920x1080*2", 25);
  native_info.set_from_native_platform(true);
  pref_info.Copy(native_info);
  pref_info.UpdateDisplaySize();
  EXPECT_EQ(gfx::Insets::TLBR(135, 29, 135, 29),
            pref_info.overscan_insets_in_dip());
  EXPECT_EQ(gfx::Insets::TLBR(270, 58, 270, 58),
            pref_info.GetOverscanInsetsInPixel());

  // Panel orientation transposes pixel dimensions before computing 1/4 max.
  ManagedDisplayInfo panel_oriented_info(30, "panel", true);
  panel_oriented_info.set_panel_orientation(PanelOrientation::kLeftUp);
  panel_oriented_info.SetBounds({800, 600});
  panel_oriented_info.SetOverscanInsets(gfx::Insets(500));
  panel_oriented_info.UpdateDisplaySize();
  EXPECT_EQ(gfx::Insets::VH(200, 150),
            panel_oriented_info.overscan_insets_in_dip());
  EXPECT_EQ(gfx::Insets::VH(200, 150),
            panel_oriented_info.GetOverscanInsetsInPixel());
  EXPECT_EQ((gfx::Size{300, 400}), panel_oriented_info.size_in_pixel());
}

}  // namespace display
