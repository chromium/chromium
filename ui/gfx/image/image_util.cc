// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_util.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <optional>

#include "build/build_config.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/webp_codec.h"
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

Image ImageFrom1xJPEGEncodedData(base::span<const uint8_t> input) {
  std::optional<SkBitmap> bitmap = gfx::JPEGCodec::Decode(input);
  if (bitmap) {
    return Image::CreateFrom1xBitmap(bitmap.value());
  }

  return Image();
}

Image ResizedImageForSearchByImage(const Image& image) {
  return ResizedImageForMaxDimensions(image, kSearchByImageMaxImageWidth,
                                      kSearchByImageMaxImageHeight,
                                      kSearchByImageMaxImageArea);
}

Image ResizedImageForMaxDimensions(const Image& image,
                                   int max_width,
                                   int max_height,
                                   int max_area) {
  const gfx::ImageSkiaRep& image_skia_rep =
      image.AsImageSkia().GetRepresentation(1.0f);
  if (image_skia_rep.scale() != 1.0f) {
    return image;
  }

  const SkBitmap& bitmap = image_skia_rep.GetBitmap();
  if (bitmap.height() * bitmap.width() > max_area &&
      (bitmap.width() > max_width || bitmap.height() > max_height)) {
    double scale = std::min(static_cast<double>(max_width) / bitmap.width(),
                            static_cast<double>(max_height) / bitmap.height());
    int width = std::clamp<int>(scale * bitmap.width(), 1, max_width);
    int height = std::clamp<int>(scale * bitmap.height(), 1, max_height);
    SkBitmap new_bitmap = skia::ImageOperations::Resize(
        bitmap, skia::ImageOperations::RESIZE_GOOD, width, height);
    return Image(ImageSkia(ImageSkiaRep(new_bitmap, 0.0f)));
  }

  return image;
}

// The MacOS implementation of this function is in image_utils_mac.mm.
#if !BUILDFLAG(IS_MAC)
std::optional<std::vector<uint8_t>> JPEG1xEncodedDataFromImage(
    const Image& image,
    int quality) {
  return JPEG1xEncodedDataFromSkiaRepresentation(image, quality);
}
#endif  // !BUILDFLAG(IS_MAC)

std::optional<std::vector<uint8_t>> JPEG1xEncodedDataFromSkiaRepresentation(
    const Image& image,
    int quality) {
  const gfx::ImageSkiaRep& image_skia_rep =
      image.AsImageSkia().GetRepresentation(1.0f);
  if (image_skia_rep.scale() != 1.0f) {
    return std::nullopt;
  }

  const SkBitmap& bitmap = image_skia_rep.GetBitmap();
  if (!bitmap.readyToDraw()) {
    return std::nullopt;
  }

  return gfx::JPEGCodec::Encode(bitmap, quality);
}

std::optional<std::vector<uint8_t>> WebpEncodedDataFromImage(const Image& image,
                                                             int quality) {
  const SkBitmap bitmap = image.AsBitmap();
  return gfx::WebpCodec::Encode(bitmap, quality);
}

Image ResizedImage(const Image& image, const gfx::Size& size) {
  const gfx::ImageSkiaRep& image_skia_rep =
      image.AsImageSkia().GetRepresentation(1.0f);

  if (image_skia_rep.scale() != 1.0f || image_skia_rep.pixel_size() == size) {
    return image;
  }

  SkBitmap new_bitmap = skia::ImageOperations::Resize(
      image_skia_rep.GetBitmap(), skia::ImageOperations::RESIZE_GOOD,
      size.width(), size.height());
  return Image(ImageSkia(ImageSkiaRep(new_bitmap, 0.0f)));
}
#endif  // !BUILDFLAG(IS_IOS)

VisibleMargins GetVisibleMargins(const ImageSkia& image) {
  VisibleMargins margins;
  if (!image.HasRepresentation(1.f))
    return margins;
  const SkBitmap& bitmap = image.GetRepresentation(1.f).GetBitmap();
  if (bitmap.drawsNothing() || bitmap.isOpaque())
    return margins;

  int x = 0;
  for (; x < bitmap.width(); ++x) {
    if (ColumnHasVisiblePixels(bitmap, x)) {
      margins.left = x;
      break;
    }
  }

  if (x == bitmap.width()) {
    // Image is fully transparent.  Divide the width in half, giving the leading
    // region the extra pixel for odd widths.
    margins.left = (bitmap.width() + 1) / 2;
    margins.right = bitmap.width() - margins.left;
    return margins;
  }

  // Since we already know column margins.left is non-transparent, we can avoid
  // rechecking that column; hence the '>' here.
  for (x = bitmap.width() - 1; x > margins.left; --x) {
    if (ColumnHasVisiblePixels(bitmap, x))
      break;
  }
  margins.right = bitmap.width() - 1 - x;

  return margins;
}

}  // namespace gfx
