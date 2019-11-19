// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/transform_util.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace gfx {

namespace {

SkMScalar Length3(SkMScalar v[3]) {
  double vd[3] = {SkMScalarToDouble(v[0]), SkMScalarToDouble(v[1]),
                  SkMScalarToDouble(v[2])};
  return SkDoubleToMScalar(
      std::sqrt(vd[0] * vd[0] + vd[1] * vd[1] + vd[2] * vd[2]));
}

template <int n>
SkMScalar Dot(const SkMScalar* a, const SkMScalar* b) {
  double total = 0.0;
  for (int i = 0; i < n; ++i)
    total += a[i] * b[i];
  return SkDoubleToMScalar(total);
}

template <int n>
void Combine(SkMScalar* out,
             const SkMScalar* a,
             const SkMScalar* b,
             double scale_a,
             double scale_b) {
  for (int i = 0; i < n; ++i)
    out[i] = SkDoubleToMScalar(a[i] * scale_a + b[i] * scale_b);
}

void Cross3(SkMScalar out[3], SkMScalar a[3], SkMScalar b[3]) {
  SkMScalar x = a[1] * b[2] - a[2] * b[1];
  SkMScalar y = a[2] * b[0] - a[0] * b[2];
  SkMScalar z = a[0] * b[1] - a[1] * b[0];
  out[0] = x;
  out[1] = y;
  out[2] = z;
}

SkMScalar Round(SkMScalar n) {
  return SkDoubleToMScalar(std::floor(SkMScalarToDouble(n) + 0.5));
}

// Returns false if the matrix cannot be normalized.
bool Normalize(SkMatrix44& m) {
  if (m.get(3, 3) == 0.0)
    // Cannot normalize.
    return false;

  SkMScalar scale = SK_MScalar1 / m.get(3, 3);
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      m.set(i, j, m.get(i, j) * scale);

  return true;
}

SkMatrix44 BuildPerspectiveMatrix(const DecomposedTransform& decomp) {
  SkMatrix44 matrix(SkMatrix44::kIdentity_Constructor);

  for (int i = 0; i < 4; i++)
    matrix.setDouble(3, i, decomp.perspective[i]);
  return matrix;
}

SkMatrix44 BuildTranslationMatrix(const DecomposedTransform& decomp) {
  SkMatrix44 matrix(SkMatrix44::kUninitialized_Constructor);
  // Implicitly calls matrix.setIdentity()
  matrix.setTranslate(SkDoubleToMScalar(decomp.translate[0]),
                      SkDoubleToMScalar(decomp.translate[1]),
                      SkDoubleToMScalar(decomp.translate[2]));
  return matrix;
}

SkMatrix44 BuildSnappedTranslationMatrix(DecomposedTransform decomp) {
  decomp.translate[0] = Round(decomp.translate[0]);
  decomp.translate[1] = Round(decomp.translate[1]);
  decomp.translate[2] = Round(decomp.translate[2]);
  return BuildTranslationMatrix(decomp);
}

SkMatrix44 BuildRotationMatrix(const DecomposedTransform& decomp) {
  return Transform(decomp.quaternion).matrix();
}

SkMatrix44 BuildSnappedRotationMatrix(const DecomposedTransform& decomp) {
  // Create snapped rotation.
  SkMatrix44 rotation_matrix = BuildRotationMatrix(decomp);
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      SkMScalar value = rotation_matrix.get(i, j);
      // Snap values to -1, 0 or 1.
      if (value < -0.5f) {
        value = -1.0f;
      } else if (value > 0.5f) {
        value = 1.0f;
      } else {
        value = 0.0f;
      }
      rotation_matrix.set(i, j, value);
    }
  }
  return rotation_matrix;
}

