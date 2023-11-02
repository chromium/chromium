// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/skia_util.h"

#include "base/check.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace gfx {

bool BitmapsAreEqual(const SkBitmap& bitmap1, const SkBitmap& bitmap2) {
  if (bitmap1.isNull() != bitmap2.isNull() ||
      bitmap1.dimensions() != bitmap2.dimensions())
    return false;

  if (bitmap1.getGenerationID() == bitmap2.getGenerationID() ||
      (bitmap1.empty() && bitmap2.empty()))
    return true;

  // Calling getAddr32() on null or empty bitmaps will assert. The conditions
  // above should return early if either bitmap is empty or null.
  DCHECK(!bitmap1.isNull() && !bitmap2.isNull());
  DCHECK(!bitmap1.empty() && !bitmap2.empty());

  void* addr1 = bitmap1.getAddr32(0, 0);
  void* addr2 = bitmap2.getAddr32(0, 0);
  size_t size1 = bitmap1.computeByteSize();
  size_t size2 = bitmap2.computeByteSize();

  return (size1 == size2) && (0 == memcmp(addr1, addr2, size1));
}

// We treat HarfBuzz ints as 16.16 fixed-point.
static const int kHbUnit1 = 1 << 16;

int SkiaScalarToHarfBuzzUnits(SkScalar value) {
  return base::saturated_cast<int>(value * kHbUnit1);
}

SkScalar HarfBuzzUnitsToSkiaScalar(int value) {
  static const SkScalar kSkToHbRatio = SK_Scalar1 / kHbUnit1;
  return kSkToHbRatio * value;
}

float HarfBuzzUnitsToFloat(int value) {
  static const float kFloatToHbRatio = 1.0f / kHbUnit1;
  return kFloatToHbRatio * value;
}

}  // namespace gfx
