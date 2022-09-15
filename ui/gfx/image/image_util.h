// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IMAGE_IMAGE_UTIL_H_
#define UI_GFX_IMAGE_IMAGE_UTIL_H_

#include <stddef.h>

#include <vector>

#include "ui/gfx/gfx_export.h"

namespace gfx {
class Image;
class ImageSkia;
}

namespace gfx {

// Creates an image from the given JPEG-encoded input. If there was an error
// creating the image, returns an IsEmpty() Image.
GFX_EXPORT Image ImageFrom1xJPEGEncodedData(const unsigned char* input,
                                            size_t input_size);

// Fills the |dst| vector with JPEG-encoded bytes of the 1x representation of
// the given image.
// Returns true if the image has a 1x representation and the 1x representation
// was encoded successfully.
// |quality| determines the compression level, 0 == lowest, 100 == highest.
// Returns true if the Image was encoded successfully.
GFX_EXPORT bool JPEG1xEncodedDataFromImage(const Image& image,
                                           int quality,
                                           std::vector<unsigned char>* dst);

bool JPEG1xEncodedDataFromSkiaRepresentation(const Image& image,
                                             int quality,
                                             std::vector<unsigned char>* dst);

// Fills the |dst| vector with WebP-encoded bytes of the the given image.
// Returns true if the image was encoded (lossy) successfully.
// |quality| determines the visual quality, 0 == lowest, 100 == highest.
// Returns true if the Image was encoded successfully.
GFX_EXPORT bool WebpEncodedDataFromImage(const Image& image,
                                         int quality,
                                         std::vector<unsigned char>* dst);

// Computes the width of any nearly-transparent regions at the sides of the
// image and returns them in |left| and |right|.  This checks each column of
// pixels from the outsides in, looking for anything with alpha above a
// reasonably small value.  For a fully-opaque image, the margins will thus be
// (0, 0); for a fully-transparent image, the margins will be
// (width / 2, width / 2), with |left| getting the extra pixel for odd widths.
GFX_EXPORT void GetVisibleMargins(const ImageSkia& image,
                                  int* left,
                                  int* right);

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

Image ResizedImageForSearchByImageSkiaRepresentation(const Image& image);
Image ResizedImageForMaxDimensionsSkiaRepresentation(const Image& image,
                                                     int max_width,
                                                     int max_height,
                                                     int max_area);

}  // namespace gfx

#endif  // UI_GFX_IMAGE_IMAGE_UTIL_H_