SkMatrix44 BuildSkewMatrix(const DecomposedTransform& decomp) {
  SkMatrix44 matrix(SkMatrix44::kIdentity_Constructor);

  SkMatrix44 temp(SkMatrix44::kIdentity_Constructor);
  if (decomp.skew[2]) {
    temp.setDouble(1, 2, decomp.skew[2]);
    matrix.preConcat(temp);
  }

  if (decomp.skew[1]) {
    temp.setDouble(1, 2, 0);
    temp.setDouble(0, 2, decomp.skew[1]);
    matrix.preConcat(temp);
  }

  if (decomp.skew[0]) {
    temp.setDouble(0, 2, 0);
    temp.setDouble(0, 1, decomp.skew[0]);
    matrix.preConcat(temp);
  }
  return matrix;
}

SkMatrix44 BuildScaleMatrix(const DecomposedTransform& decomp) {
  SkMatrix44 matrix(SkMatrix44::kUninitialized_Constructor);
  matrix.setScale(SkDoubleToMScalar(decomp.scale[0]),
                  SkDoubleToMScalar(decomp.scale[1]),
                  SkDoubleToMScalar(decomp.scale[2]));
  return matrix;
}

SkMatrix44 BuildSnappedScaleMatrix(DecomposedTransform decomp) {
  decomp.scale[0] = Round(decomp.scale[0]);
  decomp.scale[1] = Round(decomp.scale[1]);
  decomp.scale[2] = Round(decomp.scale[2]);
  return BuildScaleMatrix(decomp);
}

Transform ComposeTransform(const SkMatrix44& perspective,
                           const SkMatrix44& translation,
                           const SkMatrix44& rotation,
                           const SkMatrix44& skew,
                           const SkMatrix44& scale) {
  SkMatrix44 matrix(SkMatrix44::kIdentity_Constructor);

  matrix.preConcat(perspective);
  matrix.preConcat(translation);
  matrix.preConcat(rotation);
  matrix.preConcat(skew);
  matrix.preConcat(scale);

  Transform to_return;
  to_return.matrix() = matrix;
  return to_return;
}

bool CheckViewportPointMapsWithinOnePixel(const Point& point,
                                          const Transform& transform) {
  auto point_original = Point3F(PointF(point));
  auto point_transformed = Point3F(PointF(point));

  // Can't use TransformRect here since it would give us the axis-aligned
  // bounding rect of the 4 points in the initial rectable which is not what we
  // want.
  transform.TransformPoint(&point_transformed);

  if ((point_transformed - point_original).Length() > 1.f) {
    // The changed distance should not be more than 1 pixel.
    return false;
  }
  return true;
}

bool CheckTransformsMapsIntViewportWithinOnePixel(const Rect& viewport,
                                                  const Transform& original,
                                                  const Transform& snapped) {
  Transform original_inv(Transform::kSkipInitialization);
  bool invertible = true;
  invertible &= original.GetInverse(&original_inv);
  DCHECK(invertible) << "Non-invertible transform, cannot snap.";

  Transform combined = snapped * original_inv;

  return CheckViewportPointMapsWithinOnePixel(viewport.origin(), combined) &&
         CheckViewportPointMapsWithinOnePixel(viewport.top_right(), combined) &&
         CheckViewportPointMapsWithinOnePixel(viewport.bottom_left(),
                                              combined) &&
         CheckViewportPointMapsWithinOnePixel(viewport.bottom_right(),
                                              combined);
}

bool Is2dTransform(const Transform& transform) {
  const SkMatrix44 matrix = transform.matrix();
  if (matrix.hasPerspective())
    return false;

  return matrix.get(2, 0) == 0 && matrix.get(2, 1) == 0 &&
         matrix.get(0, 2) == 0 && matrix.get(1, 2) == 0 &&
         matrix.get(2, 2) == 1 && matrix.get(3, 2) == 0 &&
         matrix.get(2, 3) == 0;
}

