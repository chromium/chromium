// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/geometry/transform.h"

#include <ostream>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/angle_conversions.h"
#include "base/strings/stringprintf.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/clamp_float_geometry.h"
#include "ui/gfx/geometry/decomposed_transform.h"
#include "ui/gfx/geometry/double4.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/sin_cos_degrees.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace gfx {

namespace {

const double kEpsilon = std::numeric_limits<float>::epsilon();

double TanDegrees(double degrees) {
  return std::tan(base::DegToRad(degrees));
}

inline bool ApproximatelyZero(double x, double tolerance) {
  return std::abs(x) <= tolerance;
}

inline bool ApproximatelyOne(double x, double tolerance) {
  return std::abs(x - 1) <= tolerance;
}

Matrix44 AxisTransform2dToMatrix44(const AxisTransform2d& axis_2d) {
  return Matrix44(axis_2d.scale().x(), 0, 0, 0,  // col 0
                  0, axis_2d.scale().y(), 0, 0,  // col 1
                  0, 0, 1, 0,                    // col 2
                  axis_2d.translation().x(), axis_2d.translation().y(), 0, 1);
}

template <typename T>
void AxisTransform2dToColMajor(const AxisTransform2d& axis_2d, T a[16]) {
  a[0] = axis_2d.scale().x();
  a[5] = axis_2d.scale().y();
  a[12] = axis_2d.translation().x();
  a[13] = axis_2d.translation().y();
  a[1] = a[2] = a[3] = a[4] = a[6] = a[7] = a[8] = a[9] = a[11] = a[14] = 0;
  a[10] = a[15] = 1;
}

}  // namespace

// clang-format off
Transform::Transform(const Quaternion& q)
    : Transform(
          // Col 0.
          1.0 - 2.0 * (q.y() * q.y() + q.z() * q.z()),
          2.0 * (q.x() * q.y() + q.z() * q.w()),
          2.0 * (q.x() * q.z() - q.y() * q.w()),
          0,
          // Col 1.
          2.0 * (q.x() * q.y() - q.z() * q.w()),
          1.0 - 2.0 * (q.x() * q.x() + q.z() * q.z()),
          2.0 * (q.y() * q.z() + q.x() * q.w()),
          0,
          // Col 2.
          2.0 * (q.x() * q.z() + q.y() * q.w()),
          2.0 * (q.y() * q.z() - q.x() * q.w()),
          1.0 - 2.0 * (q.x() * q.x() + q.y() * q.y()),
          0,
          // Col 3.
          0, 0, 0, 1) {}
// clang-format on

Matrix44 Transform::GetFullMatrix() const {
  if (!full_matrix_) [[likely]] {
    return AxisTransform2dToMatrix44(axis_2d_);
  }
  return matrix_;
}

Matrix44& Transform::EnsureFullMatrix() {
  if (!full_matrix_) [[likely]] {
    full_matrix_ = true;
    matrix_ = AxisTransform2dToMatrix44(axis_2d_);
  }
  return matrix_;
}

// static
Transform Transform::ColMajor(const double a[16]) {
  return Transform(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9],
                   a[10], a[11], a[12], a[13], a[14], a[15]);
}

// static
Transform Transform::ColMajorF(const float a[16]) {
  if (AllTrue(Float4{a[1], a[2], a[3], a[4]} == Float4{0, 0, 0, 0} &
              Float4{a[6], a[7], a[8], a[9]} == Float4{0, 0, 0, 0} &
              Float4{a[10], a[11], a[14], a[15]} == Float4{1, 0, 0, 1})) {
    return Transform(a[0], a[5], a[12], a[13]);
  }
  return Transform(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9],
                   a[10], a[11], a[12], a[13], a[14], a[15]);
}

void Transform::GetColMajor(double a[16]) const {
  if (!full_matrix_) [[likely]] {
    AxisTransform2dToColMajor(axis_2d_, a);
  } else {
    matrix_.GetColMajor(a);
  }
}

void Transform::GetColMajorF(float a[16]) const {
  if (!full_matrix_) [[likely]] {
    AxisTransform2dToColMajor(axis_2d_, a);
  } else {
    matrix_.GetColMajorF(a);
  }
}

void Transform::RotateAboutXAxis(double degrees) {
  SinCos sin_cos = SinCosDegrees(degrees);
  if (sin_cos.IsZeroAngle())
    return;
  EnsureFullMatrix().RotateAboutXAxisSinCos(sin_cos.sin, sin_cos.cos);
}

void Transform::RotateAboutYAxis(double degrees) {
  SinCos sin_cos = SinCosDegrees(degrees);
  if (sin_cos.IsZeroAngle())
    return;
  EnsureFullMatrix().RotateAboutYAxisSinCos(sin_cos.sin, sin_cos.cos);
}

void Transform::RotateAboutZAxis(double degrees) {
  SinCos sin_cos = SinCosDegrees(degrees);
  if (sin_cos.IsZeroAngle())
    return;
  EnsureFullMatrix().RotateAboutZAxisSinCos(sin_cos.sin, sin_cos.cos);
}

