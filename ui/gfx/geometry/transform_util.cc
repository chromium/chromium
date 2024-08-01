// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/geometry/transform_util.h"

#include <algorithm>
#include <cmath>
#include <ostream>
#include <string>

#include "base/check.h"
#include "ui/gfx/geometry/clamp_float_geometry.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace gfx {

namespace {

template <int n>
void Combine(double* out,
             const double* a,
             const double* b,
             double scale_a,
             double scale_b) {
  for (int i = 0; i < n; ++i)
    out[i] = a[i] * scale_a + b[i] * scale_b;
}

}  // namespace

Transform GetScaleTransform(const Point& anchor, float scale) {
  Transform transform;
  transform.Translate(anchor.x() * (1 - scale), anchor.y() * (1 - scale));
  transform.Scale(scale, scale);
  return transform;
}

DecomposedTransform BlendDecomposedTransforms(const DecomposedTransform& to,
                                              const DecomposedTransform& from,
                                              double progress) {
  DecomposedTransform out;
  double scalea = progress;
  double scaleb = 1.0 - progress;
  Combine<3>(out.translate, to.translate, from.translate, scalea, scaleb);
  Combine<3>(out.scale, to.scale, from.scale, scalea, scaleb);
  Combine<3>(out.skew, to.skew, from.skew, scalea, scaleb);
  Combine<4>(out.perspective, to.perspective, from.perspective, scalea, scaleb);
  out.quaternion = from.quaternion.Slerp(to.quaternion, progress);
  return out;
}

DecomposedTransform AccumulateDecomposedTransforms(
    const DecomposedTransform& a,
    const DecomposedTransform& b) {
  DecomposedTransform out;

  // Translate is a simple addition.
  for (size_t i = 0; i < std::size(a.translate); i++)
    out.translate[i] = a.translate[i] + b.translate[i];

  // Scale is accumulated using 1-based addition.
  for (size_t i = 0; i < std::size(a.scale); i++)
    out.scale[i] = a.scale[i] + b.scale[i] - 1;

  // Skew can be added.
  for (size_t i = 0; i < std::size(a.skew); i++)
    out.skew[i] = a.skew[i] + b.skew[i];

  // We sum the perspective components; note that w is 1-based.
  for (size_t i = 0; i < std::size(a.perspective); i++)
    out.perspective[i] = a.perspective[i] + b.perspective[i];
  out.perspective[3] -= 1;

  // To accumulate quaternions, we multiply them. This is equivalent to 'adding'
  // the rotations that they represent.
  out.quaternion = a.quaternion * b.quaternion;

  return out;
}

Transform TransformAboutPivot(const PointF& pivot, const Transform& transform) {
  Transform result;
  result.Translate(pivot.x(), pivot.y());
  result.PreConcat(transform);
  result.Translate(-pivot.x(), -pivot.y());
  return result;
}

Transform TransformBetweenRects(const RectF& src, const RectF& dst) {
  DCHECK(!src.IsEmpty());
  Transform result;
  result.Translate(dst.origin() - src.origin());
  result.Scale(dst.width() / src.width(), dst.height() / src.height());
  return result;
}

AxisTransform2d OrthoProjectionTransform(float left,
                                         float right,
                                         float bottom,
                                         float top) {
  // Use the standard formula to map the clipping frustum to the square from
  // [-1, -1] to [1, 1].
  float delta_x = right - left;
  float delta_y = top - bottom;
  if (!delta_x || !delta_y)
    return AxisTransform2d();

  return AxisTransform2d::FromScaleAndTranslation(
      Vector2dF(2.0f / delta_x, 2.0f / delta_y),
      Vector2dF(-(right + left) / delta_x, -(top + bottom) / delta_y));
}

AxisTransform2d WindowTransform(int x, int y, int width, int height) {
  // Map from ([-1, -1] to [1, 1]) -> ([x, y] to [x + width, y + height]).
  return AxisTransform2d::FromScaleAndTranslation(
      Vector2dF(width * 0.5f, height * 0.5f),
      Vector2dF(x + width * 0.5f, y + height * 0.5f));
}

static inline bool NearlyZero(double value) {
  return std::abs(value) < std::numeric_limits<double>::epsilon();
}

static inline float ScaleOnAxis(double a, double b, double c) {
  if (NearlyZero(b) && NearlyZero(c))
    return ClampFloatGeometry(std::abs(a));
  if (NearlyZero(a) && NearlyZero(c))
    return ClampFloatGeometry(std::abs(b));
  if (NearlyZero(a) && NearlyZero(b))
    return ClampFloatGeometry(std::abs(c));

  // Do the sqrt as a double to not lose precision.
  return ClampFloatGeometry(std::sqrt(a * a + b * b + c * c));
}

std::optional<Vector2dF> TryComputeTransform2dScaleComponents(
    const Transform& transform) {
  if (transform.rc(3, 0) != 0.0f || transform.rc(3, 1) != 0.0f) {
    return std::nullopt;
  }

  float w = transform.rc(3, 3);
  if (!std::isnormal(w)) {
    return std::nullopt;
  }
  float w_scale = 1.0f / w;

  // In theory, this shouldn't be using the matrix.getDouble(2, 0) and
  // .getDouble(1, 0) values; creating a large transfer from input x or
  // y (in the layer) to output z has no visible difference when the
  // transform being considered is a transform to device space, since
  // the resulting z values are ignored.  However, ignoring them here
  // might be risky because it would mean that we would have more
  // variation in the results under animation of rotateX() or rotateY(),
  // and we'd be relying more heavily on code to compute correct scales
  // during animation.  Currently some such code only considers the
  // endpoints, which would become problematic for cases like animation
  // from rotateY(-60deg) to rotateY(60deg).
  float x_scale =
      ScaleOnAxis(transform.rc(0, 0), transform.rc(1, 0), transform.rc(2, 0));
  float y_scale =
      ScaleOnAxis(transform.rc(0, 1), transform.rc(1, 1), transform.rc(2, 1));
  return Vector2dF(ClampFloatGeometry(x_scale * w_scale),
                   ClampFloatGeometry(y_scale * w_scale));
}

Vector2dF ComputeTransform2dScaleComponents(const Transform& transform,
                                            float fallback_value) {
  std::optional<Vector2dF> scale =
      TryComputeTransform2dScaleComponents(transform);
  if (scale) {
    return *scale;
  }
  return Vector2dF(fallback_value, fallback_value);
}

float ComputeApproximateMaxScale(const Transform& transform) {
  gfx::RectF unit = transform.MapRect(RectF(0.f, 0.f, 1.f, 1.f));
  return std::max(unit.width(), unit.height());
}

}  // namespace gfx