bool Decompose2DTransform(DecomposedTransform* decomp,
                          const Transform& transform) {
  if (!Is2dTransform(transform)) {
    return false;
  }

  const SkMatrix44 matrix = transform.matrix();
  double m11 = matrix.getDouble(0, 0);
  double m21 = matrix.getDouble(0, 1);
  double m12 = matrix.getDouble(1, 0);
  double m22 = matrix.getDouble(1, 1);

  double determinant = m11 * m22 - m12 * m21;
  // Test for matrix being singular.
  if (determinant == 0) {
    return false;
  }

  // Translation transform.
  // [m11 m21 0 m41]    [1 0 0 Tx] [m11 m21 0 0]
  // [m12 m22 0 m42]  = [0 1 0 Ty] [m12 m22 0 0]
  // [ 0   0  1  0 ]    [0 0 1 0 ] [ 0   0  1 0]
  // [ 0   0  0  1 ]    [0 0 0 1 ] [ 0   0  0 1]
  decomp->translate[0] = matrix.get(0, 3);
  decomp->translate[1] = matrix.get(1, 3);

  // For the remainder of the decomposition process, we can focus on the upper
  // 2x2 submatrix
  // [m11 m21] = [cos(R) -sin(R)] [1 K] [Sx 0 ]
  // [m12 m22]   [sin(R)  cos(R)] [0 1] [0  Sy]
  //           = [Sx*cos(R) Sy*(K*cos(R) - sin(R))]
  //             [Sx*sin(R) Sy*(K*sin(R) + cos(R))]

  // Determine sign of the x and y scale.
  if (determinant < 0) {
    // If the determinant is negative, we need to flip either the x or y scale.
    // Flipping both is equivalent to rotating by 180 degrees.
    if (m11 < m22) {
      decomp->scale[0] *= -1;
    } else {
      decomp->scale[1] *= -1;
    }
  }

  // X Scale.
  // m11^2 + m12^2 = Sx^2*(cos^2(R) + sin^2(R)) = Sx^2.
  // Sx = +/-sqrt(m11^2 + m22^2)
  decomp->scale[0] *= sqrt(m11 * m11 + m12 * m12);
  m11 /= decomp->scale[0];
  m12 /= decomp->scale[0];

  // Post normalization, the submatrix is now of the form:
  // [m11 m21] = [cos(R)  Sy*(K*cos(R) - sin(R))]
  // [m12 m22]   [sin(R)  Sy*(K*sin(R) + cos(R))]

  // XY Shear.
  // m11 * m21 + m12 * m22 = Sy*K*cos^2(R) - Sy*sin(R)*cos(R) +
  //                         Sy*K*sin^2(R) + Sy*cos(R)*sin(R)
  //                       = Sy*K
  double scaledShear = m11 * m21 + m12 * m22;
  m21 -= m11 * scaledShear;
  m22 -= m12 * scaledShear;

  // Post normalization, the submatrix is now of the form:
  // [m11 m21] = [cos(R)  -Sy*sin(R)]
  // [m12 m22]   [sin(R)   Sy*cos(R)]

  // Y Scale.
  // Similar process to determining x-scale.
  decomp->scale[1] *= sqrt(m21 * m21 + m22 * m22);
  m21 /= decomp->scale[1];
  m22 /= decomp->scale[1];
  decomp->skew[0] = scaledShear / decomp->scale[1];

  // Rotation transform.
  // [1-2(yy+zz)  2(xy-zw)    2(xz+yw) ]   [cos(R) -sin(R)  0]
  // [2(xy+zw)   1-2(xx+zz)   2(yz-xw) ] = [sin(R)  cos(R)  0]
  // [2(xz-yw)    2*(yz+xw)  1-2(xx+yy)]   [  0       0     1]
  // Comparing terms, we can conclude that x = y = 0.
  // [1-2zz   -2zw  0]   [cos(R) -sin(R)  0]
  // [ 2zw   1-2zz  0] = [sin(R)  cos(R)  0]
  // [  0     0     1]   [  0       0     1]
  // cos(R) = 1 - 2*z^2
  // From the double angle formula: cos(2a) = 1 - 2 sin(a)^2
  // cos(R) = 1 - 2*sin(R/2)^2 = 1 - 2*z^2 ==> z = sin(R/2)
  // sin(R) = 2*z*w
  // But sin(2a) = 2 sin(a) cos(a)
  // sin(R) = 2 sin(R/2) cos(R/2) = 2*z*w ==> w = cos(R/2)
  double angle = atan2(m12, m11);
  decomp->quaternion.set_x(0);
  decomp->quaternion.set_y(0);
  decomp->quaternion.set_z(sin(0.5 * angle));
  decomp->quaternion.set_w(cos(0.5 * angle));

  return true;
}

}  // namespace