void Transform::RotateAbout(double x, double y, double z, double degrees) {
  SinCos sin_cos = SinCosDegrees(degrees);
  if (sin_cos.IsZeroAngle())
    return;

  double square_length = x * x + y * y + z * z;
  if (square_length == 0)
    return;
  if (square_length != 1) {
    double scale = 1.0 / sqrt(square_length);
    x *= scale;
    y *= scale;
    z *= scale;
  }
  EnsureFullMatrix().RotateUnitSinCos(x, y, z, sin_cos.sin, sin_cos.cos);
}

void Transform::RotateAbout(const Vector3dF& axis, double degrees) {
  RotateAbout(axis.x(), axis.y(), axis.z(), degrees);
}

double Transform::Determinant() const {
  if (!full_matrix_) [[likely]] {
    return axis_2d_.Determinant();
  }
  return matrix_.Determinant();
}

void Transform::Scale(float x, float y) {
  if (!full_matrix_) [[likely]] {
    axis_2d_.PreScale(Vector2dF(x, y));
  } else {
    matrix_.PreScale(x, y);
  }
}

void Transform::PostScale(float x, float y) {
  if (!full_matrix_) [[likely]] {
    axis_2d_.PostScale(Vector2dF(x, y));
  } else {
    matrix_.PostScale(x, y);
  }
}

void Transform::Scale3d(float x, float y, float z) {
  if (z == 1)
    Scale(x, y);
  else
    EnsureFullMatrix().PreScale3d(x, y, z);
}

void Transform::PostScale3d(float x, float y, float z) {
  if (z == 1)
    PostScale(x, y);
  else
    EnsureFullMatrix().PostScale3d(x, y, z);
}

void Transform::Translate(const Vector2dF& offset) {
  Translate(offset.x(), offset.y());
}

void Transform::Translate(float x, float y) {
  if (!full_matrix_) [[likely]] {
    axis_2d_.PreTranslate(Vector2dF(x, y));
  } else {
    matrix_.PreTranslate(x, y);
  }
}

void Transform::PostTranslate(const Vector2dF& offset) {
  PostTranslate(offset.x(), offset.y());
}

void Transform::PostTranslate(float x, float y) {
  if (!full_matrix_) [[likely]] {
    axis_2d_.PostTranslate(Vector2dF(x, y));
  } else {
    matrix_.PostTranslate(x, y);
  }
}

void Transform::PostTranslate3d(const Vector3dF& offset) {
  PostTranslate3d(offset.x(), offset.y(), offset.z());
}

void Transform::PostTranslate3d(float x, float y, float z) {
  if (z == 0)
    PostTranslate(x, y);
  else
    EnsureFullMatrix().PostTranslate3d(x, y, z);
}

void Transform::Translate3d(const Vector3dF& offset) {
  Translate3d(offset.x(), offset.y(), offset.z());
}

void Transform::Translate3d(float x, float y, float z) {
  if (z == 0)
    Translate(x, y);
  else
    EnsureFullMatrix().PreTranslate3d(x, y, z);
}

void Transform::Skew(double degrees_x, double degrees_y) {
  if (!degrees_x && !degrees_y)
    return;
  EnsureFullMatrix().Skew(TanDegrees(degrees_x), TanDegrees(degrees_y));
}

void Transform::ApplyPerspectiveDepth(double depth) {
  if (depth == 0)
    return;

  EnsureFullMatrix().ApplyPerspectiveDepth(depth);
}

void Transform::PreConcat(const Transform& transform) {
  if (!transform.full_matrix_) [[likely]] {
    PreConcat(transform.axis_2d_);
  } else if (!full_matrix_) [[likely]] {
    AxisTransform2d self = axis_2d_;
    *this = transform;
    PostConcat(self);
  } else {
    matrix_.PreConcat(transform.matrix_);
  }
}

void Transform::PostConcat(const Transform& transform) {
  if (!transform.full_matrix_) [[likely]] {
    PostConcat(transform.axis_2d_);
  } else if (!full_matrix_) [[likely]] {
    AxisTransform2d self = axis_2d_;
    *this = transform;
    PreConcat(self);
  } else {
    matrix_.PostConcat(transform.matrix_);
  }
}

Transform Transform::operator*(const Transform& transform) const {
  if (!transform.full_matrix_) [[likely]] {
    Transform result = *this;
    result.PreConcat(transform.axis_2d_);
    return result;
  }
  if (!full_matrix_) [[likely]] {
    Transform result = transform;
    result.PostConcat(axis_2d_);
    return result;
  }
  Transform result(Matrix44::kUninitialized);
  result.matrix_.SetConcat(matrix_, transform.matrix_);
  return result;
}

void Transform::PreConcat(const AxisTransform2d& transform) {
  Translate(transform.translation());
  Scale(transform.scale().x(), transform.scale().y());
}

void Transform::PostConcat(const AxisTransform2d& transform) {
  PostScale(transform.scale().x(), transform.scale().y());
  PostTranslate(transform.translation());
}

