// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Because the unit tests for gfx::Image are spread across multiple
// implementation files, this header contains the reusable components.

#ifndef UI_GFX_IMAGE_IMAGE_UNITTEST_UTIL_H_
#define UI_GFX_IMAGE_IMAGE_UNITTEST_UTIL_H_

#include <stdint.h>

#include "base/containers/span.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image.h"

namespace gfx {
namespace test {

#if BUILDFLAG(IS_IOS)
typedef UIImage* PlatformImage;
#elif BUILDFLAG(IS_MAC)
typedef NSImage* PlatformImage;
#else
typedef gfx::ImageSkia PlatformImage;
#endif

// Create a bitmap of `size`x`size` and color `color`.
const SkBitmap CreateBitmap(int size, SkColor color = SK_ColorGREEN);

// Create a bitmap of `width`x`height` and color `color`.
const SkBitmap CreateBitmap(int width,
                            int height,
                            SkColor color = SK_ColorGREEN);

// Creates an ImageSkia of `size`x`size` DIP and color `color` with bitmap
// data for an arbitrary scale factor.
gfx::ImageSkia CreateImageSkia(int size, SkColor color = SK_ColorGREEN);

// Creates an ImageSkia of `width`x`height` DIP and color `color` with bitmap
// data for an arbitrary scale factor.
gfx::ImageSkia CreateImageSkia(int width,
                               int height,
                               SkColor color = SK_ColorGREEN);

// Returns PNG encoded bytes for a bitmap of |edge_size|x|edge_size| and color
// `color`.
scoped_refptr<base::RefCountedMemory> CreatePNGBytes(
    int edge_size,
    SkColor color = SK_ColorGREEN);

gfx::Image CreateImage(int size, SkColor color = SK_ColorGREEN);
gfx::Image CreateImage(int width, int height, SkColor color = SK_ColorGREEN);

// Returns true if the images are equal. Converts the images to ImageSkia to
// compare them.
bool AreImagesEqual(const gfx::Image& image1, const gfx::Image& image2);

// Returns true if the images are visually similar. |max_deviation| is the
// maximum color shift in each of the red, green, and blue components for the
// images to be considered similar. Converts to ImageSkia to compare the images.
bool AreImagesClose(const gfx::Image& image1,
                    const gfx::Image& image2,
                    int max_deviation);

// Returns true if the bitmaps are equal.
bool AreBitmapsEqual(const SkBitmap& bitmap1, const SkBitmap& bitmap2);

// Returns true if the bitmaps are visually similar.
bool AreBitmapsClose(const SkBitmap& bitmap1,
                     const SkBitmap& bitmap2,
                     int max_deviation);

// Returns true if the passed in PNG bitmap is visually similar to the passed in
// SkBitmap.
bool ArePNGBytesCloseToBitmap(base::span<const uint8_t> bytes,
                              const SkBitmap& bitmap,
                              int max_deviation);

// Returns the maximum color shift in the red, green, and blue components caused
// by converting a gfx::Image between colorspaces. Color shifts occur when
// converting between NSImage & UIImage to ImageSkia.
int MaxColorSpaceConversionColorShift();

// An image which was not successfully decoded to PNG should be a red bitmap.
// Fails if the bitmap is not red.
void CheckImageIndicatesPNGDecodeFailure(const gfx::Image& image);

// Returns true if the structure of |image_skia| matches the structure
// described by |width|, |height|, and |scale_factors|.
// The structure matches if:
// - |image_skia| is non null.
// - |image_skia| has ImageSkiaReps of |scale_factors|.
// - Each of the ImageSkiaReps has a pixel size of |image_skia|.size() *
//   scale_factor.
bool ImageSkiaStructureMatches(
    const gfx::ImageSkia& image_skia,
    int width,
    int height,
    const std::vector<float>& scale_factors);

bool IsEmpty(const gfx::Image& image);

PlatformImage CreatePlatformImage();

gfx::Image::RepresentationType GetPlatformRepresentationType();

PlatformImage ToPlatformType(const gfx::Image& image);
gfx::Image CopyViaPlatformType(const gfx::Image& image);

SkColor GetPlatformImageColor(PlatformImage image, int x, int y);
void CheckColors(SkColor color1, SkColor color2);
void CheckIsTransparent(SkColor color);

bool IsPlatformImageValid(PlatformImage image);

bool PlatformImagesEqual(PlatformImage image1, PlatformImage image2);

}  // namespace test
}  // namespace gfx

#endif  // UI_GFX_IMAGE_IMAGE_UNITTEST_UTIL_H_
