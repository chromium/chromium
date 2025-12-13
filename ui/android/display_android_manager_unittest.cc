// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/display_android_manager.h"

#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/ui_android_features.h"
#include "ui/display/display.h"
#include "ui/display/screen_base.h"
#include "ui/display/util/display_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

namespace {
struct DisplayParams {
  int32_t sdk_display_id;
  std::string label;
  std::vector<int32_t> bounds;     // {left, top, right, bottom} in dip
  std::vector<int32_t> work_area;  // {left, top, right, bottom} in dip
  gfx::Size size_in_pixels;
  float dip_scale;
  float pixels_per_inch_x;
  float pixels_per_inch_y;
  int32_t rotation_degrees;
  int32_t bits_per_pixel;
  int32_t bits_per_component;
  bool is_internal;
};

const DisplayParams kPrimaryDisplayParams{.sdk_display_id = 0,
                                          .label = "primary",
                                          .bounds = {0, 0, 1536, 864},
                                          .work_area = {8, 16, 1504, 816},
                                          .size_in_pixels = {1920, 1080},
                                          .dip_scale = 1.25,
                                          .pixels_per_inch_x = 160,
                                          .pixels_per_inch_y = 160,
                                          .rotation_degrees = 0,
                                          .bits_per_pixel = 24,
                                          .bits_per_component = 8,
                                          .is_internal = true};
const DisplayParams kExternalDisplayParams{.sdk_display_id = 1,
                                           .label = "external",
                                           .bounds = {0, -1440, 900, 0},
                                           .work_area = {1, -1438, 896, 1434},
                                           .size_in_pixels = {1440, 900},
                                           .dip_scale = 1,
                                           .pixels_per_inch_x = 160,
                                           .pixels_per_inch_y = 160,
                                           .rotation_degrees = 90,
                                           .bits_per_pixel = 12,
                                           .bits_per_component = 0,
                                           .is_internal = false};
const DisplayParams kSecondExternalDisplayParams{
    .sdk_display_id = 2,
    .label = "second_external",
    .bounds = {-720, 192, 0, 672},
    .work_area = {-720, 192, 0, 672},
    .size_in_pixels = {720, 480},
    .dip_scale = 1,
    .pixels_per_inch_x = 160,
    .pixels_per_inch_y = 160,
    .rotation_degrees = 0,
    .bits_per_pixel = 12,
    .bits_per_component = 0,
    .is_internal = false};
}  // namespace

class DisplayAndroidManagerTest : public testing::Test {
 public:
  DisplayAndroidManagerTest()
      : env_(base::android::AttachCurrentThread()),
        display_android_manager_(false) {
    display_android_manager_.SetPrimaryDisplayId(
        env_, kPrimaryDisplayParams.sdk_display_id);
    display_android_manager_.SetIsDisplayTopologyAvailableForTesting(true);
  }

  void AddDisplay(const DisplayParams& display_params) {
    display_android_manager_.UpdateDisplay(
        env_, display_params.sdk_display_id,
        base::android::ConvertUTF8ToJavaString(env_, display_params.label),
        base::android::ToJavaIntArray(env_, display_params.bounds),
        base::android::ToJavaIntArray(env_, display_params.work_area),
        display_params.size_in_pixels.width(),
        display_params.size_in_pixels.height(), display_params.dip_scale,
        display_params.pixels_per_inch_x, display_params.pixels_per_inch_y,
        display_params.rotation_degrees, display_params.bits_per_pixel,
        display_params.bits_per_component,
        /* isWideColorGamut= */ false, /* isHdr= */ false,
        /* hdrMaxLuminanceRatio= */ 1.0, display_params.is_internal);
  }

  void RemoveDisplay(int32_t sdk_display_id) {
    display_android_manager_.RemoveDisplay(env_, sdk_display_id);
  }

  display::Display GetDisplay(int32_t sdk_display_id) const {
    display::Display display;
    display_android_manager_.GetDisplayWithDisplayId(sdk_display_id, &display);

    return display;
  }

  int GetDisplaysNum() const {
    return display_android_manager_.GetNumDisplays();
  }

  display::Display GetDisplayNearestPoint(const gfx::Point& point) const {
    return display_android_manager_.GetDisplayNearestPoint(point);
  }

  display::Display GetDisplayMatching(const gfx::Rect& match_rect) const {
    return display_android_manager_.GetDisplayMatching(match_rect);
  }

  void CompareDisplayParamsWithDisplay(
      const DisplayParams& display_params) const {
    const auto& display = GetDisplay(display_params.sdk_display_id);

    EXPECT_EQ(display.id(), display_params.sdk_display_id);
    EXPECT_EQ(display.label(), display_params.label);

    EXPECT_EQ(display.RotationAsDegree(), display_params.rotation_degrees);
    EXPECT_EQ(display.device_scale_factor(), display_params.dip_scale);

    gfx::Rect bounds;
    bounds.SetByBounds(display_params.bounds[0], display_params.bounds[1],
                       display_params.bounds[2], display_params.bounds[3]);
    EXPECT_EQ(display.bounds(), bounds);

    if (base::FeatureList::IsEnabled(kAndroidUseCorrectDisplayWorkArea)) {
      gfx::Rect work_area;
      work_area.SetByBounds(
          display_params.work_area[0], display_params.work_area[1],
          display_params.work_area[2], display_params.work_area[3]);
      EXPECT_EQ(display.work_area(), work_area);
    } else {
      EXPECT_EQ(display.work_area(), display.bounds());
    }

    EXPECT_EQ(display.GetSizeInPixel(), display_params.size_in_pixels);

    EXPECT_EQ(display.color_depth(), display_params.bits_per_pixel);
    EXPECT_EQ(display.depth_per_component(), display_params.bits_per_component);
    EXPECT_EQ(display.is_monochrome(), display_params.bits_per_component == 0);

    EXPECT_EQ(display.IsInternal(), display_params.is_internal);
  }