bool Transform::IsApproximatelyIdentityOrTranslation(double tolerance) const {
  DCHECK_GE(tolerance, 0);
  if (!full_matrix_) [[likely]] {
    return ApproximatelyOne(axis_2d_.scale().x(), tolerance) &&
           ApproximatelyOne(axis_2d_.scale().y(), tolerance);
  }

  if (!ApproximatelyOne(matrix_.rc(0, 0), tolerance) ||
      !ApproximatelyZero(matrix_.rc(1, 0), tolerance) ||
      !ApproximatelyZero(matrix_.rc(2, 0), tolerance) ||
      !ApproximatelyZero(matrix_.rc(0, 1), tolerance) ||
      !ApproximatelyOne(matrix_.rc(1, 1), tolerance) ||
      !ApproximatelyZero(matrix_.rc(2, 1), tolerance) ||
      !ApproximatelyZero(matrix_.rc(0, 2), tolerance) ||
      !ApproximatelyZero(matrix_.rc(1, 2), tolerance) ||
      !ApproximatelyOne(matrix_.rc(2, 2), tolerance)) {
    return false;
  }

  // Check perspective components more strictly by using the smaller of float
  // epsilon and |tolerance|.
  const double perspective_tolerance = std::min(kEpsilon, tolerance);
  return ApproximatelyZero(matrix_.rc(3, 0), perspective_tolerance) &&
         ApproximatelyZero(matrix_.rc(3, 1), perspective_tolerance) &&
         ApproximatelyZero(matrix_.rc(3, 2), perspective_tolerance) &&
         ApproximatelyOne(matrix_.rc(3, 3), perspective_tolerance);
}

bool Transform::IsApproximatelyIdentityOrIntegerTranslation(
    double tolerance) const {
  if (!IsApproximatelyIdentityOrTranslation(tolerance))
    return false;

  if (!full_matrix_) [[likely]] {
    for (float t : {axis_2d_.translation().x(), axis_2d_.translation().y()}) {
      if (!base::IsValueInRangeForNumericType<int>(t) ||
          std::abs(std::round(t) - t) > tolerance)
        return false;
    }
    return true;
  }

  for (double t : {matrix_.rc(0, 3), matrix_.rc(1, 3), matrix_.rc(2, 3)}) {
    if (!base::IsValueInRangeForNumericType<int>(t) ||
        std::abs(std::round(t) - t) > tolerance)
      return false;
  }
  return true;
}

bool Transform::Is2dProportionalUpscaleAndOr2dTranslation() const {
  if (!full_matrix_) [[likely]] {
    return axis_2d_.scale().x() >= 1 &&
           axis_2d_.scale().x() == axis_2d_.scale().y();
  }

  return matrix_.IsScaleOrTranslation() &&
         // Check proportional upscale.
         matrix_.rc(0, 0) >= 1 && matrix_.rc(1, 1) == matrix_.rc(0, 0) &&
         // Check no scale/translation in z axis.
         matrix_.rc(2, 2) == 1 && matrix_.rc(2, 3) == 0;
}

bool Transform::IsIdentityOrIntegerTranslation() const {
  if (!IsIdentityOrTranslation())
    return false;

  if (!full_matrix_) [[likely]] {
    for (float t : {axis_2d_.translation().x(), axis_2d_.translation().y()}) {
      if (!base::IsValueInRangeForNumericType<int>(t) ||
          static_cast<int>(t) != t) {
        return false;
      }
    }
    return true;
  }

  for (double t : {matrix_.rc(0, 3), matrix_.rc(1, 3), matrix_.rc(2, 3)}) {
    if (!base::IsValueInRangeForNumericType<int>(t) || static_cast<int>(t) != t)
      return false;
  }
  return true;
}

bool Transform::IsIdentityOrInteger2dTranslation() const {
  return IsIdentityOrIntegerTranslation() && rc(2, 3) == 0;
}

bool Transform::Creates3d() const {
  if (!full_matrix_) [[likely]] {
    return false;
  }
  return matrix_.rc(2, 0) != 0 || matrix_.rc(2, 1) != 0 ||
         matrix_.rc(2, 3) != 0;
}

