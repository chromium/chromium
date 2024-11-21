// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_SKIA_CONVERSIONS_H_
#define UI_GFX_GEOMETRY_SKIA_CONVERSIONS_H_

#include "base/component_export.h"
#include "third_party/skia/include/core/SkM44.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/size.h"

class SkMatrix;

namespace gfx {

class AxisTransform2d;
class Point;
class PointF;
class Rect;
class RectF;
class Transform;

// Convert between Skia and gfx types.
COMPONENT_EXPORT(GEOMETRY_SKIA) SkPoint PointToSkPoint(const Point& point);
COMPONENT_EXPORT(GEOMETRY_SKIA) SkIPoint PointToSkIPoint(const Point& point);
COMPONENT_EXPORT(GEOMETRY_SKIA) Point SkIPointToPoint(const SkIPoint& point);
COMPONENT_EXPORT(GEOMETRY_SKIA) SkPoint PointFToSkPoint(const PointF& point);
COMPONENT_EXPORT(GEOMETRY_SKIA) PointF SkPointToPointF(const SkPoint& point);
COMPONENT_EXPORT(GEOMETRY_SKIA) SkRect RectToSkRect(const Rect& rect);
COMPONENT_EXPORT(GEOMETRY_SKIA) SkIRect RectToSkIRect(const Rect& rect);
COMPONENT_EXPORT(GEOMETRY_SKIA) Rect SkIRectToRect(const SkIRect& rect);
COMPONENT_EXPORT(GEOMETRY_SKIA) SkRect RectFToSkRect(const RectF& rect);
COMPONENT_EXPORT(GEOMETRY_SKIA) RectF SkRectToRectF(const SkRect& rect);
COMPONENT_EXPORT(GEOMETRY_SKIA) SkSize SizeFToSkSize(const SizeF& size);
COMPONENT_EXPORT(GEOMETRY_SKIA) SkISize SizeToSkISize(const Size& size);
COMPONENT_EXPORT(GEOMETRY_SKIA) SizeF SkSizeToSizeF(const SkSize& size);
COMPONENT_EXPORT(GEOMETRY_SKIA) Size SkISizeToSize(const SkISize& size);

COMPONENT_EXPORT(GEOMETRY_SKIA)
void QuadFToSkPoints(const QuadF& quad, SkPoint points[4]);

COMPONENT_EXPORT(GEOMETRY_SKIA)
SkMatrix AxisTransform2dToSkMatrix(const AxisTransform2d& transform);

COMPONENT_EXPORT(GEOMETRY_SKIA)
SkM44 TransformToSkM44(const Transform& tranform);
COMPONENT_EXPORT(GEOMETRY_SKIA) Transform SkM44ToTransform(const SkM44& matrix);
// TODO(crbug.com/40237414): Remove this function in favor of the other form.
COMPONENT_EXPORT(GEOMETRY_SKIA)
void TransformToFlattenedSkMatrix(const gfx::Transform& transform,
                                  SkMatrix* flattened);
COMPONENT_EXPORT(GEOMETRY_SKIA)
SkMatrix TransformToFlattenedSkMatrix(const Transform& transform);
COMPONENT_EXPORT(GEOMETRY_SKIA)
Transform SkMatrixToTransform(const SkMatrix& matrix);

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_SKIA_CONVERSIONS_H_
