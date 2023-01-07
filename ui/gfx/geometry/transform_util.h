// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_TRANSFORM_UTIL_H_
#define UI_GFX_GEOMETRY_TRANSFORM_UTIL_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/geometry_skia_export.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/transform.h"

namespace gfx {

class Rect;
class RectF;

// Returns a scale transform at |anchor| point.
GEOMETRY_SKIA_EXPORT Transform GetScaleTransform(const Point& anchor,
                                                 float scale);

// Contains the components of a factored transform. These components may be
// blended and recomposed.
struct GEOMETRY_SKIA_EXPORT DecomposedTransform {
  // The default constructor initializes the components in such a way that
  // if used with Compose below, will produce the identity transform.
  DecomposedTransform();

  SkScalar translate[3];
  SkScalar scale[3];
  SkScalar skew[3];
  SkScalar perspective[4];
  Quaternion quaternion;

  std::string ToString() const;

  // Copy and assign are allowed.
};

// Interpolates the decomposed components |to| with |from| using the
// routines described in http://www.w3.org/TR/css3-3d-transform/.
// |progress| is in the range [0, 1]. If 0 we will return |from|, if 1, we will
// return |to|.
GEOMETRY_SKIA_EXPORT DecomposedTransform
BlendDecomposedTransforms(const DecomposedTransform& to,
                          const DecomposedTransform& from,
                          double progress);

// Decomposes this transform into its translation, scale, skew, perspective,
// and rotation components following the routines detailed in this spec:
// http://www.w3.org/TR/css3-3d-transforms/.
GEOMETRY_SKIA_EXPORT bool DecomposeTransform(DecomposedTransform* out,
                                             const Transform& transform);

// Composes a transform from the given translation, scale, skew, perspective,
// and rotation components following the routines detailed in this spec:
// http://www.w3.org/TR/css3-3d-transforms/.
GEOMETRY_SKIA_EXPORT Transform
ComposeTransform(const DecomposedTransform& decomp);

GEOMETRY_SKIA_EXPORT bool SnapTransform(Transform* out,
                                        const Transform& transform,
                                        const Rect& viewport);

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

// Compute 2D scale if possible; return whether it was set.
GEOMETRY_SKIA_EXPORT absl::optional<Vector2dF>
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
