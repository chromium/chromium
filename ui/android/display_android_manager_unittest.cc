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
constexpr int32_t kPrimaryDisplayId = 0;
}  // namespace

struct TestDisplayParams {
  int32_t sdk_display_id;
  std::string label;
  std::vector<int32_t> bounds;
  gfx::Rect scaled_bounds;
  std::vector<int32_t> insets;
  gfx::Rect scaled_work_area;
  float dip_scale;
  int32_t rotation_degrees;
  int32_t bits_per_pixel;
  int32_t bits_per_component;
  bool is_internal;
};

class DisplayAndroidManagerTest : public testing::TestWithParam<bool> {
 public:
  DisplayAndroidManagerTest()
      : env_(base::android::AttachCurrentThread()),
        display_android_manager_(false) {
    display_android_manager_.SetPrimaryDisplayId(env_, nullptr,
                                                 kPrimaryDisplayId);
  }

  void AddDisplay(const TestDisplayParams& display_params) {
    display_android_manager_.UpdateDisplay(
        env_, nullptr, display_params.sdk_display_id,
        base::android::ConvertUTF8ToJavaString(env_, display_params.label),
        base::android::ToJavaIntArray(env_, display_params.bounds),
        base::android::ToJavaIntArray(env_, display_params.insets),
        display_params.dip_scale, display_params.rotation_degrees,
        display_params.bits_per_pixel, display_params.bits_per_component,
        /* isWideColorGamut= */ false, /* isHdr= */ false,
        /* hdrMaxLuminanceRatio= */ 1.0, display_params.is_internal);
  }

  void RemoveDisplay(int32_t sdk_display_id) {
    display_android_manager_.RemoveDisplay(env_, nullptr, sdk_display_id);
  }

  display::Display GetDisplay(int32_t sdk_display_id) const {
    display::Display display;
    display_android_manager_.GetDisplayWithDisplayId(sdk_display_id, &display);

    return display;
  }

  int GetDisplaysNum() const {
    return display_android_manager_.GetNumDisplays();
  }

  void CompareDisplayParamsWithDisplay(
      const TestDisplayParams& display_params) const {
    const auto& display = GetDisplay(display_params.sdk_display_id);

    EXPECT_EQ(display.id(), display_params.sdk_display_id);
    EXPECT_EQ(display.label(), display_params.label);

    EXPECT_EQ(display.GetSizeInPixel(),
              gfx::Size(display_params.bounds[2] - display_params.bounds[0],
                        display_params.bounds[3] - display_params.bounds[1]));
    EXPECT_EQ(display.RotationAsDegree(), display_params.rotation_degrees);
    EXPECT_EQ(display.device_scale_factor(), display_params.dip_scale);
    EXPECT_EQ(display.bounds(), display_params.scaled_bounds);

    if (base::FeatureList::IsEnabled(kAndroidUseCorrectDisplayWorkArea)) {
      EXPECT_EQ(display.work_area(), display_params.scaled_work_area);
    } else {
      EXPECT_EQ(display.work_area(), display_params.scaled_bounds);
    }

    EXPECT_EQ(display.color_depth(), display_params.bits_per_pixel);
    EXPECT_EQ(display.depth_per_component(), display_params.bits_per_component);
    EXPECT_EQ(display.is_monochrome(), display_params.bits_per_component == 0);

    EXPECT_EQ(display.IsInternal(), display_params.is_internal);
  }

 private:
  raw_ptr<JNIEnv> env_;
  DisplayAndroidManager display_android_manager_;
};

INSTANTIATE_TEST_SUITE_P(DisplayAndroidManagerTest,
                         DisplayAndroidManagerTest,
                         testing::Values(true, false));

TEST_P(DisplayAndroidManagerTest, General) {
  static const TestDisplayParams primary_display_params{
      .sdk_display_id = kPrimaryDisplayId,
      .label = "label_1",
      .bounds = {0, 0, 1920, 1080},
      .scaled_bounds = {0, 0, 1536, 864},
      .insets = {10, 20, 30, 40},
      .scaled_work_area = {8, 16, 1504, 816},
      .dip_scale = 1.25,
      .rotation_degrees = 0,
      .bits_per_pixel = 24,
      .bits_per_component = 8,
      .is_internal = true};
  static const TestDisplayParams secondary_display_params{
      .sdk_display_id = 1,
      .label = "label_2",
      .bounds = {0, -1440, 900, 0},
      .scaled_bounds = {0, -1440, 900, 1440},
      .insets = {1, 2, 3, 4},
      .scaled_work_area = {1, -1438, 896, 1434},
      .dip_scale = 1,
      .rotation_degrees = 90,
      .bits_per_pixel = 12,
      .bits_per_component = 0,
      .is_internal = false};

  base::test::ScopedFeatureList scoped_feature_list;
  if (GetParam()) {
    scoped_feature_list.InitAndEnableFeature(kAndroidUseCorrectDisplayWorkArea);
  }

  // Checking the initial state
  EXPECT_EQ(GetDisplaysNum(), 0);
  EXPECT_TRUE(display::GetInternalDisplayIds().empty());

  // Adding the primary internal display
  AddDisplay(primary_display_params);
  EXPECT_EQ(GetDisplaysNum(), 1);
  EXPECT_EQ(display::GetInternalDisplayIds().size(), std::size_t(1));

  // Adding the secondary external display
  AddDisplay(secondary_display_params);
  EXPECT_EQ(GetDisplaysNum(), 2);
  EXPECT_EQ(display::GetInternalDisplayIds().size(), std::size_t(1));

  // Comparing displays
  CompareDisplayParamsWithDisplay(primary_display_params);
  CompareDisplayParamsWithDisplay(secondary_display_params);

  // Removing external display
  RemoveDisplay(secondary_display_params.sdk_display_id);
  EXPECT_EQ(display::GetInternalDisplayIds().size(), std::size_t(1));

  // Removing internal display
  RemoveDisplay(primary_display_params.sdk_display_id);

  // Checking the finished state
  EXPECT_EQ(GetDisplaysNum(), 0);
  EXPECT_TRUE(display::GetInternalDisplayIds().empty());
}

}  // namespace ui
