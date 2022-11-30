// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/cursor_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point.h"

namespace wm {
namespace {

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

}  // namespace
}  // namespace wm
