// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/nine_image_painter.h"

#include "base/base64.h"
#include "base/strings/strcat.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace gfx {

static std::string GetPNGDataUrl(const SkBitmap& bitmap) {
  std::vector<unsigned char> png_data;
  gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &png_data);
  return base::StrCat({"data:image/png;base64,", base::Base64Encode(png_data)});
}

void ExpectRedWithGreenRect(const SkBitmap& bitmap,
                            const Rect& outer_rect,
                            const Rect& green_rect) {
  for (int y = outer_rect.y(); y < outer_rect.bottom(); y++) {
    SCOPED_TRACE(y);
    for (int x = outer_rect.x(); x < outer_rect.right(); x++) {
      SCOPED_TRACE(x);
      if (green_rect.Contains(x, y)) {
        ASSERT_EQ(SK_ColorGREEN, bitmap.getColor(x, y))
            << "Output image:\n" << GetPNGDataUrl(bitmap);
      } else {
        ASSERT_EQ(SK_ColorRED, bitmap.getColor(x, y)) << "Output image:\n"
                                                      << GetPNGDataUrl(bitmap);
      }
    }
  }
}

TEST(NineImagePainterTest, GetSubsetRegions) {
  SkBitmap src;
  src.allocN32Pixels(40, 50);
  const ImageSkia image_skia(ImageSkiaRep(src, 1.0));
  const auto insets = gfx::Insets::TLBR(1, 2, 3, 4);
  std::vector<Rect> rects;
  NineImagePainter::GetSubsetRegions(image_skia, insets, &rects);
  ASSERT_EQ(9u, rects.size());
  EXPECT_EQ(gfx::Rect(0, 0, 2, 1), rects[0]);
  EXPECT_EQ(gfx::Rect(2, 0, 34, 1), rects[1]);
  EXPECT_EQ(gfx::Rect(36, 0, 4, 1), rects[2]);
  EXPECT_EQ(gfx::Rect(0, 1, 2, 46), rects[3]);
  EXPECT_EQ(gfx::Rect(2, 1, 34, 46), rects[4]);
  EXPECT_EQ(gfx::Rect(36, 1, 4, 46), rects[5]);
  EXPECT_EQ(gfx::Rect(0, 47, 2, 3), rects[6]);
  EXPECT_EQ(gfx::Rect(2, 47, 34, 3), rects[7]);
  EXPECT_EQ(gfx::Rect(36, 47, 4, 3), rects[8]);
}

TEST(NineImagePainterTest, PaintHighDPI) {
  SkBitmap src = gfx::test::CreateBitmap(/*size=*/100, SK_ColorRED);
  src.eraseArea(SkIRect::MakeXYWH(10, 10, 80, 80), SK_ColorGREEN);

  float image_scale = 2.f;

  gfx::ImageSkia image = gfx::ImageSkia::CreateFromBitmap(src, image_scale);
  gfx::Insets insets(10);
  gfx::NineImagePainter painter(image, insets);

  bool is_opaque = true;
  gfx::Canvas canvas(gfx::Size(100, 100), image_scale, is_opaque);

  gfx::Vector2d offset(20, 10);
  canvas.Translate(offset);

  gfx::Rect bounds(0, 0, 50, 50);
  painter.Paint(&canvas, bounds);

  SkBitmap result = canvas.GetBitmap();

  gfx::Vector2d paint_offset =
      gfx::ToFlooredVector2d(gfx::ScaleVector2d(offset, image_scale));
  gfx::Rect green_rect = gfx::Rect(10, 10, 80, 80) + paint_offset;
  gfx::Rect outer_rect = gfx::Rect(100, 100) + paint_offset;
  ExpectRedWithGreenRect(result, outer_rect, green_rect);
}

TEST(NineImagePainterTest, PaintStaysInBounds) {
  // In this test the bounds rect is 1x1 but each image is 2x2.
  // The NineImagePainter should not paint outside the bounds.
  // The border images should be cropped, but still painted.

  SkBitmap src = gfx::test::CreateBitmap(/*size=*/6, SK_ColorGREEN);
  src.erase(SK_ColorRED, SkIRect::MakeXYWH(2, 2, 2, 2));

  gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(src);
  gfx::Insets insets(2);
  gfx::NineImagePainter painter(image, insets);

  int image_scale = 1;
  bool is_opaque = true;
  gfx::Canvas canvas(gfx::Size(3, 3), image_scale, is_opaque);
  canvas.DrawColor(SK_ColorBLACK);

  gfx::Rect bounds(1, 1, 1, 1);
  painter.Paint(&canvas, bounds);

  SkBitmap result = canvas.GetBitmap();

  EXPECT_EQ(SK_ColorGREEN, result.getColor(1, 1));

  EXPECT_EQ(SK_ColorBLACK, result.getColor(0, 0));
  EXPECT_EQ(SK_ColorBLACK, result.getColor(0, 1));
  EXPECT_EQ(SK_ColorBLACK, result.getColor(0, 2));
  EXPECT_EQ(SK_ColorBLACK, result.getColor(1, 0));
  EXPECT_EQ(SK_ColorBLACK, result.getColor(1, 2));
  EXPECT_EQ(SK_ColorBLACK, result.getColor(2, 0));
  EXPECT_EQ(SK_ColorBLACK, result.getColor(2, 1));
  EXPECT_EQ(SK_ColorBLACK, result.getColor(2, 2));
}

