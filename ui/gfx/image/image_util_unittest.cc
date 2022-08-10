// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_util.h"

#include <memory>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/image/resize_image_dimensions.h"

namespace {

SkBitmap CreateRandomImage(int size, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size, size);
  bitmap.eraseColor(color);
  return bitmap;
}

}  // namespace

TEST(ImageUtilTest, JPEGEncodeAndDecode) {
  gfx::Image original = gfx::test::CreateImage(100, 100);

  std::vector<unsigned char> encoded;
  ASSERT_TRUE(gfx::JPEG1xEncodedDataFromImage(original, 80, &encoded));

  gfx::Image decoded =
      gfx::ImageFrom1xJPEGEncodedData(&encoded.front(), encoded.size());

  // JPEG is lossy, so simply check that the image decoded successfully.
  EXPECT_FALSE(decoded.IsEmpty());
}

TEST(ImageUtilTest, GetVisibleMargins) {
  int left, right;

  // Fully transparent image.
  {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(16, 14);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    gfx::ImageSkia img(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
    gfx::GetVisibleMargins(img, &left, &right);
    EXPECT_EQ(8, left);
    EXPECT_EQ(8, right);
  }

  // Fully non-transparent image.
  {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(16, 14);
    bitmap.eraseColor(SK_ColorYELLOW);
    gfx::ImageSkia img(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
    gfx::GetVisibleMargins(img, &left, &right);
    EXPECT_EQ(0, left);
    EXPECT_EQ(0, right);
  }

  // Image with non-transparent section in center.
  {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(16, 14);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    bitmap.eraseArea(SkIRect::MakeLTRB(3, 2, 13, 13), SK_ColorYELLOW);
    gfx::ImageSkia img(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
    gfx::GetVisibleMargins(img, &left, &right);
    EXPECT_EQ(3, left);
    EXPECT_EQ(3, right);
  }

  // Image with non-transparent section skewed to one side.
  {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(16, 14);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    bitmap.eraseArea(SkIRect::MakeLTRB(3, 2, 5, 5), SK_ColorYELLOW);
    gfx::ImageSkia img(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
    gfx::GetVisibleMargins(img, &left, &right);
    EXPECT_EQ(3, left);
    EXPECT_EQ(11, right);
  }

  // Image with non-transparent section at leading edge.
  {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(16, 14);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    bitmap.eraseArea(SkIRect::MakeLTRB(0, 3, 5, 5), SK_ColorYELLOW);
    gfx::ImageSkia img(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
    gfx::GetVisibleMargins(img, &left, &right);
    EXPECT_EQ(0, left);
    EXPECT_EQ(11, right);
  }

  // Image with non-transparent section at trailing edge.
  {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(16, 14);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    bitmap.eraseArea(SkIRect::MakeLTRB(4, 3, 16, 13), SK_ColorYELLOW);
    gfx::ImageSkia img(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
    gfx::GetVisibleMargins(img, &left, &right);
    EXPECT_EQ(4, left);
    EXPECT_EQ(0, right);
  }

  // Image with narrow non-transparent section.
  {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(16, 14);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    bitmap.eraseArea(SkIRect::MakeLTRB(8, 3, 9, 5), SK_ColorYELLOW);
    gfx::ImageSkia img(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
    gfx::GetVisibleMargins(img, &left, &right);
    EXPECT_EQ(8, left);
    EXPECT_EQ(7, right);
  }

  // Image with faint pixels.
  {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(16, 14);
    bitmap.eraseColor(SkColorSetA(SK_ColorYELLOW, 0x02));
    gfx::ImageSkia img(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
    gfx::GetVisibleMargins(img, &left, &right);
    EXPECT_EQ(8, left);
    EXPECT_EQ(8, right);
  }

  // Fully transparent image with odd width.
  {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(17, 14);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    gfx::ImageSkia img(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
    gfx::GetVisibleMargins(img, &left, &right);
    EXPECT_EQ(9, left);
    EXPECT_EQ(8, right);
  }
}

TEST(ImageUtilTest, ResizedImageForSearchByImage) {
  // Make sure the image large enough to let ResizedImageForSearchByImage to
  // resize the image.
  gfx::Image original_image =
      gfx::test::CreateImage(gfx::kSearchByImageMaxImageWidth * 2,
                             gfx::kSearchByImageMaxImageHeight * 2);

  gfx::Image resized_image = gfx::ResizedImageForSearchByImage(original_image);
  EXPECT_FALSE(resized_image.IsEmpty());
  EXPECT_EQ(resized_image.Width(), gfx::kSearchByImageMaxImageWidth);
  EXPECT_EQ(resized_image.Height(), gfx::kSearchByImageMaxImageHeight);
}

TEST(ImageUtilTest, ResizedImageForSearchByImageShouldKeepRatio) {
  // Make sure the image large enough to let ResizedImageForSearchByImage to
  // resize the image.
  gfx::Image original_image = gfx::test::CreateImage(600, 600);

  gfx::Image resized_image = gfx::ResizedImageForSearchByImage(original_image);
  EXPECT_EQ(resized_image.Width(), 400);
  EXPECT_EQ(resized_image.Height(), 400);
}

TEST(ImageUtilTest, NoFilterNoResize) {
  std::vector<SkBitmap> previous_images;
  previous_images.push_back(CreateRandomImage(1, SK_ColorBLACK));
  previous_images.push_back(CreateRandomImage(2, SK_ColorGRAY));
  previous_images.push_back(CreateRandomImage(3, SK_ColorBLUE));

  std::vector<SkBitmap> filtered_images;
  std::vector<gfx::Size> filtered_sizes;
  gfx::FilterAndResizeImagesForMaximalSize(
      previous_images, /*max_image_size=*/5, filtered_images, filtered_sizes);

  // No image gets filtered.
  EXPECT_EQ(filtered_images.size(), previous_images.size());
  EXPECT_EQ(3u, filtered_sizes.size());
  EXPECT_EQ(SK_ColorBLACK, filtered_images.at(0).getColor(0, 0));
  EXPECT_EQ(SK_ColorGRAY, filtered_images.at(1).getColor(1, 1));
  EXPECT_EQ(SK_ColorBLUE, filtered_images.at(2).getColor(2, 2));
}

TEST(ImageUtilTest, FilterImageNoResize) {
  std::vector<SkBitmap> previous_images;
  previous_images.push_back(CreateRandomImage(1, SK_ColorBLACK));
  previous_images.push_back(CreateRandomImage(3, SK_ColorGRAY));
  previous_images.push_back(CreateRandomImage(4, SK_ColorBLUE));

  std::vector<SkBitmap> filtered_images;
  std::vector<gfx::Size> filtered_sizes;
  gfx::FilterAndResizeImagesForMaximalSize(
      previous_images, /*max_image_size=*/2, filtered_images, filtered_sizes);

  // No image gets filtered.
  EXPECT_EQ(1u, filtered_images.size());
  EXPECT_EQ(1u, filtered_sizes.size());
  EXPECT_EQ(SK_ColorBLACK, filtered_images.at(0).getColor(0, 0));
  // Verify grey and blue SkBitmaps are not in the filtered image.
  EXPECT_NE(SK_ColorGRAY, filtered_images.at(0).getColor(0, 0));
  EXPECT_NE(SK_ColorBLUE, filtered_images.at(0).getColor(0, 0));
}

TEST(ImageUtilTest, AllFilterOnlyResize) {
  std::vector<SkBitmap> previous_images;
  previous_images.push_back(CreateRandomImage(6, SK_ColorBLACK));
  previous_images.push_back(CreateRandomImage(5, SK_ColorGRAY));
  previous_images.push_back(CreateRandomImage(4, SK_ColorBLUE));

  std::vector<SkBitmap> filtered_images;
  std::vector<gfx::Size> filtered_sizes;
  gfx::FilterAndResizeImagesForMaximalSize(
      previous_images, /*max_image_size=*/3, filtered_images, filtered_sizes);

  // Only 1 image gets resized, and it is the smallest one (the blue one).
  // All other images are not filtered.
  EXPECT_EQ(1u, filtered_images.size());
  EXPECT_EQ(1u, filtered_sizes.size());
  // Verify that resizing happens to proper size.
  EXPECT_EQ(3, filtered_images.at(0).dimensions().width());
  EXPECT_EQ(3, filtered_images.at(0).dimensions().height());
  // Original sizes.
  EXPECT_EQ(4, filtered_sizes.at(0).width());
  EXPECT_EQ(4, filtered_sizes.at(0).height());
  // Verify grey and black SkBitmaps are not in the filtered image.
  // Only blue image is filtered.
  EXPECT_EQ(SK_ColorBLUE, filtered_images.at(0).getColor(0, 0));
  EXPECT_NE(SK_ColorGRAY, filtered_images.at(0).getColor(0, 0));
  EXPECT_NE(SK_ColorBLACK, filtered_images.at(0).getColor(0, 0));
}
