// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IMAGE_IMAGE_SKIA_OPERATIONS_H_
#define UI_GFX_IMAGE_IMAGE_SKIA_OPERATIONS_H_

#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkDrawLooper.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skbitmap_operations.h"

namespace gfx {
class ImageSkia;
class Rect;
class Size;

class GFX_EXPORT ImageSkiaOperations {
 public:
  // Create an image that is a blend of two others. The alpha argument
  // specifies the opacity of the second imag. The provided image must
  // use the kARGB_8888_Config config and be of equal dimensions.
  static ImageSkia CreateBlendedImage(const ImageSkia& first,
                                      const ImageSkia& second,
                                      double alpha);

  // Creates an image that is the original image with opacity set to |alpha|.
  static ImageSkia CreateTransparentImage(const ImageSkia& image, double alpha);

  // Creates new image by painting first and second image respectively.
  // The second image is centered in respect to the first image.
  static ImageSkia CreateSuperimposedImage(const ImageSkia& first,
                                           const ImageSkia& second);

  // Create an image that is the original image masked out by the mask defined
  // in the alpha image. The images must use the kARGB_8888_Config config and
  // be of equal dimensions.
  static ImageSkia CreateMaskedImage(const ImageSkia& first,
                                     const ImageSkia& alpha);

  // Create an image that is cropped from another image. This is special
  // because it tiles the original image, so your coordinates can extend
  // outside the bounds of the original image.
  static ImageSkia CreateTiledImage(const ImageSkia& image,
                                    int src_x, int src_y,
                                    int dst_w, int dst_h);

  // Shift an image's HSL values. The shift values are in the range of 0-1,
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
  static ImageSkia CreateHSLShiftedImage(const gfx::ImageSkia& image,
                                         const color_utils::HSL& hsl_shift);

  // Creates a button background image by compositing the color and image
  // together, then applying the mask. This is a highly specialized composite
  // operation that is the equivalent of drawing a background in |color|,
  // tiling |image| over the top, and then masking the result out with |mask|.
  // The images must use kARGB_8888_Config config.
  static ImageSkia CreateButtonBackground(SkColor color,
                                          const gfx::ImageSkia& image,
                                          const gfx::ImageSkia& mask);

  // Returns an image which is a subset of |image| with bounds |subset_bounds|.
  // The |image| cannot use kA1_Config config.
  static ImageSkia ExtractSubset(const gfx::ImageSkia& image,
                                 const gfx::Rect& subset_bounds);

  // Creates an image by resizing |source| to given |target_dip_size|.
  static ImageSkia CreateResizedImage(const ImageSkia& source,
                                      skia::ImageOperations::ResizeMethod methd,
                                      const Size& target_dip_size);

  // Creates an image with drop shadow defined in |shadows| for |source|.
  static ImageSkia CreateImageWithDropShadow(const ImageSkia& source,
                                             const ShadowValues& shadows);

  // Creates an image that is 1dp wide, suitable for tiling horizontally to
  // create a drop shadow effect. The purpose of tiling a static image is to
  // avoid repeatedly asking Skia to draw a shadow.
  static ImageSkia CreateHorizontalShadow(
      const std::vector<ShadowValue>& shadows,
      bool fades_down);

  // Creates an image which is a rotation of the |source|. |rotation| is the
  // amount of clockwise rotation in degrees.
  static ImageSkia CreateRotatedImage(
      const ImageSkia& source,
      SkBitmapOperations::RotationAmount rotation);

  // Creates an icon by painting the second icon as a badge to the first one.
  // The second icon is in the right corner of the first icon. If the icon
  // is valid and the badge is not, the icon will be returned.
  static ImageSkia CreateIconWithBadge(const ImageSkia& icon,
                                       const ImageSkia& badge);

  // Creates an image by combining |image| and color |color|.
  // The image must use the kARGB_8888_Config config.
  static ImageSkia CreateColorMask(const gfx::ImageSkia& image, SkColor color);

 private:
  ImageSkiaOperations();  // Class for scoping only.
};

}  // namespace gfx

#endif  // UI_GFX_IMAGE_IMAGE_SKIA_OPERATIONS_H_