TEST(NineImagePainterTest, PaintWithBoundOffset) {
  SkBitmap src = gfx::test::CreateBitmap(/*size=*/10, SK_ColorRED);
  src.eraseArea(SkIRect::MakeXYWH(1, 1, 8, 8), SK_ColorGREEN);

  gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(src);
  gfx::Insets insets(1);
  gfx::NineImagePainter painter(image, insets);

  bool is_opaque = true;
  gfx::Canvas canvas(gfx::Size(10, 10), 1, is_opaque);

  gfx::Rect bounds(1, 1, 10, 10);
  painter.Paint(&canvas, bounds);

  SkBitmap result = canvas.GetBitmap();

  SkIRect green_rect = SkIRect::MakeLTRB(2, 2, 10, 10);
  for (int y = 1; y < 10; y++) {
    for (int x = 1; x < 10; x++) {
      if (green_rect.contains(x, y)) {
        ASSERT_EQ(SK_ColorGREEN, result.getColor(x, y));
      } else {
        ASSERT_EQ(SK_ColorRED, result.getColor(x, y));
      }
    }
  }
}

TEST(NineImagePainterTest, PaintWithScale) {
  SkBitmap src = gfx::test::CreateBitmap(/*size=*/100, SK_ColorRED);
  src.eraseArea(SkIRect::MakeXYWH(10, 10, 80, 80), SK_ColorGREEN);

  float image_scale = 2.f;

  gfx::ImageSkia image = gfx::ImageSkia::CreateFromBitmap(src, image_scale);
  gfx::Insets insets(10);
  gfx::NineImagePainter painter(image, insets);

  bool is_opaque = true;
  gfx::Canvas canvas(gfx::Size(400, 400), image_scale, is_opaque);

  gfx::Vector2d offset(20, 10);
  canvas.Translate(offset);
  canvas.Scale(2, 1);

  gfx::Rect bounds(0, 0, 50, 50);
  painter.Paint(&canvas, bounds);

  SkBitmap result = canvas.GetBitmap();

  gfx::Vector2d paint_offset =
      gfx::ToFlooredVector2d(gfx::ScaleVector2d(offset, image_scale));
  gfx::Rect green_rect = gfx::Rect(20, 10, 160, 80) + paint_offset;
  gfx::Rect outer_rect = gfx::Rect(200, 100) + paint_offset;
  ExpectRedWithGreenRect(result, outer_rect, green_rect);
}

TEST(NineImagePainterTest, PaintWithNegativeScale) {
  SkBitmap src = gfx::test::CreateBitmap(/*size=*/100, SK_ColorRED);
  src.eraseArea(SkIRect::MakeXYWH(10, 10, 80, 80), SK_ColorGREEN);

  float image_scale = 2.f;

  gfx::ImageSkia image = gfx::ImageSkia::CreateFromBitmap(src, image_scale);
  gfx::Insets insets(10);
  gfx::NineImagePainter painter(image, insets);

  bool is_opaque = true;
  gfx::Canvas canvas(gfx::Size(400, 400), image_scale, is_opaque);
  canvas.Translate(gfx::Vector2d(70, 60));
  canvas.Scale(-1, -1);

  gfx::Rect bounds(0, 0, 50, 50);
  painter.Paint(&canvas, bounds);

  SkBitmap result = canvas.GetBitmap();

  // The painting space is 50x50 and the scale of -1,-1 means an offset of 50,50
  // would put the output in the top left corner. Since the offset is 70,60 it
  // moves by 20,10. Since the output is 2x DPI it will become offset by 40,20.
  gfx::Vector2d paint_offset(40, 20);
  gfx::Rect green_rect = gfx::Rect(10, 10, 80, 80) + paint_offset;
  gfx::Rect outer_rect = gfx::Rect(100, 100) + paint_offset;
  ExpectRedWithGreenRect(result, outer_rect, green_rect);
}

}  // namespace gfx