Transform GetScaleTransform(const Point& anchor, float scale) {
  Transform transform;
  transform.Translate(anchor.x() * (1 - scale), anchor.y() * (1 - scale));
  transform.Scale(scale, scale);
  return transform;
}

DecomposedTransform::DecomposedTransform() {
  translate[0] = translate[1] = translate[2] = 0.0;
  scale[0] = scale[1] = scale[2] = 1.0;
  skew[0] = skew[1] = skew[2] = 0.0;
  perspective[0] = perspective[1] = perspective[2] = 0.0;
  perspective[3] = 1.0;
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

// Taken from http://www.w3.org/TR/css3-transforms/.
// TODO(crbug/937296): This implementation is virtually identical to the
// implementation in blink::TransformationMatrix with the main difference being
// the representation of the underlying matrix. These implementations should be
// consolidated.
bool DecomposeTransform(DecomposedTransform* decomp,
                        const Transform& transform) {
  if (!decomp)
    return false;

  if (Decompose2DTransform(decomp, transform))
    return true;

  // We'll operate on a copy of the matrix.
  SkMatrix44 matrix = transform.matrix();

  // If we cannot normalize the matrix, then bail early as we cannot decompose.
  if (!Normalize(matrix))
    return false;

  SkMatrix44 perspectiveMatrix = matrix;

  for (int i = 0; i < 3; ++i)
    perspectiveMatrix.set(3, i, 0.0);

  perspectiveMatrix.set(3, 3, 1.0);

  // If the perspective matrix is not invertible, we are also unable to
  // decompose, so we'll bail early. Constant taken from SkMatrix44::invert.
  if (std::abs(perspectiveMatrix.determinant()) < 1e-8)
    return false;

  if (matrix.get(3, 0) != 0.0 || matrix.get(3, 1) != 0.0 ||
      matrix.get(3, 2) != 0.0) {
    // rhs is the right hand side of the equation.
    SkMScalar rhs[4] = {matrix.get(3, 0), matrix.get(3, 1), matrix.get(3, 2),
                        matrix.get(3, 3)};

    // Solve the equation by inverting perspectiveMatrix and multiplying
    // rhs by the inverse.
    SkMatrix44 inversePerspectiveMatrix(SkMatrix44::kUninitialized_Constructor);
    if (!perspectiveMatrix.invert(&inversePerspectiveMatrix))
      return false;

    SkMatrix44 transposedInversePerspectiveMatrix = inversePerspectiveMatrix;

    transposedInversePerspectiveMatrix.transpose();
    transposedInversePerspectiveMatrix.mapMScalars(rhs);

    for (int i = 0; i < 4; ++i)
      decomp->perspective[i] = rhs[i];

  } else {
    // No perspective.
    for (int i = 0; i < 3; ++i)
      decomp->perspective[i] = 0.0;
    decomp->perspective[3] = 1.0;
  }

  for (int i = 0; i < 3; i++)
    decomp->translate[i] = matrix.get(i, 3);

  // Copy of matrix is stored in column major order to facilitate column-level
  // operations.
  SkMScalar column[3][3];
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; ++j)
      column[i][j] = matrix.get(j, i);

  // Compute X scale factor and normalize first column.
  decomp->scale[0] = Length3(column[0]);
  if (decomp->scale[0] != 0.0) {
    column[0][0] /= decomp->scale[0];
    column[0][1] /= decomp->scale[0];
    column[0][2] /= decomp->scale[0];
  }

  // Compute XY shear factor and make 2nd column orthogonal to 1st.
  decomp->skew[0] = Dot<3>(column[0], column[1]);
  Combine<3>(column[1], column[1], column[0], 1.0, -decomp->skew[0]);

  // Now, compute Y scale and normalize 2nd column.
  decomp->scale[1] = Length3(column[1]);
  if (decomp->scale[1] != 0.0) {
    column[1][0] /= decomp->scale[1];
    column[1][1] /= decomp->scale[1];
    column[1][2] /= decomp->scale[1];
  }

  decomp->skew[0] /= decomp->scale[1];

  // Compute XZ and YZ shears, orthogonalize the 3rd column.
  decomp->skew[1] = Dot<3>(column[0], column[2]);
  Combine<3>(column[2], column[2], column[0], 1.0, -decomp->skew[1]);
  decomp->skew[2] = Dot<3>(column[1], column[2]);
  Combine<3>(column[2], column[2], column[1], 1.0, -decomp->skew[2]);

  // Next, get Z scale and normalize the 3rd column.
  decomp->scale[2] = Length3(column[2]);
  if (decomp->scale[2] != 0.0) {
    column[2][0] /= decomp->scale[2];
    column[2][1] /= decomp->scale[2];
    column[2][2] /= decomp->scale[2];
  }

  decomp->skew[1] /= decomp->scale[2];
  decomp->skew[2] /= decomp->scale[2];

  // At this point, the matrix is orthonormal.
  // Check for a coordinate system flip.  If the determinant
  // is -1, then negate the matrix and the scaling factors.
  // TODO(kevers): This is inconsistent from the 2D specification, in which
  // only 1 axis is flipped when the determinant is negative. Verify if it is
  // correct to flip all of the scales and matrix elements, as this introduces
  // rotation for the simple case of a single axis scale inversion.
  SkMScalar pdum3[3];
  Cross3(pdum3, column[1], column[2]);
  if (Dot<3>(column[0], pdum3) < 0) {
    for (int i = 0; i < 3; i++) {
      decomp->scale[i] *= -1.0;
      for (int j = 0; j < 3; ++j)
        column[i][j] *= -1.0;
    }
  }

  // See https://en.wikipedia.org/wiki/Rotation_matrix#Quaternion.
  // Note: deviating from spec (http://www.w3.org/TR/css3-transforms/)
  // which has a degenerate case of zero off-diagonal elements in the
  // orthonormal matrix, which leads to errors in determining the sign
  // of the quaternions.
  double q_xx = SkMScalarToDouble(column[0][0]);
  double q_xy = SkMScalarToDouble(column[1][0]);
  double q_xz = SkMScalarToDouble(column[2][0]);
  double q_yx = SkMScalarToDouble(column[0][1]);
  double q_yy = SkMScalarToDouble(column[1][1]);
  double q_yz = SkMScalarToDouble(column[2][1]);
  double q_zx = SkMScalarToDouble(column[0][2]);
  double q_zy = SkMScalarToDouble(column[1][2]);
  double q_zz = SkMScalarToDouble(column[2][2]);

  double r, s, t, x, y, z, w;
  t = q_xx + q_yy + q_zz;
  if (t > 0) {
    r = std::sqrt(1.0 + t);
    s = 0.5 / r;
    w = 0.5 * r;
    x = (q_zy - q_yz) * s;
    y = (q_xz - q_zx) * s;
    z = (q_yx - q_xy) * s;
  } else if (q_xx > q_yy && q_xx > q_zz) {
    r = std::sqrt(1.0 + q_xx - q_yy - q_zz);
    s = 0.5 / r;
    x = 0.5 * r;
    y = (q_xy + q_yx) * s;
    z = (q_xz + q_zx) * s;
    w = (q_zy - q_yz) * s;
  } else if (q_yy > q_zz) {
    r = std::sqrt(1.0 - q_xx + q_yy - q_zz);
    s = 0.5 / r;
    x = (q_xy + q_yx) * s;
    y = 0.5 * r;
    z = (q_yz + q_zy) * s;
    w = (q_xz - q_zx) * s;
  } else {
    r = std::sqrt(1.0 - q_xx - q_yy + q_zz);
    s = 0.5 / r;
    x = (q_xz + q_zx) * s;
    y = (q_yz + q_zy) * s;
    z = 0.5 * r;
    w = (q_yx - q_xy) * s;
  }

  decomp->quaternion.set_x(SkDoubleToMScalar(x));
  decomp->quaternion.set_y(SkDoubleToMScalar(y));
  decomp->quaternion.set_z(SkDoubleToMScalar(z));
  decomp->quaternion.set_w(SkDoubleToMScalar(w));

  return true;
}

