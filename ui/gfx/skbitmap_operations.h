// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SKBITMAP_OPERATIONS_H_
#define UI_GFX_SKBITMAP_OPERATIONS_H_

#include "base/gtest_prod_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/shadow_value.h"

class SkBitmap;

class GFX_EXPORT SkBitmapOperations {
 public:
  // Enum for use in rotating images (must be in 90 degree increments),
  // see: Rotate.
  enum RotationAmount {
    ROTATION_90_CW,
    ROTATION_180_CW,
    ROTATION_270_CW,
  };

  // Create a bitmap that is an inverted image of the passed in image.
  // Each color becomes its inverse in the color wheel. So (255, 15, 0) becomes
  // (0, 240, 255). The alpha value is not inverted.
  static SkBitmap CreateInvertedBitmap(const SkBitmap& image);

  // Create a bitmap that is a blend of two others. The alpha argument
  // specifies the opacity of the second bitmap. The provided bitmaps must
  // use have the kARGB_8888_Config config and be of equal dimensions.
  static SkBitmap CreateBlendedBitmap(const SkBitmap& first,
                                      const SkBitmap& second,
                                      double alpha);

  // Create a bitmap that is the original bitmap masked out by the mask defined
  // in the alpha bitmap. The images must use the kARGB_8888_Config config and
  // be of equal dimensions.
  static SkBitmap CreateMaskedBitmap(const SkBitmap& first,
                                     const SkBitmap& alpha);

  // We create a button background image by compositing the color and image
  // together, then applying the mask. This is a highly specialized composite
  // operation that is the equivalent of drawing a background in |color|,
  // tiling |image| over the top, and then masking the result out with |mask|.
  // The images must use kARGB_8888_Config config.
  static SkBitmap CreateButtonBackground(SkColor color,
                                         const SkBitmap& image,
                                         const SkBitmap& mask);

  // Shift a bitmap's HSL values. The shift values are in the range of 0-1,
  // with the option to specify -1 for 'no change'. The shift values are
  // defined as:
  // hsl_shift[0] (hue): The absolute hue value for the image - 0 and 1 map
  //    to 0 and 360 on the hue color wheel (red).
  // hsl_shift[1] (saturation): A saturation shift for the image, with the
  //    following key values:
  //    0 = remove all color.
  //    0.5 = leave unchanged.
  //    1 = fully saturate the image.
  // hsl_shift[2] (lightness): A lightness shift for the image, with the
  //    following key values:
  //    0 = remove all lightness (make all pixels black).
  //    0.5 = leave unchanged.
  //    1 = full lightness (make all pixels white).
  static SkBitmap CreateHSLShiftedBitmap(const SkBitmap& bitmap,
                                         const color_utils::HSL& hsl_shift);

  // Create a bitmap that is cropped from another bitmap. This is special
  // because it tiles the original bitmap, so your coordinates can extend
  // outside the bounds of the original image.
  static SkBitmap CreateTiledBitmap(const SkBitmap& bitmap,
                                    int src_x, int src_y,
                                    int dst_w, int dst_h);

  // Iteratively downsamples by 2 until the bitmap is no smaller than the
  // input size. The normal use of this is to downsample the bitmap "close" to
  // the final size, and then use traditional resampling on the result.
  // Because the bitmap will be closer to the final size, it will be faster,
  // and linear interpolation will generally work well as a second step.
  static SkBitmap DownsampleByTwoUntilSize(const SkBitmap& bitmap,
                                           int min_w, int min_h);

  // Makes a bitmap half has large in each direction by averaging groups of
  // 4 pixels. This is one step in generating a mipmap.
  static SkBitmap DownsampleByTwo(const SkBitmap& bitmap);

  // Unpremultiplies all pixels in |bitmap|. You almost never want to call
  // this, as |SkBitmap|s are always premultiplied by conversion. Call this
  // only if you will pass the bitmap's data into a system function that
  // doesn't expect premultiplied colors.
  static SkBitmap UnPreMultiply(const SkBitmap& bitmap);

  // Transpose the pixels in |bitmap| by swapping x and y.
  static SkBitmap CreateTransposedBitmap(const SkBitmap& bitmap);

  // Create a bitmap by combining alpha channel of |bitmap| and color |c|.
  // The image must use the kARGB_8888_Config config.
  static SkBitmap CreateColorMask(const SkBitmap& bitmap, SkColor c);

  // Create a bitmap with drop shadow added to |bitmap|. |shadows| defines
  // the shadows to add. The created bitmap would be padded to have enough space
  // for shadows and have original bitmap in the center. The image must use the
  // kARGB_8888_Config config.
  static SkBitmap CreateDropShadow(const SkBitmap& bitmap,
                                   const gfx::ShadowValues& shadows);

  // Rotates the given source bitmap clockwise by the requested amount.
  static SkBitmap Rotate(const SkBitmap& source, RotationAmount rotation);

 private:
  SkBitmapOperations();  // Class for scoping only.

  FRIEND_TEST_ALL_PREFIXES(SkBitmapOperationsTest, DownsampleByTwo);
  FRIEND_TEST_ALL_PREFIXES(SkBitmapOperationsTest, DownsampleByTwoSmall);
};

#endif  // UI_GFX_SKBITMAP_OPERATIONS_H_
