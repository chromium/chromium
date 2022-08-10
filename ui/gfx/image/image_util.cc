// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_util.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "base/check.h"
#include "base/cxx17_backports.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/webp_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/resize_image_dimensions.h"

namespace {

// Returns whether column |x| of |bitmap| has any "visible pixels", where
// "visible" is defined as having an opactiy greater than an arbitrary small
// value.
bool ColumnHasVisiblePixels(const SkBitmap& bitmap, int x) {
  const SkAlpha kMinimumVisibleOpacity = 12;
  for (int y = 0; y < bitmap.height(); ++y) {
    if (SkColorGetA(bitmap.getColor(x, y)) > kMinimumVisibleOpacity)
      return true;
  }
  return false;
}

}  // namespace

namespace gfx {

// The iOS implementations of the JPEG functions are in image_util_ios.mm.
#if !BUILDFLAG(IS_IOS)

Image ImageFrom1xJPEGEncodedData(const unsigned char* input,
                                 size_t input_size) {
  std::unique_ptr<SkBitmap> bitmap(gfx::JPEGCodec::Decode(input, input_size));
  if (bitmap.get())
    return Image::CreateFrom1xBitmap(*bitmap);

  return Image();
}

Image ResizedImageForSearchByImage(const Image& image) {
  return ResizedImageForSearchByImageSkiaRepresentation(image);
}

Image ResizedImageForMaxDimensions(const Image& image,
                                   int max_width,
                                   int max_height,
                                   int max_area) {
  return ResizedImageForMaxDimensionsSkiaRepresentation(image, max_width,
                                                        max_height, max_area);
}

// The MacOS implementation of this function is in image_utils_mac.mm.
#if !BUILDFLAG(IS_MAC)
bool JPEG1xEncodedDataFromImage(const Image& image,
                                int quality,
                                std::vector<unsigned char>* dst) {
  return JPEG1xEncodedDataFromSkiaRepresentation(image, quality, dst);
}
#endif  // !BUILDFLAG(IS_MAC)

bool JPEG1xEncodedDataFromSkiaRepresentation(const Image& image,
                                             int quality,
                                             std::vector<unsigned char>* dst) {
  const gfx::ImageSkiaRep& image_skia_rep =
      image.AsImageSkia().GetRepresentation(1.0f);
  if (image_skia_rep.scale() != 1.0f)
    return false;

  const SkBitmap& bitmap = image_skia_rep.GetBitmap();
  if (!bitmap.readyToDraw())
    return false;

  return gfx::JPEGCodec::Encode(bitmap, quality, dst);
}

bool WebpEncodedDataFromImage(const Image& image,
                              int quality,
                              std::vector<unsigned char>* dst) {
  const SkBitmap bitmap = image.AsBitmap();
  return gfx::WebpCodec::Encode(bitmap, quality, dst);
}

Image ResizedImageForSearchByImageSkiaRepresentation(const Image& image) {
  return ResizedImageForMaxDimensionsSkiaRepresentation(
      image, kSearchByImageMaxImageWidth, kSearchByImageMaxImageHeight,
      kSearchByImageMaxImageArea);
}

Image ResizedImageForMaxDimensionsSkiaRepresentation(const Image& image,
                                                     int max_width,
                                                     int max_height,
                                                     int max_area) {
  const gfx::ImageSkiaRep& image_skia_rep =
      image.AsImageSkia().GetRepresentation(1.0f);
  if (image_skia_rep.scale() != 1.0f)
    return image;

  const SkBitmap& bitmap = image_skia_rep.GetBitmap();
  if (bitmap.height() * bitmap.width() > max_area &&
      (bitmap.width() > max_width || bitmap.height() > max_height)) {
    double scale = std::min(static_cast<double>(max_width) / bitmap.width(),
                            static_cast<double>(max_height) / bitmap.height());
    int width = base::clamp<int>(scale * bitmap.width(), 1, max_width);
    int height = base::clamp<int>(scale * bitmap.height(), 1, max_height);
    SkBitmap new_bitmap = skia::ImageOperations::Resize(
        bitmap, skia::ImageOperations::RESIZE_GOOD, width, height);
    return Image(ImageSkia(ImageSkiaRep(new_bitmap, 0.0f)));
  }

  return image;
}
#endif  // !BUILDFLAG(IS_IOS)

void GetVisibleMargins(const ImageSkia& image, int* left, int* right) {
  *left = 0;
  *right = 0;
  if (!image.HasRepresentation(1.f))
    return;
  const SkBitmap& bitmap = image.GetRepresentation(1.f).GetBitmap();
  if (bitmap.drawsNothing() || bitmap.isOpaque())
    return;

  int x = 0;
  for (; x < bitmap.width(); ++x) {
    if (ColumnHasVisiblePixels(bitmap, x)) {
      *left = x;
      break;
    }
  }

  if (x == bitmap.width()) {
    // Image is fully transparent.  Divide the width in half, giving the leading
    // region the extra pixel for odd widths.
    *left = (bitmap.width() + 1) / 2;
    *right = bitmap.width() - *left;
    return;
  }

  // Since we already know column *left is non-transparent, we can avoid
  // rechecking that column; hence the '>' here.
  for (x = bitmap.width() - 1; x > *left; --x) {
    if (ColumnHasVisiblePixels(bitmap, x))
      break;
  }
  *right = bitmap.width() - 1 - x;
}

SkBitmap ResizeImageToLargestSize(const SkBitmap& image,
                                  uint32_t max_image_size) {
  uint32_t max_dimension = std::max(image.width(), image.height());
  if (max_dimension <= max_image_size)
    return image;
  // Proportionally resize the minimal image to fit in a box of size
  // max_image_size.
  return skia::ImageOperations::Resize(
      image, skia::ImageOperations::RESIZE_BEST,
      base::checked_cast<uint32_t>(image.width()) * max_image_size /
          max_dimension,
      base::checked_cast<uint32_t>(image.height()) * max_image_size /
          max_dimension);
}

void FilterAndResizeImagesForMaximalSize(
    const std::vector<SkBitmap>& images,
    uint32_t max_image_size,
    std::vector<SkBitmap>& filtered_images,
    std::vector<gfx::Size>& filtered_image_sizes) {
  filtered_images.clear();
  filtered_image_sizes.clear();

  if (images.empty())
    return;

  const SkBitmap* min_image = nullptr;
  uint32_t min_image_size = std::numeric_limits<uint32_t>::max();
  // Filter the images by |max_image_size|, and also identify the smallest
  // image in case all the images are bigger than |max_image_size|.
  for (const SkBitmap& image : images) {
    uint32_t current_size = std::max(image.width(), image.height());
    if (current_size < min_image_size) {
      min_image = &image;
      min_image_size = current_size;
    }
    if (base::checked_cast<uint32_t>(image.width()) <= max_image_size &&
        base::checked_cast<uint32_t>(image.height()) <= max_image_size) {
      filtered_images.emplace_back(image);
      filtered_image_sizes.emplace_back(
          gfx::Size(image.width(), image.height()));
    }
  }
  if (!filtered_images.empty())
    return;
  // Proportionally resize the minimal image to fit in a box of size
  // |max_image_size|.
  DCHECK(min_image);
  SkBitmap resized = ResizeImageToLargestSize(*min_image, max_image_size);
  // Drop null or empty SkBitmap.
  if (resized.drawsNothing())
    return;
  filtered_images.emplace_back(resized);
  filtered_image_sizes.emplace_back(
      gfx::Size(min_image->width(), min_image->height()));
}

}  // namespace gfx