bool Transform::IsBackFaceVisible() const {
  if (!full_matrix_) [[likely]] {
    return false;
  }

  // Compute whether a layer with a forward-facing normal of (0, 0, 1, 0)
  // would have its back face visible after applying the transform.
  // This is done by transforming the normal and seeing if the resulting z
  // value is positive or negative. However, note that transforming a normal
  // actually requires using the inverse-transpose of the original transform.
  //
  // We can avoid inverting and transposing the matrix since we know we want
  // to transform only the specific normal vector (0, 0, 1, 0). In this case,
  // we only need the 3rd row, 3rd column of the inverse-transpose. We can
  // calculate only the 3rd row 3rd column element of the inverse, skipping
  // everything else.
  //
  // For more information, refer to:
  //   http://en.wikipedia.org/wiki/Invertible_matrix#Analytic_solution
  //

  double determinant = matrix_.Determinant();

  // If matrix was not invertible, then just assume back face is not visible.
  if (determinant == 0)
    return false;

  // Compute the cofactor of the 3rd row, 3rd column.
  double cofactor_part_1 =
      matrix_.rc(0, 0) * matrix_.rc(1, 1) * matrix_.rc(3, 3);

  double cofactor_part_2 =
      matrix_.rc(0, 1) * matrix_.rc(1, 3) * matrix_.rc(3, 0);

  double cofactor_part_3 =
      matrix_.rc(0, 3) * matrix_.rc(1, 0) * matrix_.rc(3, 1);

  double cofactor_part_4 =
      matrix_.rc(0, 0) * matrix_.rc(1, 3) * matrix_.rc(3, 1);

  double cofactor_part_5 =
      matrix_.rc(0, 1) * matrix_.rc(1, 0) * matrix_.rc(3, 3);

  double cofactor_part_6 =
      matrix_.rc(0, 3) * matrix_.rc(1, 1) * matrix_.rc(3, 0);

  double cofactor33 = cofactor_part_1 + cofactor_part_2 + cofactor_part_3 -
                      cofactor_part_4 - cofactor_part_5 - cofactor_part_6;

  // Technically the transformed z component is cofactor33 / determinant.  But
  // we can avoid the costly division because we only care about the resulting
  // +/- sign; we can check this equivalently by multiplication.
  return cofactor33 * determinant < -kEpsilon;
}

bool Transform::GetInverse(Transform* transform) const {
  if (!full_matrix_) [[likely]] {
    transform->full_matrix_ = false;
    if (axis_2d_.IsInvertible()) {
      transform->axis_2d_ = axis_2d_;
      transform->axis_2d_.Invert();
      return true;
    }
    transform->axis_2d_ = AxisTransform2d();
    return false;
  }

  if (matrix_.GetInverse(transform->matrix_)) {
    transform->full_matrix_ = true;
    return true;
  }

  // Initialize the return value to identity if this matrix turned
  // out to be un-invertible.
  transform->MakeIdentity();
  return false;
}

Transform Transform::GetCheckedInverse() const {
  Transform inverse;
  if (!GetInverse(&inverse))
    DUMP_WILL_BE_NOTREACHED() << ToString() << " is not invertible";
  return inverse;
}

Transform Transform::InverseOrIdentity() const {
  Transform inverse;
  bool invertible = GetInverse(&inverse);
  DCHECK(invertible || inverse.IsIdentity());
  return inverse;
}

bool Transform::Preserves2dAffine() const {
  if (!full_matrix_) [[likely]] {
    return true;
  }

  // The first two columns of row 2 allow the x and y axis to skew in the z
  // direction. We also check there is no z translation. We can ignore the z
  // scale component since it cannot affect coordinates where z = 0.
  const bool is_flat_ignore_z = gfx::AllTrue(gfx::Double4{
                                                 matrix_.rc(2, 0),
                                                 matrix_.rc(2, 1),
                                                 0,
                                                 matrix_.rc(2, 3),
                                             } == gfx::Double4{0, 0, 0, 0});

  // We must ensure that the x and y perspective components are 0 since they can
  // affect the affine-ness of the x/y plane. We can ignore the z perspective
  // component since it does not affect values on the x/y plane.
  const bool has_no_perspective_ignore_z =
      gfx::AllTrue(gfx::Double4{
                       matrix_.rc(3, 0),
                       matrix_.rc(3, 1),
                       0,
                       matrix_.rc(3, 3),
                   } == gfx::Double4{0, 0, 0, 1});

  if (is_flat_ignore_z && has_no_perspective_ignore_z) {
    return true;
  }

  return false;
}

bool Transform::Preserves2dAxisAlignment() const {
  if (!full_matrix_) [[likely]] {
    return true;
  }

  // Check whether an axis aligned 2-dimensional rect would remain axis-aligned
  // after being transformed by this matrix (and implicitly projected by
  // dropping any non-zero z-values).
  //
  // The 4th column can be ignored because translations don't affect axis
  // alignment. The 3rd column can be ignored because we are assuming 2d
  // inputs, where z-values will be zero. The 3rd row can also be ignored
  // because we are assuming 2d outputs, and any resulting z-value is dropped
  // anyway. For the inner 2x2 portion, the only effects that keep a rect axis
  // aligned are (1) swapping axes and (2) scaling axes. This can be checked by
  // verifying only 1 element of every column and row is non-zero.  Degenerate
  // cases that project the x or y dimension to zero are considered to preserve
  // axis alignment.
  //
  // If the matrix does have perspective component that is affected by x or y
  // values: The current implementation conservatively assumes that axis
  // alignment is not preserved.

  bool has_x_or_y_perspective = matrix_.rc(3, 0) != 0 || matrix_.rc(3, 1) != 0;

  int num_non_zero_in_row_0 = 0;
  int num_non_zero_in_row_1 = 0;
  int num_non_zero_in_col_0 = 0;
  int num_non_zero_in_col_1 = 0;

  if (std::abs(matrix_.rc(0, 0)) > kEpsilon) {
    num_non_zero_in_row_0++;
    num_non_zero_in_col_0++;
  }

  if (std::abs(matrix_.rc(0, 1)) > kEpsilon) {
    num_non_zero_in_row_0++;
    num_non_zero_in_col_1++;
  }

  if (std::abs(matrix_.rc(1, 0)) > kEpsilon) {
    num_non_zero_in_row_1++;
    num_non_zero_in_col_0++;
  }

  if (std::abs(matrix_.rc(1, 1)) > kEpsilon) {
    num_non_zero_in_row_1++;
    num_non_zero_in_col_1++;
  }

  return num_non_zero_in_row_0 <= 1 && num_non_zero_in_row_1 <= 1 &&
         num_non_zero_in_col_0 <= 1 && num_non_zero_in_col_1 <= 1 &&
         !has_x_or_y_perspective;
}

