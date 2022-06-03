// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_skia_util_ios.h"

#import <UIKit/UIKit.h>

#include "base/mac/scoped_cftyperef.h"
#include "skia/ext/skia_utils_ios.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"

namespace gfx {

gfx::ImageSkia ImageSkiaFromUIImage(UIImage* image) {
  gfx::ImageSkia image_skia;
  float max_scale = ImageSkia::GetSupportedScales().back();
  gfx::ImageSkiaRep image_skia_rep = ImageSkiaRepOfScaleFromUIImage(
      image, max_scale);
  if (!image_skia_rep.is_null())
    image_skia.AddRepresentation(image_skia_rep);
  return image_skia;
}

gfx::ImageSkiaRep ImageSkiaRepOfScaleFromUIImage(UIImage* image, float scale) {
  if (!image)
    return gfx::ImageSkiaRep();

  CGSize size = image.size;
  CGSize desired_size_for_scale =
      CGSizeMake(size.width * scale, size.height * scale);
  SkBitmap bitmap(skia::CGImageToSkBitmap(image.CGImage,
                                          desired_size_for_scale,
                                          false));
  return gfx::ImageSkiaRep(bitmap, scale);
}

UIImage* UIImageFromImageSkia(const gfx::ImageSkia& image_skia) {
  return UIImageFromImageSkiaRep(
      image_skia.GetRepresentation(ImageSkia::GetSupportedScales().back()));
}

UIImage* UIImageFromImageSkiaRep(const gfx::ImageSkiaRep& image_skia_rep) {
  if (image_skia_rep.is_null())
    return nil;

  float scale = image_skia_rep.scale();
  base::ScopedCFTypeRef<CGColorSpaceRef> color_space(
      CGColorSpaceCreateDeviceRGB());
  return skia::SkBitmapToUIImageWithColorSpace(image_skia_rep.GetBitmap(),
                                               scale, color_space);
}

}  // namespace gfx
