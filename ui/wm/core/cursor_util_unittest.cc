// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/cursor_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace wm {
namespace {

using ::ui::mojom::CursorType;

// Parameterized test for cursor bitmaps with premultiplied and unpremultiplied
// alpha.
class CursorUtilTest : public testing::TestWithParam<bool> {
 public:
  SkColor GetPixelColor() {
    return GetParam() ? SkColorSetARGB(128, 255, 0, 0)
                      : SkColorSetARGB(128, 128, 0, 0);
  }
  SkImageInfo GetImageInfo() {
    return GetParam() ? SkImageInfo::MakeN32(10, 5, kUnpremul_SkAlphaType)
                      : SkImageInfo::MakeN32(10, 5, kPremul_SkAlphaType);
  }
};

TEST_P(CursorUtilTest, ScaleAndRotate) {
  const SkColor pixel_color = GetPixelColor();

  SkBitmap bitmap;
  bitmap.setInfo(GetImageInfo());
  bitmap.allocPixels();
  bitmap.eraseColor(pixel_color);

  gfx::Point hotpoint(3, 4);

  ScaleAndRotateCursorBitmapAndHotpoint(1.0f, display::Display::ROTATE_0,
                                        &bitmap, &hotpoint);
  EXPECT_EQ(10, bitmap.width());
  EXPECT_EQ(5, bitmap.height());
  EXPECT_EQ("3,4", hotpoint.ToString());
  EXPECT_EQ(pixel_color, bitmap.pixmap().getColor(0, 0));

  ScaleAndRotateCursorBitmapAndHotpoint(1.0f, display::Display::ROTATE_90,
                                        &bitmap, &hotpoint);
  EXPECT_EQ(5, bitmap.width());
  EXPECT_EQ(10, bitmap.height());
  EXPECT_EQ("1,3", hotpoint.ToString());
  EXPECT_EQ(pixel_color, bitmap.pixmap().getColor(0, 0));

  ScaleAndRotateCursorBitmapAndHotpoint(2.0f, display::Display::ROTATE_180,
                                        &bitmap, &hotpoint);
  EXPECT_EQ(10, bitmap.width());
  EXPECT_EQ(20, bitmap.height());
  EXPECT_EQ("8,14", hotpoint.ToString());
  EXPECT_EQ(pixel_color, bitmap.pixmap().getColor(0, 0));

  ScaleAndRotateCursorBitmapAndHotpoint(1.0f, display::Display::ROTATE_270,
                                        &bitmap, &hotpoint);
  EXPECT_EQ(20, bitmap.width());
  EXPECT_EQ(10, bitmap.height());
  EXPECT_EQ("14,2", hotpoint.ToString());
  EXPECT_EQ(pixel_color, bitmap.pixmap().getColor(0, 0));
}

INSTANTIATE_TEST_SUITE_P(All, CursorUtilTest, testing::Bool());

TEST(CursorUtil, GetCursorData) {
  const auto kDefaultSize = ui::CursorSize::kNormal;
  const float kDefaultScale = 1.0f;
  const auto kDefaultRotation = display::Display::ROTATE_0;

  const auto pointer_cursor_data = GetCursorData(
      CursorType::kPointer, kDefaultSize, kDefaultScale, kDefaultRotation);
  ASSERT_TRUE(pointer_cursor_data);
  EXPECT_EQ(pointer_cursor_data->bitmaps.size(), 1u);
  EXPECT_FALSE(pointer_cursor_data->hotspot.IsOrigin());

  const auto wait_cursor_data = GetCursorData(CursorType::kWait, kDefaultSize,
                                              kDefaultScale, kDefaultRotation);
  ASSERT_TRUE(wait_cursor_data);
  EXPECT_GT(wait_cursor_data->bitmaps.size(), 1u);
  EXPECT_FALSE(wait_cursor_data->hotspot.IsOrigin());

  // Test for different scale factors.

  // Data from CursorType::kPointer resources:
  const auto kSize = gfx::Size(25, 25);
  const auto kHotspot1x = gfx::Point(4, 4);
  const auto kHotspot2x = gfx::Point(7, 7);

  bool resource_2x_available =
      ui::ResourceBundle::GetSharedInstance().GetMaxResourceScaleFactor() ==
      ui::k200Percent;

  const float kScales[] = {0.8f, 1.0f, 1.3f, 1.5f, 2.0f, 2.5f};
  for (const auto scale : kScales) {
    const auto pointer_data = GetCursorData(CursorType::kPointer, kDefaultSize,
                                            scale, kDefaultRotation);
    ASSERT_TRUE(pointer_data);
    ASSERT_EQ(pointer_data->bitmaps.size(), 1u);
    // TODO(https://crbug.com/1193775): fractional scales are not supported, and
    // only the bitmap is scaled.
    EXPECT_EQ(gfx::SkISizeToSize(pointer_data->bitmaps[0].dimensions()),
              gfx::ScaleToCeiledSize(kSize, scale));  // ImageSkia uses ceil.
    EXPECT_EQ(pointer_data->hotspot, scale == 1.0f || !resource_2x_available
                                         ? kHotspot1x
                                         : kHotspot2x);
  }
}

}  // namespace
}  // namespace wm
