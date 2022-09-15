// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/skia_conversions.h"

#include <stddef.h>
#include <stdint.h>

#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"

namespace gfx {

SkPoint PointToSkPoint(const Point& point) {
  return SkPoint::Make(SkIntToScalar(point.x()), SkIntToScalar(point.y()));
}

SkIPoint PointToSkIPoint(const Point& point) {
  return SkIPoint::Make(point.x(), point.y());
}

Point SkIPointToPoint(const SkIPoint& point) {
  return Point(point.x(), point.y());
}

SkPoint PointFToSkPoint(const PointF& point) {
  return SkPoint::Make(SkFloatToScalar(point.x()), SkFloatToScalar(point.y()));
}

PointF SkPointToPointF(const SkPoint& point) {
  return PointF(SkScalarToFloat(point.x()), SkScalarToFloat(point.y()));
}

SkRect RectToSkRect(const Rect& rect) {
  return SkRect::MakeXYWH(SkIntToScalar(rect.x()), SkIntToScalar(rect.y()),
                          SkIntToScalar(rect.width()),
                          SkIntToScalar(rect.height()));
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
  return SkRect::MakeXYWH(SkFloatToScalar(rect.x()), SkFloatToScalar(rect.y()),
                          SkFloatToScalar(rect.width()),
                          SkFloatToScalar(rect.height()));
}

RectF SkRectToRectF(const SkRect& rect) {
  return RectF(SkScalarToFloat(rect.x()), SkScalarToFloat(rect.y()),
               SkScalarToFloat(rect.width()), SkScalarToFloat(rect.height()));
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

void QuadFToSkPoints(const QuadF& quad, SkPoint points[4]) {
  points[0] = PointFToSkPoint(quad.p1());
  points[1] = PointFToSkPoint(quad.p2());
  points[2] = PointFToSkPoint(quad.p3());
  points[3] = PointFToSkPoint(quad.p4());
}

SkM44 TransformToSkM44(const Transform& matrix) {
  return SkM44(
      matrix.rc(0, 0), matrix.rc(0, 1), matrix.rc(0, 2), matrix.rc(0, 3),
      matrix.rc(1, 0), matrix.rc(1, 1), matrix.rc(1, 2), matrix.rc(1, 3),
      matrix.rc(2, 0), matrix.rc(2, 1), matrix.rc(2, 2), matrix.rc(2, 3),
      matrix.rc(3, 0), matrix.rc(3, 1), matrix.rc(3, 2), matrix.rc(3, 3));
}

gfx::Transform SkM44ToTransform(const SkM44& matrix) {
  return Transform(
      matrix.rc(0, 0), matrix.rc(0, 1), matrix.rc(0, 2), matrix.rc(0, 3),
      matrix.rc(1, 0), matrix.rc(1, 1), matrix.rc(1, 2), matrix.rc(1, 3),
      matrix.rc(2, 0), matrix.rc(2, 1), matrix.rc(2, 2), matrix.rc(2, 3),
      matrix.rc(3, 0), matrix.rc(3, 1), matrix.rc(3, 2), matrix.rc(3, 3));
}

// TODO(crbug.com/1359528): Remove this function in favor of the other form.
void TransformToFlattenedSkMatrix(const gfx::Transform& transform,
                                  SkMatrix* flattened) {
  *flattened = TransformToFlattenedSkMatrix(transform);
}

SkMatrix TransformToFlattenedSkMatrix(const Transform& matrix) {
  // Convert from 4x4 to 3x3 by dropping row 2 (counted from 0) and column 2.
  return SkMatrix::MakeAll(matrix.rc(0, 0), matrix.rc(0, 1), matrix.rc(0, 3),
                           matrix.rc(1, 0), matrix.rc(1, 1), matrix.rc(1, 3),
                           matrix.rc(3, 0), matrix.rc(3, 1), matrix.rc(3, 3));
}

Transform SkMatrixToTransform(const SkMatrix& matrix) {
  return Transform(matrix.rc(0, 0), matrix.rc(0, 1), 0, matrix.rc(0, 2),
                   matrix.rc(1, 0), matrix.rc(1, 1), 0, matrix.rc(1, 2),  //
                   0, 0, 1, 0,                                            //
                   matrix.rc(2, 0), matrix.rc(2, 1), 0, matrix.rc(2, 2));
}

}  // namespace gfx
