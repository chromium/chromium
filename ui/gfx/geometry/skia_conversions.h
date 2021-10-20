// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_SKIA_CONVERSIONS_H_
#define UI_GFX_GEOMETRY_SKIA_CONVERSIONS_H_

#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/geometry/geometry_skia_export.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/size.h"

class SkMatrix;

namespace gfx {

class Point;
class PointF;
class Rect;
class RectF;
class Transform;

// Convert between Skia and gfx types.
GEOMETRY_SKIA_EXPORT SkPoint PointToSkPoint(const Point& point);
GEOMETRY_SKIA_EXPORT SkIPoint PointToSkIPoint(const Point& point);
GEOMETRY_SKIA_EXPORT Point SkIPointToPoint(const SkIPoint& point);
GEOMETRY_SKIA_EXPORT SkPoint PointFToSkPoint(const PointF& point);
GEOMETRY_SKIA_EXPORT PointF SkPointToPointF(const SkPoint& point);
GEOMETRY_SKIA_EXPORT SkRect RectToSkRect(const Rect& rect);
GEOMETRY_SKIA_EXPORT SkIRect RectToSkIRect(const Rect& rect);
GEOMETRY_SKIA_EXPORT Rect SkIRectToRect(const SkIRect& rect);
GEOMETRY_SKIA_EXPORT SkRect RectFToSkRect(const RectF& rect);
GEOMETRY_SKIA_EXPORT RectF SkRectToRectF(const SkRect& rect);
GEOMETRY_SKIA_EXPORT SkSize SizeFToSkSize(const SizeF& size);
GEOMETRY_SKIA_EXPORT SkISize SizeToSkISize(const Size& size);
GEOMETRY_SKIA_EXPORT SizeF SkSizeToSizeF(const SkSize& size);
GEOMETRY_SKIA_EXPORT Size SkISizeToSize(const SkISize& size);

GEOMETRY_SKIA_EXPORT void QuadFToSkPoints(const gfx::QuadF& quad,
                                          SkPoint points[4]);

GEOMETRY_SKIA_EXPORT void TransformToFlattenedSkMatrix(
    const gfx::Transform& transform,
    SkMatrix* flattened);

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_SKIA_CONVERSIONS_H_