bool Transform::NonDegeneratePreserves2dAxisAlignment() const {
  if (!full_matrix_) [[likely]] {
    return axis_2d_.scale().x() > kEpsilon && axis_2d_.scale().y() > kEpsilon;
  }

  // See comments above for Preserves2dAxisAlignment.

  // This function differs from it by requiring:
  //  (1) that there are exactly two nonzero values on a diagonal in
  //      the upper left 2x2 submatrix, and
  //  (2) that the w perspective value is positive.

  bool has_x_or_y_perspective = matrix_.rc(3, 0) != 0 || matrix_.rc(3, 1) != 0;
  bool positive_w_perspective = matrix_.rc(3, 3) > kEpsilon;

  bool have_0_0 = std::abs(matrix_.rc(0, 0)) > kEpsilon;
  bool have_0_1 = std::abs(matrix_.rc(0, 1)) > kEpsilon;
  bool have_1_0 = std::abs(matrix_.rc(1, 0)) > kEpsilon;
  bool have_1_1 = std::abs(matrix_.rc(1, 1)) > kEpsilon;

  return have_0_0 == have_1_1 && have_0_1 == have_1_0 && have_0_0 != have_0_1 &&
         !has_x_or_y_perspective && positive_w_perspective;
}

void Transform::Transpose() {
  if (!IsScale2d())
    EnsureFullMatrix().Transpose();
}

void Transform::ApplyTransformOrigin(float x, float y, float z) {
  PostTranslate3d(x, y, z);
  Translate3d(-x, -y, -z);
}

void Transform::Zoom(float zoom_factor) {
  if (!full_matrix_) [[likely]] {
    axis_2d_.Zoom(zoom_factor);
  } else {
    matrix_.Zoom(zoom_factor);
  }
}

void Transform::Flatten() {
  if (full_matrix_) [[unlikely]] {
    matrix_.Flatten();
  }
  DCHECK(IsFlat());
}

bool Transform::IsFlat() const {
  if (!full_matrix_) [[likely]] {
    return true;
  }
  return matrix_.IsFlat();
}

bool Transform::Is2dTransform() const {
  if (!full_matrix_) [[likely]] {
    return true;
  }
  return matrix_.Is2dTransform();
}

Vector2dF Transform::To2dTranslation() const {
  if (!full_matrix_) [[likely]] {
    return Vector2dF(ClampFloatGeometry(axis_2d_.translation().x()),
                     ClampFloatGeometry(axis_2d_.translation().y()));
  }
  return Vector2dF(ClampFloatGeometry(matrix_.rc(0, 3)),
                   ClampFloatGeometry(matrix_.rc(1, 3)));
}

Vector3dF Transform::To3dTranslation() const {
  if (!full_matrix_) [[likely]] {
    return Vector3dF(ClampFloatGeometry(axis_2d_.translation().x()),
                     ClampFloatGeometry(axis_2d_.translation().y()), 0);
  }
  return Vector3dF(ClampFloatGeometry(matrix_.rc(0, 3)),
                   ClampFloatGeometry(matrix_.rc(1, 3)),
                   ClampFloatGeometry(matrix_.rc(2, 3)));
}

Vector2dF Transform::To2dScale() const {
  if (!full_matrix_) [[likely]] {
    return Vector2dF(ClampFloatGeometry(axis_2d_.scale().x()),
                     ClampFloatGeometry(axis_2d_.scale().y()));
  }
  return Vector2dF(ClampFloatGeometry(matrix_.rc(0, 0)),
                   ClampFloatGeometry(matrix_.rc(1, 1)));
}

Point Transform::MapPoint(const Point& point) const {
  return gfx::ToRoundedPoint(MapPoint(gfx::PointF(point)));
}

PointF Transform::MapPoint(const PointF& point) const {
  if (!full_matrix_) [[likely]] {
    return axis_2d_.MapPoint(point);
  }
  return MapPointInternal(matrix_, point);
}

Point3F Transform::MapPoint(const Point3F& point) const {
  if (!full_matrix_) [[likely]] {
    PointF result = axis_2d_.MapPoint(point.AsPointF());
    return Point3F(result.x(), result.y(), ClampFloatGeometry(point.z()));
  }
  return MapPointInternal(matrix_, point);
}

