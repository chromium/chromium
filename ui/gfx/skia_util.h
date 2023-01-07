// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SKIA_UTIL_H_
#define UI_GFX_SKIA_UTIL_H_

#include "third_party/skia/include/core/SkScalar.h"
#include "ui/gfx/gfx_skia_export.h"

class SkBitmap;

namespace gfx {

// Returns true if the two bitmaps contain the same pixels.
GFX_SKIA_EXPORT bool BitmapsAreEqual(const SkBitmap& bitmap1,
                                     const SkBitmap& bitmap2);

// Converts a Skia floating-point value to an int appropriate for hb_position_t.
GFX_SKIA_EXPORT int SkiaScalarToHarfBuzzUnits(SkScalar value);

// Converts an hb_position_t to a Skia floating-point value.
GFX_SKIA_EXPORT SkScalar HarfBuzzUnitsToSkiaScalar(int value);

// Converts an hb_position_t to a float.
GFX_SKIA_EXPORT float HarfBuzzUnitsToFloat(int value);

}  // namespace gfx

#endif  // UI_GFX_SKIA_UTIL_H_
