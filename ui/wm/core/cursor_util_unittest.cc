// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/cursor_util.h"

#include <array>
#include <optional>

#include "base/compiler_specific.h"
#include "base/numerics/safe_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
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
    std::array<gfx::Size, 2> size;  // indexed by cursor size.
    gfx::Point hotspot[2][2];  // indexed by cursor size and scale.
  } kCursorTestCases[] = {
      {CursorType::kPointer,
       {gfx::Size(25, 25), gfx::Size(25, 25)},
       {{gfx::Point(6, 4), gfx::Point(12, 8)},
        {gfx::Point(6, 4), gfx::Point(12, 8)}}},
      {CursorType::kWait,
       {gfx::Size(25, 25), gfx::Size(25, 25)},
       {{gfx::Point(12, 12), gfx::Point(24, 24)},
        {gfx::Point(12, 12), gfx::Point(24, 24)}}},

  };

  for (const float scale : {0.8f, 1.0f, 1.3f, 1.5f, 2.0f, 2.5f}) {
    SCOPED_TRACE(testing::Message() << "scale " << scale);
    for (const auto size : {ui::CursorSize::kNormal, ui::CursorSize::kLarge}) {
      SCOPED_TRACE(testing::Message()
                   << "size " << base::checked_cast<int>(scale));
      for (const auto& test : kCursorTestCases) {
        SCOPED_TRACE(test.cursor);
        static constexpr auto kDefaultRotation = display::Display::ROTATE_0;
        const auto pointer_data = GetCursorData(
            test.cursor, scale, std::nullopt, kDefaultRotation, SK_ColorBLACK);
        ASSERT_TRUE(pointer_data);
        ASSERT_GT(pointer_data->bitmaps.size(), 0u);
        EXPECT_EQ(gfx::SkISizeToSize(pointer_data->bitmaps[0].dimensions()),
                  gfx::ScaleToRoundedSize(
                      test.size[base::checked_cast<int>(size)], scale));
        const float resource_scale = ui::GetScaleForResourceScaleFactor(
            ui::GetSupportedResourceScaleFactorForRescale(scale));
        UNSAFE_TODO(EXPECT_EQ(
            pointer_data->hotspot,
            gfx::ScaleToFlooredPoint(
                test.hotspot[base::checked_cast<int>(size)]
                            [base::checked_cast<int>(resource_scale) - 1],
                scale / resource_scale)));
      }
    }
  }
}

// Tests cursor bitmap is correct after size rescaling.
TEST(CursorUtil, GetCursorDataWithTargetCursorSize) {
  // Data from `kLargeCursorResourceData`.
  constexpr struct {
    CursorType cursor;
    gfx::Size size;         // large cursor size in dip
    std::array<gfx::Point, 2> hotspot;  // hotspot in px, indexed by scale.
  } kCursorTestCases[] = {{CursorType::kPointer,
                           gfx::Size(25, 25),
                           {gfx::Point(6, 4), gfx::Point(12, 8)}},
                          {CursorType::kWait,
                           gfx::Size(25, 25),
                           {gfx::Point(12, 12), gfx::Point(24, 24)}}};

  for (const float scale : {0.8f, 1.0f, 1.3f, 1.5f, 2.0f, 2.5f}) {
    SCOPED_TRACE(testing::Message() << "scale " << scale);
    for (const auto& test : kCursorTestCases) {
      SCOPED_TRACE(test.cursor);
      static constexpr auto kDefaultRotation = display::Display::ROTATE_0;
      for (const auto& target_cursor_size_in_px :
           {ui::kMinLargeCursorSize, ui::kMaxLargeCursorSize}) {
        const float resource_scale = ui::GetScaleForResourceScaleFactor(
            ui::GetSupportedResourceScaleFactorForRescale(scale));
        const auto pointer_data = GetCursorData(
            test.cursor, scale, std::make_optional(target_cursor_size_in_px),
            kDefaultRotation, SK_ColorBLACK);
        ASSERT_TRUE(pointer_data);
        ASSERT_GT(pointer_data->bitmaps.size(), 0u);

        // `target_cursor_size_in_px` should be the height of the final size of
        // cursor.
        const gfx::Size actual_cursor_size_in_px =
            gfx::SkISizeToSize(pointer_data->bitmaps[0].dimensions());
        EXPECT_EQ(actual_cursor_size_in_px.height(), target_cursor_size_in_px);

        const gfx::Point actual_hotspot_in_px = pointer_data->hotspot;
        const float rescale = static_cast<float>(target_cursor_size_in_px) /
                              static_cast<float>(test.size.height());
        const gfx::Point expected_hotspot_in_dip = gfx::ScaleToFlooredPoint(
            test.hotspot[base::checked_cast<int>(resource_scale) - 1],
            1 / resource_scale);
        const gfx::Point expected_hotspot_in_px =
            gfx::ScaleToFlooredPoint(expected_hotspot_in_dip, rescale);
        EXPECT_EQ(actual_hotspot_in_px, expected_hotspot_in_px);
      }
    }
  }
}

// Tests cursor bitmap is correct after applying color on it.
TEST(CursorUtil, GetCursorDataWithColor) {
  const struct {
    SkColor cursor_color;  // Set the cursor to this color.
    SkColor not_found;     // Spot-check: This color shouldn't be in the cursor.
    SkColor found;         // Spot-check: This color should be in the cursor.
    CursorType cursor_type;
  } kColorTestCases[] = {
      // Cursors should not have black because the black cursor body should be
      // replaced by other colors.
      {SK_ColorRED, SK_ColorBLACK, SK_ColorWHITE, CursorType::kPointer},
      {SK_ColorRED, SK_ColorBLACK, SK_ColorWHITE, CursorType::kCell},

      // Cursors should not have white because the white cursor body should be
      // replaced by other colors.
      {SK_ColorMAGENTA, SK_ColorWHITE, SK_ColorBLACK, CursorType::kHand},

      // Also cursors should still have transparent.
      {SK_ColorRED, SK_ColorGREEN, SK_ColorTRANSPARENT, CursorType::kPointer},

      // The no drop cursor has red in it, check it's still there:
      // Cursor body should be colored, but the red part shouldn't be
      // re-colored.
      {SK_ColorBLUE, SK_ColorGREEN, SkColorSetRGB(181, 70, 72),
       CursorType::kNoDrop},

      // Similarly, the copy cursor has a green part in it which should not be
      // re-colored.
      {SK_ColorBLUE, SK_ColorRED, SkColorSetRGB(57, 149, 88),
       CursorType::kCopy},
  };

  for (const auto& test : kColorTestCases) {
    ClearCursorAnimationCache();
    auto pointer_data =
        GetCursorData(test.cursor_type, 1.0f, std::nullopt,
                      display::Display::ROTATE_0, test.cursor_color);
    const SkBitmap bitmap = pointer_data->bitmaps[0];
    // We should find `cursor_color` and `found` color pixels in the cursor, but
    // no |not_found| color pixels.
    bool has_color = false;
    bool has_not_found_color = false;
    bool has_found_color = false;
    for (int x = 0; x < bitmap.width(); ++x) {
      for (int y = 0; y < bitmap.height(); ++y) {
        SkColor color = bitmap.getColor(x, y);
        if (color == test.cursor_color) {
          has_color = true;
        } else if (color == test.not_found) {
          has_not_found_color = true;
        } else if (color == test.found) {
          has_found_color = true;
        }
      }
    }
    EXPECT_TRUE(has_color);
    EXPECT_TRUE(has_found_color);
    EXPECT_FALSE(has_not_found_color);
  }
}

}  // namespace
}  // namespace wm