// Taken from http://www.w3.org/TR/css3-transforms/.
Transform ComposeTransform(const DecomposedTransform& decomp) {
  SkMatrix44 perspective = BuildPerspectiveMatrix(decomp);
  SkMatrix44 translation = BuildTranslationMatrix(decomp);
  SkMatrix44 rotation = BuildRotationMatrix(decomp);
  SkMatrix44 skew = BuildSkewMatrix(decomp);
  SkMatrix44 scale = BuildScaleMatrix(decomp);

  return ComposeTransform(perspective, translation, rotation, skew, scale);
}

bool SnapTransform(Transform* out,
                   const Transform& transform,
                   const Rect& viewport) {
  DecomposedTransform decomp;
  DecomposeTransform(&decomp, transform);

  SkMatrix44 rotation_matrix = BuildSnappedRotationMatrix(decomp);
  SkMatrix44 translation = BuildSnappedTranslationMatrix(decomp);
  SkMatrix44 scale = BuildSnappedScaleMatrix(decomp);

  // Rebuild matrices for other unchanged components.
  SkMatrix44 perspective = BuildPerspectiveMatrix(decomp);

  // Completely ignore the skew.
  SkMatrix44 skew(SkMatrix44::kIdentity_Constructor);

  // Get full tranform
  Transform snapped =
      ComposeTransform(perspective, translation, rotation_matrix, skew, scale);

  // Verify that viewport is not moved unnaturally.
  bool snappable = CheckTransformsMapsIntViewportWithinOnePixel(
      viewport, transform, snapped);
  if (snappable) {
    *out = snapped;
  }
  return snappable;
}