 private:
  raw_ptr<JNIEnv> env_;
  DisplayAndroidManager display_android_manager_;
};

TEST_F(DisplayAndroidManagerTest, General) {
  // Checking the initial state
  EXPECT_EQ(GetDisplaysNum(), 0);
  EXPECT_TRUE(display::GetInternalDisplayIds().empty());

  // Adding the primary internal display
  AddDisplay(kPrimaryDisplayParams);
  EXPECT_EQ(GetDisplaysNum(), 1);
  EXPECT_EQ(display::GetInternalDisplayIds().size(), std::size_t(1));

  // Adding the external display
  AddDisplay(kExternalDisplayParams);
  EXPECT_EQ(GetDisplaysNum(), 2);
  EXPECT_EQ(display::GetInternalDisplayIds().size(), std::size_t(1));

  // Comparing displays
  CompareDisplayParamsWithDisplay(kPrimaryDisplayParams);
  CompareDisplayParamsWithDisplay(kExternalDisplayParams);

  // Removing external display
  RemoveDisplay(kExternalDisplayParams.sdk_display_id);
  EXPECT_EQ(display::GetInternalDisplayIds().size(), std::size_t(1));

  // Removing internal display
  RemoveDisplay(kPrimaryDisplayParams.sdk_display_id);

  // Checking the finished state
  EXPECT_EQ(GetDisplaysNum(), 0);
  EXPECT_TRUE(display::GetInternalDisplayIds().empty());
}

TEST_F(DisplayAndroidManagerTest, WorkArea) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kAndroidUseCorrectDisplayWorkArea);

  AddDisplay(kPrimaryDisplayParams);
  CompareDisplayParamsWithDisplay(kPrimaryDisplayParams);

  AddDisplay(kExternalDisplayParams);
  CompareDisplayParamsWithDisplay(kExternalDisplayParams);
}

TEST_F(DisplayAndroidManagerTest, DisplayTopology) {
  // Display Topology
  //             (0, -1440) +-------------+
  //                        |             |
  //                        |             |
  //                        |             |
  //                        |             |
  //                        |             |
  //                        |             |
  //                        |             | (900, 0)
  //                 (0, 0) +-------------+----------+
  //                        |                        |
  //  (-720, 192) +---------+                        |
  //              |         |                        |
  //              +---------+ (0, 672)               |
  //                        |                        |
  //                        +------------------------+ (1536, 864)

  AddDisplay(kPrimaryDisplayParams);
  AddDisplay(kExternalDisplayParams);
  AddDisplay(kSecondExternalDisplayParams);

  // Point is located on the border between primary and second external
  // displays. This border belongs to the primary display.
  EXPECT_EQ(GetDisplayNearestPoint(gfx::Point(0, 432)).id(),
            kPrimaryDisplayParams.sdk_display_id);
  {
    // Point is located on the equal distance from external and second
    // external displays. Each of them can be returned.
    const auto display_id = GetDisplayNearestPoint(gfx::Point(-384, -192)).id();
    EXPECT_TRUE(display_id == kExternalDisplayParams.sdk_display_id ||
                display_id == kSecondExternalDisplayParams.sdk_display_id);
  }
  // Point is located near primary and external displays. If the display is
  // determined by pixel coordinates, the primary display will be closer to the
  // point.
  EXPECT_EQ(GetDisplayNearestPoint(gfx::Point(1800, -700)).id(),
            kExternalDisplayParams.sdk_display_id);
  // Point is at the corner of the external display.
  EXPECT_EQ(GetDisplayNearestPoint(gfx::Point(900, -1440)).id(),
            kExternalDisplayParams.sdk_display_id);
  // Point doesn't belong to any displays.
  EXPECT_EQ(GetDisplayNearestPoint(gfx::Point(-900, -500)).id(),
            kSecondExternalDisplayParams.sdk_display_id);

  // Rectangle covers all displays. Display with the most area should be
  // returned.
  EXPECT_EQ(GetDisplayMatching(gfx::Rect(-720, -1440, 2256, 2304)).id(),
            kPrimaryDisplayParams.sdk_display_id);
  // Rectangle intersects with primary and external displays. The center of the
  // rectangle is located on the border of these displays.
  EXPECT_EQ(GetDisplayMatching(gfx::Rect(800, -100, 200, 200)).id(),
            kPrimaryDisplayParams.sdk_display_id);
  // Rectangle intersects with primary and external displays. If the
  // intersection area is counted in pixel coordinates, the primary display will
  // have the most intersection area.
  EXPECT_EQ(GetDisplayMatching(gfx::Rect(800, -1000, 1100, 900)).id(),
            kExternalDisplayParams.sdk_display_id);
  // Rectangle is fully located inside second_external_display.
  EXPECT_EQ(GetDisplayMatching(gfx::Rect(100, -1340, 700, 1240)).id(),
            kExternalDisplayParams.sdk_display_id);
  // Rectangle intersects with all displays. Display with the most
  // intersection area should be returned.
  EXPECT_EQ(GetDisplayMatching(gfx::Rect(-600, -300, 700, 600)).id(),
            kSecondExternalDisplayParams.sdk_display_id);
}

}  // namespace ui
