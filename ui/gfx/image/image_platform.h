// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file declares platform-specific helper functions in gfx::internal for
// use by image.cc.
//
// The functions are implemented in image_generic.cc (all platforms other than
// iOS), image_ios.mm and image_mac.mm.

#ifndef UI_GFX_IMAGE_IMAGE_PLATFORM_H_
#define UI_GFX_IMAGE_IMAGE_PLATFORM_H_

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_png_rep.h"
#include "ui/gfx/image/image_skia.h"

#if BUILDFLAG(IS_IOS)
#include "base/apple/foundation_util.h"
#include "ui/gfx/image/image_skia_util_ios.h"
#elif BUILDFLAG(IS_MAC)
#include "base/apple/foundation_util.h"
#include "base/mac/mac_util.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#endif

namespace gfx::internal {

class ImageRep;
class ImageRepCocoa;
class ImageRepCocoaTouch;

#if BUILDFLAG(IS_IOS)
scoped_refptr<base::RefCountedMemory> Get1xPNGBytesFromUIImage(
    UIImage* uiimage);
UIImage* UIImageFromPNG(const std::vector<ImagePNGRep>& image_png_reps);

UIImage* UIImageOfImageRepCocoaTouch(const ImageRepCocoaTouch* image_rep);
std::unique_ptr<ImageRep> MakeImageRepCocoaTouch(UIImage* image);
#elif BUILDFLAG(IS_MAC)
scoped_refptr<base::RefCountedMemory> Get1xPNGBytesFromNSImage(
    NSImage* nsimage);

NSImage* NSImageFromPNG(const std::vector<ImagePNGRep>& image_png_reps);

// TODO(crbug.com/40286491): Remove callers to this function.
inline NSImage* NSImageFromPNG(const std::vector<ImagePNGRep>& image_png_reps,
                               CGColorSpaceRef color_space) {
  return NSImageFromPNG(image_png_reps);
}

NSImage* NSImageOfImageRepCocoa(const ImageRepCocoa* image_rep);
std::unique_ptr<ImageRep> MakeImageRepCocoa(NSImage* image);
#endif

ImageSkia ImageSkiaFromPNG(const std::vector<ImagePNGRep>& image_png_reps);
scoped_refptr<base::RefCountedMemory> Get1xPNGBytesFromImageSkia(
    const ImageSkia* image_skia);

}  // namespace gfx::internal

#endif  // UI_GFX_IMAGE_IMAGE_PLATFORM_H_
