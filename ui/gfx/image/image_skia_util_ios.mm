// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_skia_util_ios.h"

#import <UIKit/UIKit.h>

#include "base/apple/scoped_cftyperef.h"
#include "skia/ext/skia_utils_ios.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace gfx {

gfx::ImageSkia ImageSkiaFromUIImage(UIImage* image) {
  gfx::ImageSkia image_skia;
  const float max_scale = ui::GetScaleForMaxSupportedResourceScaleFactor();
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

  // Resizing the image can fail.
  // https://crbug.com/1184688, https://crbug.com/41495327
  if (bitmap.empty()) {
    return gfx::ImageSkiaRep();
  }
  CHECK_EQ(bitmap.colorType(), kN32_SkColorType);

  return gfx::ImageSkiaRep(bitmap, scale);
}

UIImage* UIImageFromImageSkia(const gfx::ImageSkia& image_skia) {
  const float max_scale = ui::GetScaleForMaxSupportedResourceScaleFactor();
  return UIImageFromImageSkiaRep(image_skia.GetRepresentation(max_scale));
}

UIImage* UIImageFromImageSkiaRep(const gfx::ImageSkiaRep& image_skia_rep) {
  if (image_skia_rep.is_null())
    return nil;

  float scale = image_skia_rep.scale();
  base::apple::ScopedCFTypeRef<CGColorSpaceRef> color_space(
      CGColorSpaceCreateDeviceRGB());
  return skia::SkBitmapToUIImageWithColorSpace(image_skia_rep.GetBitmap(),
                                               scale, color_space.get());
}

}  // namespace gfx
