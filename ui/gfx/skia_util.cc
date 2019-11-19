// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/skia_util.h"

#include <stddef.h>
#include <stdint.h>

#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkUnPreMultiply.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "third_party/skia/include/effects/SkLayerDrawLooper.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/transform.h"

namespace gfx {

SkPoint PointToSkPoint(const Point& point) {
  return SkPoint::Make(SkIntToScalar(point.x()), SkIntToScalar(point.y()));
}

SkIPoint PointToSkIPoint(const Point& point) {
  return SkIPoint::Make(point.x(), point.y());
}

SkPoint PointFToSkPoint(const PointF& point) {
  return SkPoint::Make(SkFloatToScalar(point.x()), SkFloatToScalar(point.y()));
}

SkRect RectToSkRect(const Rect& rect) {
  return SkRect::MakeXYWH(
      SkIntToScalar(rect.x()), SkIntToScalar(rect.y()),
      SkIntToScalar(rect.width()), SkIntToScalar(rect.height()));
}

SkIRect RectToSkIRect(const Rect& rect) {
  return SkIRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height());
}

Rect SkIRectToRect(const SkIRect& rect) {
  Rect result;
  result.SetByBounds(rect.left(), rect.top(), rect.right(), rect.bottom());
  return result;
}

SkRect RectFToSkRect(const RectF& rect) {
  return SkRect::MakeXYWH(SkFloatToScalar(rect.x()),
                          SkFloatToScalar(rect.y()),
                          SkFloatToScalar(rect.width()),
                          SkFloatToScalar(rect.height()));
}

RectF SkRectToRectF(const SkRect& rect) {
  return RectF(SkScalarToFloat(rect.x()),
               SkScalarToFloat(rect.y()),
               SkScalarToFloat(rect.width()),
               SkScalarToFloat(rect.height()));
}

SkSize SizeFToSkSize(const SizeF& size) {
  return SkSize::Make(SkFloatToScalar(size.width()),
                      SkFloatToScalar(size.height()));
}

SkISize SizeToSkISize(const Size& size) {
  return SkISize::Make(size.width(), size.height());
}

SizeF SkSizeToSizeF(const SkSize& size) {
  return SizeF(SkScalarToFloat(size.width()), SkScalarToFloat(size.height()));
}

Size SkISizeToSize(const SkISize& size) {
  return Size(size.width(), size.height());
}

void TransformToFlattenedSkMatrix(const gfx::Transform& transform,
                                  SkMatrix* flattened) {
  // Convert from 4x4 to 3x3 by dropping the third row and column.
  flattened->set(0, SkMScalarToScalar(transform.matrix().get(0, 0)));
  flattened->set(1, SkMScalarToScalar(transform.matrix().get(0, 1)));
  flattened->set(2, SkMScalarToScalar(transform.matrix().get(0, 3)));
  flattened->set(3, SkMScalarToScalar(transform.matrix().get(1, 0)));
  flattened->set(4, SkMScalarToScalar(transform.matrix().get(1, 1)));
  flattened->set(5, SkMScalarToScalar(transform.matrix().get(1, 3)));
  flattened->set(6, SkMScalarToScalar(transform.matrix().get(3, 0)));
  flattened->set(7, SkMScalarToScalar(transform.matrix().get(3, 1)));
  flattened->set(8, SkMScalarToScalar(transform.matrix().get(3, 3)));
}

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

void ConvertSkiaToRGBA(const unsigned char* skia,
                       int pixel_width,
                       unsigned char* rgba) {
  int total_length = pixel_width * 4;
  for (int i = 0; i < total_length; i += 4) {
    const uint32_t pixel_in = *reinterpret_cast<const uint32_t*>(&skia[i]);

    // Pack the components here.
    SkAlpha alpha = SkGetPackedA32(pixel_in);
    if (alpha != 0 && alpha != 255) {
      SkColor unmultiplied = SkUnPreMultiply::PMColorToColor(pixel_in);
      rgba[i + 0] = SkColorGetR(unmultiplied);
      rgba[i + 1] = SkColorGetG(unmultiplied);
      rgba[i + 2] = SkColorGetB(unmultiplied);
      rgba[i + 3] = alpha;
    } else {
      rgba[i + 0] = SkGetPackedR32(pixel_in);
      rgba[i + 1] = SkGetPackedG32(pixel_in);
      rgba[i + 2] = SkGetPackedB32(pixel_in);
      rgba[i + 3] = alpha;
    }
  }
}

void QuadFToSkPoints(const gfx::QuadF& quad, SkPoint points[4]) {
  points[0] = PointFToSkPoint(quad.p1());
  points[1] = PointFToSkPoint(quad.p2());
  points[2] = PointFToSkPoint(quad.p3());
  points[3] = PointFToSkPoint(quad.p4());
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
