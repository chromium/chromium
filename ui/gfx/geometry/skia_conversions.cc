// Copyright 2021 The Chromium Authors. All rights reserved.
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

void TransformToFlattenedSkMatrix(const gfx::Transform& transform,
                                  SkMatrix* flattened) {
  // Convert from 4x4 to 3x3 by dropping the third row and column.
  flattened->set(0, transform.matrix().rc(0, 0));
  flattened->set(1, transform.matrix().rc(0, 1));
  flattened->set(2, transform.matrix().rc(0, 3));
  flattened->set(3, transform.matrix().rc(1, 0));
  flattened->set(4, transform.matrix().rc(1, 1));
  flattened->set(5, transform.matrix().rc(1, 3));
  flattened->set(6, transform.matrix().rc(3, 0));
  flattened->set(7, transform.matrix().rc(3, 1));
  flattened->set(8, transform.matrix().rc(3, 3));
}

void QuadFToSkPoints(const gfx::QuadF& quad, SkPoint points[4]) {
  points[0] = PointFToSkPoint(quad.p1());
  points[1] = PointFToSkPoint(quad.p2());
  points[2] = PointFToSkPoint(quad.p3());
  points[3] = PointFToSkPoint(quad.p4());
}

}  // namespace gfx