Vector3dF Transform::MapVector(const Vector3dF& vector) const {
  if (!full_matrix_) [[likely]] {
    return Vector3dF(ClampFloatGeometry(vector.x() * axis_2d_.scale().x()),
                     ClampFloatGeometry(vector.y() * axis_2d_.scale().y()),
                     ClampFloatGeometry(vector.z()));
  }
  double p[4] = {vector.x(), vector.y(), vector.z(), 0};
  matrix_.MapVector4(p);
  return Vector3dF(ClampFloatGeometry(p[0]), ClampFloatGeometry(p[1]),
                   ClampFloatGeometry(p[2]));
}

void Transform::TransformVector4(float vector[4]) const {
  DCHECK(vector);
  if (!full_matrix_) [[likely]] {
    vector[0] = vector[0] * axis_2d_.scale().x() +
                vector[3] * axis_2d_.translation().x();
    vector[1] = vector[1] * axis_2d_.scale().y() +
                vector[3] * axis_2d_.translation().y();
    for (int i = 0; i < 4; i++)
      vector[i] = ClampFloatGeometry(vector[i]);
  } else {
    double v[4] = {vector[0], vector[1], vector[2], vector[3]};
    matrix_.MapVector4(v);
    for (int i = 0; i < 4; i++)
      vector[i] = ClampFloatGeometry(v[i]);
  }
}

std::optional<PointF> Transform::InverseMapPoint(const PointF& point) const {
  if (!full_matrix_) [[likely]] {
    if (!axis_2d_.IsInvertible())
      return std::nullopt;
    return axis_2d_.InverseMapPoint(point);
  }
  Matrix44 inverse(Matrix44::kUninitialized);
  if (!matrix_.GetInverse(inverse))
    return std::nullopt;
  return MapPointInternal(inverse, point);
}

std::optional<Point> Transform::InverseMapPoint(const Point& point) const {
  if (std::optional<PointF> point_f = InverseMapPoint(PointF(point))) {
    return ToRoundedPoint(*point_f);
  }
  return std::nullopt;
}

std::optional<Point3F> Transform::InverseMapPoint(const Point3F& point) const {
  if (!full_matrix_) [[likely]] {
    if (!axis_2d_.IsInvertible())
      return std::nullopt;
    PointF result = axis_2d_.InverseMapPoint(point.AsPointF());
    return Point3F(result.x(), result.y(), ClampFloatGeometry(point.z()));
  }
  Matrix44 inverse(Matrix44::kUninitialized);
  if (!matrix_.GetInverse(inverse))
    return std::nullopt;
  return std::make_optional(MapPointInternal(inverse, point));
}

RectF Transform::MapRect(const RectF& rect) const {
  if (IsIdentity())
    return rect;

  if (!full_matrix_) [[likely]] {
    if (axis_2d_.scale().x() >= 0 && axis_2d_.scale().y() >= 0) {
      return axis_2d_.MapRect(rect);
    }
  }

  return MapQuad(QuadF(rect)).BoundingBox();
}

Rect Transform::MapRect(const Rect& rect) const {
  if (IsIdentity())
    return rect;

  return ToEnclosingRect(MapRect(RectF(rect)));
}

std::optional<RectF> Transform::InverseMapRect(const RectF& rect) const {
  if (IsIdentity())
    return rect;

  if (!full_matrix_) [[likely]] {
    if (!axis_2d_.IsInvertible())
      return std::nullopt;
    if (axis_2d_.scale().x() > 0 && axis_2d_.scale().y() > 0)
      return axis_2d_.InverseMapRect(rect);
  }

  Transform inverse;
  if (!GetInverse(&inverse))
    return std::nullopt;

  return inverse.MapQuad(QuadF(rect)).BoundingBox();
}

std::optional<Rect> Transform::InverseMapRect(const Rect& rect) const {
  if (IsIdentity())
    return rect;

  if (std::optional<RectF> mapped = InverseMapRect(RectF(rect))) {
    return ToEnclosingRect(mapped.value());
  }
  return std::nullopt;
}

BoxF Transform::MapBox(const BoxF& box) const {
  BoxF bounds;
  bool first_point = true;
  for (int corner = 0; corner < 8; ++corner) {
    gfx::Point3F point = box.origin();
    point += gfx::Vector3dF(corner & 1 ? box.width() : 0.f,
                            corner & 2 ? box.height() : 0.f,
                            corner & 4 ? box.depth() : 0.f);
    point = MapPoint(point);
    if (first_point) {
      bounds.set_origin(point);
      first_point = false;
    } else {
      bounds.ExpandTo(point);
    }
  }
  return bounds;
}

QuadF Transform::MapQuad(const QuadF& quad) const {
  return QuadF(MapPoint(quad.p1()), MapPoint(quad.p2()), MapPoint(quad.p3()),
               MapPoint(quad.p4()));
}

