// Copyright 2017 The Chromium Authors. All rights reserved.
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

#if defined(OS_IOS)
#include "base/mac/foundation_util.h"
#include "ui/gfx/image/image_skia_util_ios.h"
#elif defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#endif

namespace gfx {
namespace internal {

#if defined(OS_IOS)
scoped_refptr<base::RefCountedMemory> Get1xPNGBytesFromUIImage(
    UIImage* uiimage);
UIImage* UIImageFromPNG(const std::vector<ImagePNGRep>& image_png_reps);
gfx::Size UIImageSize(UIImage* image);
#elif defined(OS_MACOSX)
scoped_refptr<base::RefCountedMemory> Get1xPNGBytesFromNSImage(
    NSImage* nsimage);
NSImage* NSImageFromPNG(const std::vector<ImagePNGRep>& image_png_reps,
                        CGColorSpaceRef color_space);
gfx::Size NSImageSize(NSImage* image);
#endif  // defined(OS_MACOSX)

ImageSkia ImageSkiaFromPNG(const std::vector<ImagePNGRep>& image_png_reps);
scoped_refptr<base::RefCountedMemory> Get1xPNGBytesFromImageSkia(
    const ImageSkia* image_skia);

}  // namespace internal
}  // namespace gfx

#endif  // UI_GFX_IMAGE_IMAGE_PLATFORM_H_
