// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MAC_COLOR_SPACE_UTIL_H_
#define UI_GFX_MAC_COLOR_SPACE_UTIL_H_

#include <CoreFoundation/CoreFoundation.h>

#include "base/apple/scoped_cftyperef.h"
#include "ui/gfx/color_space_export.h"

namespace gfx {

class ColorSpace;

// Converts a gfx::ColorSpace to individual kCVImageBuffer keys. If
// `prefer_srgb_trfn` is true then return the sRGB transfer function for all
// Rec709-like content.
COLOR_SPACE_EXPORT bool ColorSpaceToCVImageBufferKeys(
    const gfx::ColorSpace& color_space,
    bool prefer_srgb_trfn,
    CFStringRef* out_primaries,
    CFStringRef* out_transfer,
    CFStringRef* out_matrix);

// Converts individual kCVImageBuffer keys to a gfx::ColorSpace.
COLOR_SPACE_EXPORT gfx::ColorSpace ColorSpaceFromCVImageBufferKeys(
    CFTypeRef primaries,
    CFTypeRef transfer,
    CFTypeRef gamma,
    CFTypeRef matrix);

}  // namespace gfx

#endif  // UI_GFX_MAC_COLOR_SPACE_UTIL_H_
