// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_skia_util_mac.h"

#import <AppKit/AppKit.h>
#include <stddef.h>

#include <cmath>
#include <limits>
#include <memory>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"

namespace {

// Returns NSImageRep whose pixel size most closely matches |desired_size|.
NSImageRep* GetNSImageRepWithPixelSize(NSImage* image,
                                       NSSize desired_size) {
  float smallest_diff = std::numeric_limits<float>::max();
  NSImageRep* closest_match = nil;
  for (NSImageRep* image_rep in [image representations]) {
    float diff = std::abs(desired_size.width - [image_rep pixelsWide]) +
        std::abs(desired_size.height - [image_rep pixelsHigh]);
    if (diff < smallest_diff) {
      smallest_diff = diff;
      closest_match = image_rep;
    }
  }
  return closest_match;
}

// Returns true if NSImage has no representations
bool IsNSImageEmpty(NSImage* image) {
  return ([image representations].count == 0);
}

}  // namespace

namespace gfx {

gfx::ImageSkia ImageSkiaFromNSImage(NSImage* image) {
  return ImageSkiaFromResizedNSImage(image, [image size]);
}

gfx::ImageSkia ImageSkiaFromResizedNSImage(NSImage* image,
                                           NSSize desired_size) {
  // Resize and convert to ImageSkia simultaneously to save on computation.
  // TODO(pkotwicz): Separate resizing NSImage and converting to ImageSkia.
  // Convert to ImageSkia by finding the most appropriate NSImageRep for
  // each supported scale factor and resizing if necessary.

  if (IsNSImageEmpty(image))
    return gfx::ImageSkia();

  std::vector<float> supported_scales = ImageSkia::GetSupportedScales();

  gfx::ImageSkia image_skia;
  for (size_t i = 0; i < supported_scales.size(); ++i) {
    float scale = supported_scales[i];
    NSSize desired_size_for_scale = NSMakeSize(desired_size.width * scale,
                                               desired_size.height * scale);
    NSImageRep* ns_image_rep = GetNSImageRepWithPixelSize(image,
        desired_size_for_scale);

    // TODO(dcheng): Should this function take a color space argument?
    SkBitmap bitmap(skia::NSImageRepToSkBitmapWithColorSpace(ns_image_rep,
        desired_size_for_scale, false, base::mac::GetGenericRGBColorSpace()));
    if (bitmap.isNull())
      continue;

    image_skia.AddRepresentation(gfx::ImageSkiaRep(bitmap, scale));
  }
  return image_skia;
}

NSImage* NSImageFromImageSkia(const gfx::ImageSkia& image_skia) {
  if (image_skia.isNull())
    return nil;

  base::scoped_nsobject<NSImage> image([[NSImage alloc] init]);
  image_skia.EnsureRepsForSupportedScales();
  std::vector<gfx::ImageSkiaRep> image_reps = image_skia.image_reps();
  for (std::vector<gfx::ImageSkiaRep>::const_iterator it = image_reps.begin();
       it != image_reps.end(); ++it) {
    [image addRepresentation:skia::SkBitmapToNSBitmapImageRep(it->GetBitmap())];
  }

  [image setSize:NSMakeSize(image_skia.width(), image_skia.height())];
  return [image.release() autorelease];
}

NSImage* NSImageFromImageSkiaWithColorSpace(const gfx::ImageSkia& image_skia,
                                            CGColorSpaceRef color_space) {
  if (image_skia.isNull())
    return nil;

  base::scoped_nsobject<NSImage> image([[NSImage alloc] init]);
  image_skia.EnsureRepsForSupportedScales();
  std::vector<gfx::ImageSkiaRep> image_reps = image_skia.image_reps();
  for (std::vector<gfx::ImageSkiaRep>::const_iterator it = image_reps.begin();
       it != image_reps.end(); ++it) {
    [image addRepresentation:skia::SkBitmapToNSBitmapImageRepWithColorSpace(
                                 it->GetBitmap(), color_space)];
  }

  [image setSize:NSMakeSize(image_skia.width(), image_skia.height())];
  return [image.release() autorelease];
}

}  // namespace gfx
