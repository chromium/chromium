// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_TRANSFORM_UTIL_H_
#define UI_GFX_GEOMETRY_TRANSFORM_UTIL_H_

#include <optional>

#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/decomposed_transform.h"
#include "ui/gfx/geometry/geometry_skia_export.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/transform.h"

namespace gfx {

class RectF;

// Returns a scale transform at |anchor| point.
GEOMETRY_SKIA_EXPORT Transform GetScaleTransform(const Point& anchor,
                                                 float scale);

// Interpolates the decomposed components |to| with |from| using the
// routines described in
// https://www.w3.org/TR/css-transforms-2/#interpolation-of-decomposed-3d-matrix-values
// |progress| is in the range [0, 1]. If 0 we will return |from|, if 1, we will
// return |to|.
GEOMETRY_SKIA_EXPORT DecomposedTransform
BlendDecomposedTransforms(const DecomposedTransform& to,
                          const DecomposedTransform& from,
                          double progress);

// Accumulates the decomposed components |to| with |from| using the
// routines described in
// https://www.w3.org/TR/css-transforms-2/#combining-transform-lists
GEOMETRY_SKIA_EXPORT DecomposedTransform
AccumulateDecomposedTransforms(const DecomposedTransform& to,
                               const DecomposedTransform& from);

// Calculates a transform with a transformed origin. The resulting transform is
// created by composing P * T * P^-1 where P is a constant transform to the new
// origin.
GEOMETRY_SKIA_EXPORT Transform TransformAboutPivot(const PointF& pivot,
                                                   const Transform& transform);

// Calculates a transform which would transform |src| to |dst|.
GEOMETRY_SKIA_EXPORT Transform TransformBetweenRects(const RectF& src,
                                                     const RectF& dst);

// Returns the 2d axis transform that maps the clipping frustum to the square
// from [-1, -1] (the original bottom-left corner) to [1, 1] (the original
// top-right corner).
GEOMETRY_SKIA_EXPORT AxisTransform2d OrthoProjectionTransform(float left,
                                                              float right,
                                                              float bottom,
                                                              float top);

// Returns the 2d axis transform that maps from ([-1, -1] .. [1, 1]) to
// ([x, y] .. [x + width, y + height]).
GEOMETRY_SKIA_EXPORT AxisTransform2d WindowTransform(int x,
                                                     int y,
                                                     int width,
                                                     int height);

// Compute 2D scale if possible, clamped with ClampFloatGeometry().
GEOMETRY_SKIA_EXPORT std::optional<Vector2dF>
TryComputeTransform2dScaleComponents(const Transform& transform);

// Compute 2D scale, and fall back to fallback_value if not possible.
GEOMETRY_SKIA_EXPORT Vector2dF
ComputeTransform2dScaleComponents(const Transform& transform,
                                  float fallback_value);

// Returns an approximate max scale value of the transform even if it has
// perspective. Prefer to use ComputeTransform2dScaleComponents if there is no
// perspective, since it can produce more accurate results.
GEOMETRY_SKIA_EXPORT
float ComputeApproximateMaxScale(const Transform& transform);

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_TRANSFORM_UTIL_H_