PointF Transform::ProjectPoint(const PointF& point, bool* clamped) const {
  // This is basically ray-tracing. We have a point in the destination plane
  // with z=0, and we cast a ray parallel to the z-axis from that point to find
  // the z-position at which it intersects the z=0 plane with the transform
  // applied. Once we have that point we apply the inverse transform to find
  // the corresponding point in the source space.
  //
  // Given a plane with normal Pn, and a ray starting at point R0 and with
  // direction defined by the vector Rd, we can find the intersection point as
  // a distance d from R0 in units of Rd by:
  //
  // d = -dot (Pn', R0) / dot (Pn', Rd)

  if (clamped)
    *clamped = false;

  if (!full_matrix_) [[likely]] {
    return axis_2d_.MapPoint(point);
  }

  if (!std::isnormal(matrix_.rc(2, 2))) {
    // In this case, the projection plane is parallel to the ray we are trying
    // to trace, and there is no well-defined value for the projection.
    if (clamped)
      *clamped = true;
    return gfx::PointF();
  }

  double x = point.x();
  double y = point.y();
  double z = -(matrix_.rc(2, 0) * x + matrix_.rc(2, 1) * y + matrix_.rc(2, 3)) /
             matrix_.rc(2, 2);
  if (!std::isfinite(z)) {
    // Same as the previous condition.
    if (clamped)
      *clamped = true;
    return gfx::PointF();
  }

  double v[4] = {x, y, z, 1};
  matrix_.MapVector4(v);

  if (v[3] <= 0) {
    // To represent infinity and ensure the bounding box of ProjectQuad() is
    // accurate in both float, int and blink::LayoutUnit, we use a large but
    // not-too-large number here when clamping.
    constexpr double kBigNumber = 1 << (std::numeric_limits<float>::digits - 1);
    if (clamped)
      *clamped = true;
    return PointF(std::copysign(kBigNumber, v[0]),
                  std::copysign(kBigNumber, v[1]));
  }

  if (v[3] != 1) {
    v[0] /= v[3];
    v[1] /= v[3];
  }
  return PointF(ClampFloatGeometry(v[0]), ClampFloatGeometry(v[1]));
}

QuadF Transform::ProjectQuad(const QuadF& quad) const {
  bool clamped1 = false;
  bool clamped2 = false;
  bool clamped3 = false;
  bool clamped4 = false;

  QuadF projected_quad(
      ProjectPoint(quad.p1(), &clamped1), ProjectPoint(quad.p2(), &clamped2),
      ProjectPoint(quad.p3(), &clamped3), ProjectPoint(quad.p4(), &clamped4));

  // If all points on the quad had w < 0, then the entire quad would not be
  // visible to the projected surface.
  if (clamped1 && clamped2 && clamped3 && clamped4)
    return QuadF();

  return projected_quad;
}

std::optional<DecomposedTransform> Transform::Decompose() const {
  if (!full_matrix_) [[likely]] {
    // Consider letting 2d decomposition always succeed.
    if (!axis_2d_.IsInvertible())
      return std::nullopt;
    return axis_2d_.Decompose();
  }
  return matrix_.Decompose();
}

// static
Transform Transform::Compose(const DecomposedTransform& decomp) {
  Transform result;

  for (int i = 0; i < 3; i++) {
    if (decomp.perspective[i] != 0)
      result.set_rc(3, i, decomp.perspective[i]);
  }
  if (decomp.perspective[3] != 1)
    result.set_rc(3, 3, decomp.perspective[3]);

  result.Translate3d(decomp.translate[0], decomp.translate[1],
                     decomp.translate[2]);

  result.PreConcat(Transform(decomp.quaternion));

  if (decomp.skew[0] || decomp.skew[1] || decomp.skew[2])
    result.EnsureFullMatrix().ApplyDecomposedSkews(decomp.skew);

  result.Scale3d(decomp.scale[0], decomp.scale[1], decomp.scale[2]);

  return result;
}

bool Transform::Blend(const Transform& from, double progress) {
  std::optional<DecomposedTransform> to_decomp = Decompose();
  if (!to_decomp)
    return false;
  std::optional<DecomposedTransform> from_decomp = from.Decompose();
  if (!from_decomp)
    return false;

  *to_decomp = BlendDecomposedTransforms(*to_decomp, *from_decomp, progress);

  *this = Compose(*to_decomp);
  return true;
}

bool Transform::Accumulate(const Transform& other) {
  std::optional<DecomposedTransform> this_decomp = Decompose();
  if (!this_decomp)
    return false;
  std::optional<DecomposedTransform> other_decomp = other.Decompose();
  if (!other_decomp)
    return false;

  *this_decomp = AccumulateDecomposedTransforms(*this_decomp, *other_decomp);

  *this = Compose(*this_decomp);
  return true;
}

void Transform::Round2dTranslationComponents() {
  if (!full_matrix_) [[likely]] {
    axis_2d_ = AxisTransform2d::FromScaleAndTranslation(
        axis_2d_.scale(), Vector2dF(std::round(axis_2d_.translation().x()),
                                    std::round(axis_2d_.translation().y())));
  } else {
    matrix_.set_rc(0, 3, std::round(matrix_.rc(0, 3)));
    matrix_.set_rc(1, 3, std::round(matrix_.rc(1, 3)));
  }
}