Transform TransformAboutPivot(const Point& pivot, const Transform& transform) {
  Transform result;
  result.Translate(pivot.x(), pivot.y());
  result.PreconcatTransform(transform);
  result.Translate(-pivot.x(), -pivot.y());
  return result;
}

Transform TransformBetweenRects(const RectF& src, const RectF& dst) {
  DCHECK(!src.IsEmpty() && !dst.IsEmpty());
  Transform result;
  result.Translate(dst.origin() - src.origin());
  result.Scale(dst.width() / src.width(), dst.height() / src.height());
  return result;
}

std::string DecomposedTransform::ToString() const {
  return base::StringPrintf(
      "translate: %+0.4f %+0.4f %+0.4f\n"
      "scale: %+0.4f %+0.4f %+0.4f\n"
      "skew: %+0.4f %+0.4f %+0.4f\n"
      "perspective: %+0.4f %+0.4f %+0.4f %+0.4f\n"
      "quaternion: %+0.4f %+0.4f %+0.4f %+0.4f\n",
      translate[0], translate[1], translate[2], scale[0], scale[1], scale[2],
      skew[0], skew[1], skew[2], perspective[0], perspective[1], perspective[2],
      perspective[3], quaternion.x(), quaternion.y(), quaternion.z(),
      quaternion.w());
}

}  // namespace gfx
