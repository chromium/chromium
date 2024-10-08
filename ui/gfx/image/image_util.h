// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IMAGE_IMAGE_UTIL_H_
#define UI_GFX_IMAGE_IMAGE_UTIL_H_

#include <stddef.h>

#include <vector>

#include "base/containers/span.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {
class Image;
class ImageSkia;
}

namespace gfx {

// Creates an image from the given JPEG-encoded input. If there was an error
// creating the image, returns an IsEmpty() Image.
GFX_EXPORT Image ImageFrom1xJPEGEncodedData(base::span<const uint8_t> input);

// Returns the JPEG-encoded bytes of the 1x representation of the given image.
//
// Returns the data if the image has a 1x representation and the 1x
// representation was encoded successfully. Returns nullopt otherwise.
//
// `quality` determines the compression level, 0 == lowest, 100 == highest.
GFX_EXPORT std::optional<std::vector<uint8_t>> JPEG1xEncodedDataFromImage(
    const Image& image,
    int quality);

GFX_EXPORT std::optional<std::vector<uint8_t>>
JPEG1xEncodedDataFromSkiaRepresentation(const Image& image, int quality);

// Returns the WebP-encoded bytes of the the given image.
//
// Returns the data if the image was encoded (lossy) successfully. Returns
// nullopt otherwise.
//
// `quality` determines the visual quality, 0 == lowest, 100 == highest.
GFX_EXPORT std::optional<std::vector<uint8_t>> WebpEncodedDataFromImage(
    const Image& image,
    int quality);

// Computes the width of any nearly-transparent regions at the sides of the
// image and returns them.  This checks each column of pixels from the outsides
// in, looking for anything with alpha above a reasonably small value.  For a
// fully-opaque image, the margins will thus be (0, 0); for a fully-transparent
// image, the margins will be (width / 2, width / 2), with `left` getting the
// extra pixel for odd widths.
struct GFX_EXPORT VisibleMargins {
  int left = 0;
  int right = 0;
};
GFX_EXPORT VisibleMargins GetVisibleMargins(const ImageSkia& image);

// Returns a resized Image from the provided Image.
// The resizing operation uses skia::ImageOperations::RESIZE_GOOD quality.
// This function is safe to use with any valid Image and gfx::Size objects.
// Returns:
// - If the provided image has a scale other than 1.0f, or if it already has the
//   requested size, the function returns the original Image object unchanged.
// - Otherwise, it returns a new Image object containing a resized version of
//   the original.
GFX_EXPORT Image ResizedImage(const Image& image, const gfx::Size& size);

// Downsizes the image if its area exceeds kSearchByImageMaxImageArea AND
// (either its width exceeds kSearchByImageMaxImageWidth OR its height exceeds
// kSearchByImageMaxImageHeight) in preparation for searching.
GFX_EXPORT Image ResizedImageForSearchByImage(const Image& image);

// Downsizes the image if its area exceeds the max_area defined AND (either its
// width exceeds the max_width defined OR its height exceeds the max_height
// defined).
GFX_EXPORT Image ResizedImageForMaxDimensions(const Image& image,
                                              int max_width,
                                              int max_height,
                                              int max_area);

}  // namespace gfx

#endif  // UI_GFX_IMAGE_IMAGE_UTIL_H_