void Transform::RoundToIdentityOrIntegerTranslation() {
  if (!full_matrix_) [[likely]] {
    axis_2d_ = AxisTransform2d::FromScaleAndTranslation(
        Vector2dF(1, 1), Vector2dF(std::round(axis_2d_.translation().x()),
                                   std::round(axis_2d_.translation().y())));
  } else {
    matrix_ =
        Matrix44(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0,  // col0-2
                 std::round(matrix_.rc(0, 3)),        // col3
                 std::round(matrix_.rc(1, 3)), std::round(matrix_.rc(2, 3)), 1);
  }
}

PointF Transform::MapPointInternal(const Matrix44& matrix,
                                   const PointF& point) const {
  DCHECK(full_matrix_);

  double p[2] = {point.x(), point.y()};

  double w = matrix.MapVector2(p);

  if (w != 1.0 && std::isnormal(w)) {
    double w_inverse = 1.0 / w;
    return PointF(ClampFloatGeometry(p[0] * w_inverse),
                  ClampFloatGeometry(p[1] * w_inverse));
  }
  return PointF(ClampFloatGeometry(p[0]), ClampFloatGeometry(p[1]));
}

Point3F Transform::MapPointInternal(const Matrix44& matrix,
                                    const Point3F& point) const {
  DCHECK(full_matrix_);

  double p[4] = {point.x(), point.y(), point.z(), 1};

  matrix.MapVector4(p);

  if (p[3] != 1.0 && std::isnormal(p[3])) {
    double w_inverse = 1.0 / p[3];
    return Point3F(ClampFloatGeometry(p[0] * w_inverse),
                   ClampFloatGeometry(p[1] * w_inverse),
                   ClampFloatGeometry(p[2] * w_inverse));
  }
  return Point3F(ClampFloatGeometry(p[0]), ClampFloatGeometry(p[1]),
                 ClampFloatGeometry(p[2]));
}

bool Transform::ApproximatelyEqual(const gfx::Transform& transform,
                                   float abs_translation_tolerance,
                                   float abs_other_tolerance,
                                   float rel_scale_tolerance) const {
  if (*this == transform)
    return true;

  if (abs_translation_tolerance == 0 && abs_other_tolerance == 0)
    return false;

  auto approximately_equal = [abs_other_tolerance](float a, float b) {
    return std::abs(a - b) <= abs_other_tolerance;
  };
  auto translation_approximately_equal = [abs_translation_tolerance](float a,
                                                                     float b) {
    return std::abs(a - b) <= abs_translation_tolerance;
  };
  auto scale_approximately_equal = [abs_other_tolerance, rel_scale_tolerance](
                                       float a, float b) {
    float diff = std::abs(a - b);
    return diff <= abs_other_tolerance &&
           (rel_scale_tolerance == 0 ||
            diff <= (std::abs(a) + std::abs(b)) * rel_scale_tolerance);
  };

  if (!full_matrix_ && !transform.full_matrix_) [[likely]] {
    return scale_approximately_equal(axis_2d_.scale().x(),
                                     transform.axis_2d_.scale().x()) &&
           scale_approximately_equal(axis_2d_.scale().y(),
                                     transform.axis_2d_.scale().y()) &&
           translation_approximately_equal(
               axis_2d_.translation().x(),
               transform.axis_2d_.translation().x()) &&
           translation_approximately_equal(
               axis_2d_.translation().y(),
               transform.axis_2d_.translation().y());
  }

  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 4; col++) {
      float x = rc(row, col);
      float y = transform.rc(row, col);
      if (row < 3 && col == 3) {
        if (!translation_approximately_equal(x, y))
          return false;
      } else if (row < 3 && col == row) {
        if (!scale_approximately_equal(x, y))
          return false;
      } else if (!approximately_equal(x, y)) {
        return false;
      }
    }
  }
  return true;
}

std::string Transform::ToString() const {
  return base::StringPrintf(
      "[ %lg %lg %lg %lg\n"
      "  %lg %lg %lg %lg\n"
      "  %lg %lg %lg %lg\n"
      "  %lg %lg %lg %lg ]\n",
      rc(0, 0), rc(0, 1), rc(0, 2), rc(0, 3), rc(1, 0), rc(1, 1), rc(1, 2),
      rc(1, 3), rc(2, 0), rc(2, 1), rc(2, 2), rc(2, 3), rc(3, 0), rc(3, 1),
      rc(3, 2), rc(3, 3));
}

std::string Transform::ToDecomposedString() const {
  std::optional<gfx::DecomposedTransform> decomp = Decompose();
  if (!decomp)
    return ToString() + "(degenerate)";

  if (IsIdentity())
    return "identity";

  if (IsIdentityOrTranslation()) {
    return base::StringPrintf("translate: %lg,%lg,%lg", decomp->translate[0],
                              decomp->translate[1], decomp->translate[2]);
  }

  return decomp->ToString();
}

}  // namespace gfx
