// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/wm/core/cursor_util.h"

#include <optional>

#include "base/numerics/safe_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
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
  // Data from `kNormalCursorResourceData` and `kLargeCursorResourceData`.
  constexpr struct {
    CursorType cursor;
    gfx::Size size[2];         // indexed by cursor size.
    gfx::Point hotspot[2][2];  // indexed by cursor size and scale.
  } kTestCases[] = {
      {CursorType::kPointer,
       {gfx::Size(25, 25), gfx::Size(64, 64)},
       {{gfx::Point(4, 4), gfx::Point(7, 7)},
        {gfx::Point(10, 10), gfx::Point(20, 20)}}},
      {CursorType::kWait,
       {gfx::Size(16, 16), gfx::Size(16, 16)},
       {{gfx::Point(7, 7), gfx::Point(14, 14)},
        {gfx::Point(7, 7), gfx::Point(14, 14)}}},
  };

  for (const float scale : {0.8f, 1.0f, 1.3f, 1.5f, 2.0f, 2.5f}) {
    SCOPED_TRACE(testing::Message() << "scale " << scale);
    for (const auto size : {ui::CursorSize::kNormal, ui::CursorSize::kLarge}) {
      SCOPED_TRACE(testing::Message()
                   << "size " << base::checked_cast<int>(scale));
      for (const auto& test : kTestCases) {
        SCOPED_TRACE(test.cursor);
        constexpr auto kDefaultRotation = display::Display::ROTATE_0;
        const auto pointer_data = GetCursorData(test.cursor, size, scale,
                                                std::nullopt, kDefaultRotation);
        ASSERT_TRUE(pointer_data);
        ASSERT_GT(pointer_data->bitmaps.size(), 0u);
        EXPECT_EQ(gfx::SkISizeToSize(pointer_data->bitmaps[0].dimensions()),
                  gfx::ScaleToFlooredSize(
                      test.size[base::checked_cast<int>(size)], scale));
        const float resource_scale = ui::GetScaleForResourceScaleFactor(
            ui::GetSupportedResourceScaleFactorForRescale(scale));
        EXPECT_EQ(pointer_data->hotspot,
                  gfx::ScaleToFlooredPoint(
                      test.hotspot[base::checked_cast<int>(size)]
                                  [base::checked_cast<int>(resource_scale) - 1],
                      scale / resource_scale));
      }
    }
  }
}

}  // namespace
}  // namespace wm
