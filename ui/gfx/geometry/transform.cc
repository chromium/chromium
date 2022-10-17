// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/transform.h"

#include "base/check_op.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/clamp_float_geometry.h"
#include "ui/gfx/geometry/double4.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace gfx {

namespace {

const double kEpsilon = std::numeric_limits<float>::epsilon();

double TanDegrees(double degrees) {
  return std::tan(gfx::DegToRad(degrees));
}

inline bool ApproximatelyZero(double x, double tolerance) {
  return std::abs(x) <= tolerance;
}

inline bool ApproximatelyOne(double x, double tolerance) {
  return std::abs(x - 1) <= tolerance;
}

}  // namespace

Transform::Transform() = default;
Transform::~Transform() = default;
Transform::Transform(SkipInitialization) {}
Transform::Transform(Transform&&) = default;
Transform& Transform::operator=(Transform&&) = default;

// clang-format off
Transform::Transform(double r0c0, double r1c0, double r2c0, double r3c0,
                     double r0c1, double r1c1, double r2c1, double r3c1,
                     double r0c2, double r1c2, double r2c2, double r3c2,
                     double r0c3, double r1c3, double r2c3, double r3c3) {
  if (AllTrue(Double4{r1c0, r2c0, r3c0, r0c1} == Double4{0, 0, 0, 0} &
              Double4{r2c1, r3c1, r0c2, r1c2} == Double4{0, 0, 0, 0} &
              Double4{r2c2, r3c2, r2c3, r3c3} == Double4{1, 0, 0, 1})) {
    axis_2d_ = AxisTransform2d::FromScaleAndTranslation(
        Vector2dF(r0c0, r1c1), Vector2dF(r0c3, r1c3));
  } else {
    // The parameters of SkMatrix's constructor is in row-major order.
    matrix_ = std::make_unique<Matrix44>(r0c0, r0c1, r0c2, r0c3,   // row 0
                                         r1c0, r1c1, r1c2, r1c3,   // row 1
                                         r2c0, r2c1, r2c2, r2c3,   // row 2
                                         r3c0, r3c1, r3c2, r3c3);  // row 3
  }
}

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

Transform::Transform(float scale_x, float scale_y, float trans_x, float trans_y)
    : axis_2d_(AxisTransform2d::FromScaleAndTranslation(
          Vector2dF(scale_x, scale_y),
          Vector2dF(trans_x, trans_y))) {}

Transform::Transform(const Transform& rhs)
    : axis_2d_(rhs.axis_2d_),
      matrix_(rhs.matrix_ ? std::make_unique<Matrix44>(*rhs.matrix_)
                          : nullptr) {}

Transform& Transform::operator=(const Transform& rhs) {
  if (LIKELY(!rhs.matrix_)) {
    axis_2d_ = rhs.axis_2d_;
    matrix_ = nullptr;
  } else if (matrix_) {
    *matrix_ = *rhs.matrix_;
  } else {
    matrix_ = std::make_unique<Matrix44>(*rhs.matrix_);
  }
  return *this;
}

// static
const Transform& Transform::Identity() {
  static const base::NoDestructor<Transform> kIdentity;
  return *kIdentity;
}

Matrix44 Transform::GetFullMatrix() const {
  if (LIKELY(!matrix_)) {
    return Matrix44(axis_2d_.scale().x(), 0, 0, axis_2d_.translation().x(), 0,
                    axis_2d_.scale().y(), 0, axis_2d_.translation().y(), 0, 0,
                    1, 0, 0, 0, 0, 1);
  }
  return *matrix_;
}

Matrix44& Transform::EnsureFullMatrix() {
  if (LIKELY(!matrix_)) {
    matrix_ = std::make_unique<Matrix44>(
        axis_2d_.scale().x(), 0, 0, axis_2d_.translation().x(),  // row 0
        0, axis_2d_.scale().y(), 0, axis_2d_.translation().y(),  // row 1
        0, 0, 1, 0,                                              // row 2
        0, 0, 0, 1);                                             // row 3
  }
  return *matrix_;
}

// static
Transform Transform::ColMajorF(const float a[16]) {
  return Transform(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9],
                   a[10], a[11], a[12], a[13], a[14], a[15]);
}

void Transform::GetColMajorF(float a[16]) const {
  if (LIKELY(!matrix_)) {
    a[0] = axis_2d_.scale().x();
    a[5] = axis_2d_.scale().y();
    a[12] = axis_2d_.translation().x();
    a[13] = axis_2d_.translation().y();
    a[1] = a[2] = a[3] = a[4] = a[6] = a[7] = a[8] = a[9] = a[11] = a[14] = 0;
    a[10] = a[15] = 1;
  } else {
    matrix_->getColMajor(a);
  }
}

void Transform::RotateAboutXAxis(double degrees) {
  if (degrees == 0)
    return;
  double radians = gfx::DegToRad(degrees);
  double sin_angle = std::sin(radians);
  double cos_angle = std::cos(radians);
  Transform t(kSkipInitialization);
  t.EnsureFullMatrix().setRotateAboutXAxisSinCos(sin_angle, cos_angle);
  PreConcat(t);
}

void Transform::RotateAboutYAxis(double degrees) {
  if (degrees == 0)
    return;
  double radians = gfx::DegToRad(degrees);
  double sin_angle = std::sin(radians);
  double cos_angle = std::cos(radians);
  Transform t(kSkipInitialization);
  t.EnsureFullMatrix().setRotateAboutYAxisSinCos(sin_angle, cos_angle);
  PreConcat(t);
}

void Transform::RotateAboutZAxis(double degrees) {
  if (degrees == 0)
    return;
  double radians = gfx::DegToRad(degrees);
  double sin_angle = std::sin(radians);
  double cos_angle = std::cos(radians);
  Transform t(kSkipInitialization);
  t.EnsureFullMatrix().setRotateAboutZAxisSinCos(sin_angle, cos_angle);
  PreConcat(t);
}

void Transform::RotateAbout(const Vector3dF& axis, double degrees) {
  if (degrees == 0)
    return;
  double x = axis.x();
  double y = axis.y();
  double z = axis.z();
  double square_length = x * x + y * y + z * z;
  if (square_length == 0)
    return;
  if (square_length != 1) {
    double scale = 1 / sqrt(square_length);
    x *= scale;
    y *= scale;
    z *= scale;
  }
  double radians = gfx::DegToRad(degrees);
  double sin_angle = std::sin(radians);
  double cos_angle = std::cos(radians);
  Transform t(kSkipInitialization);
  t.EnsureFullMatrix().setRotateUnitSinCos(x, y, z, sin_angle, cos_angle);
  PreConcat(t);
}

double Transform::Determinant() const {
  return LIKELY(!matrix_) ? axis_2d_.Determinant() : matrix_->determinant();
}

void Transform::Scale(double x, double y) {
  if (LIKELY(!matrix_))
    axis_2d_.PreScale(Vector2dF(x, y));
  else
    matrix_->preScale(x, y, 1);
}

void Transform::PostScale(double x, double y) {
  if (LIKELY(!matrix_))
    axis_2d_.PostScale(Vector2dF(x, y));
  else
    matrix_->postScale(x, y, 1);
}

void Transform::Scale3d(double x, double y, double z) {
  if (z == 1)
    Scale(x, y);
  else
    EnsureFullMatrix().preScale(x, y, z);
}

void Transform::PostScale3d(double x, double y, double z) {
  if (z == 1)
    PostScale(x, y);
  else
    EnsureFullMatrix().postScale(x, y, z);
}

void Transform::Translate(const Vector2dF& offset) {
  Translate(offset.x(), offset.y());
}

void Transform::Translate(double x, double y) {
  if (LIKELY(!matrix_))
    axis_2d_.PreTranslate(Vector2dF(x, y));
  else
    matrix_->preTranslate(x, y, 0);
}

void Transform::PostTranslate(const Vector2dF& offset) {
  PostTranslate(offset.x(), offset.y());
}

void Transform::PostTranslate(double x, double y) {
  if (LIKELY(!matrix_))
    axis_2d_.PostTranslate(Vector2dF(x, y));
  else
    matrix_->postTranslate(x, y, 0);
}

void Transform::PostTranslate3d(const Vector3dF& offset) {
  PostTranslate3d(offset.x(), offset.y(), offset.z());
}

void Transform::PostTranslate3d(double x, double y, double z) {
  if (z == 0)
    PostTranslate(x, y);
  else
    EnsureFullMatrix().postTranslate(x, y, z);
}

void Transform::Translate3d(const Vector3dF& offset) {
  Translate3d(offset.x(), offset.y(), offset.z());
}

void Transform::Translate3d(double x, double y, double z) {
  if (z == 0)
    Translate(x, y);
  else
    EnsureFullMatrix().preTranslate(x, y, z);
}

void Transform::Skew(double angle_x, double angle_y) {
  if (!angle_x && !angle_y)
    return;

  auto& matrix = EnsureFullMatrix();
  if (matrix.isIdentity()) {
    matrix.setRC(0, 1, TanDegrees(angle_x));
    matrix.setRC(1, 0, TanDegrees(angle_y));
  } else {
    Matrix44 skew;
    skew.setRC(0, 1, TanDegrees(angle_x));
    skew.setRC(1, 0, TanDegrees(angle_y));
    matrix.preConcat(skew);
  }
}

void Transform::ApplyPerspectiveDepth(double depth) {
  if (depth == 0)
    return;

  auto& matrix = EnsureFullMatrix();
  if (matrix.isIdentity()) {
    matrix.setRC(3, 2, -1.0 / depth);
  } else {
    Matrix44 m;
    m.setRC(3, 2, -1.0 / depth);
    matrix.preConcat(m);
  }
}

void Transform::PreConcat(const Transform& transform) {
  if (LIKELY(!transform.matrix_)) {
    PreConcat(transform.axis_2d_);
  } else {
    EnsureFullMatrix().preConcat(*transform.matrix_);
  }
}

void Transform::PostConcat(const Transform& transform) {
  if (LIKELY(!transform.matrix_)) {
    PostConcat(transform.axis_2d_);
  } else {
    EnsureFullMatrix().postConcat(*transform.matrix_);
  }
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
  if (LIKELY(!matrix_)) {
    return ApproximatelyOne(axis_2d_.scale().x(), tolerance) &&
           ApproximatelyOne(axis_2d_.scale().y(), tolerance);
  }

  return ApproximatelyOne(matrix_->rc(0, 0), tolerance) &&
         ApproximatelyZero(matrix_->rc(1, 0), tolerance) &&
         ApproximatelyZero(matrix_->rc(2, 0), tolerance) &&
         matrix_->rc(3, 0) == 0 &&
         ApproximatelyZero(matrix_->rc(0, 1), tolerance) &&
         ApproximatelyOne(matrix_->rc(1, 1), tolerance) &&
         ApproximatelyZero(matrix_->rc(2, 1), tolerance) &&
         matrix_->rc(3, 1) == 0 &&
         ApproximatelyZero(matrix_->rc(0, 2), tolerance) &&
         ApproximatelyZero(matrix_->rc(1, 2), tolerance) &&
         ApproximatelyOne(matrix_->rc(2, 2), tolerance) &&
         matrix_->rc(3, 2) == 0 && matrix_->rc(3, 3) == 1;
}

bool Transform::IsApproximatelyIdentityOrIntegerTranslation(
    double tolerance) const {
  if (!IsApproximatelyIdentityOrTranslation(tolerance))
    return false;

  if (LIKELY(!matrix_)) {
    for (float t : {axis_2d_.translation().x(), axis_2d_.translation().y()}) {
      if (!base::IsValueInRangeForNumericType<int>(t) ||
          std::abs(std::round(t) - t) > tolerance)
        return false;
    }
    return true;
  }

  for (double t : {matrix_->rc(0, 3), matrix_->rc(1, 3), matrix_->rc(2, 3)}) {
    if (!base::IsValueInRangeForNumericType<int>(t) ||
        std::abs(std::round(t) - t) > tolerance)
      return false;
  }
  return true;
}

bool Transform::IsIdentityOrIntegerTranslation() const {
  if (!IsIdentityOrTranslation())
    return false;

  if (LIKELY(!matrix_)) {
    for (float t : {axis_2d_.translation().x(), axis_2d_.translation().y()}) {
      if (!base::IsValueInRangeForNumericType<int>(t) ||
          static_cast<int>(t) != t) {
        return false;
      }
    }
    return true;
  }

  for (double t : {matrix_->rc(0, 3), matrix_->rc(1, 3), matrix_->rc(2, 3)}) {
    if (!base::IsValueInRangeForNumericType<int>(t) || static_cast<int>(t) != t)
      return false;
  }
  return true;
}

bool Transform::IsBackFaceVisible() const {
  if (LIKELY(!matrix_))
    return false;

  // Compute whether a layer with a forward-facing normal of (0, 0, 1, 0)
  // would have its back face visible after applying the transform.
  if (matrix_->isIdentity())
    return false;

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

  double determinant = matrix_->determinant();

  // If matrix was not invertible, then just assume back face is not visible.
  if (determinant == 0)
    return false;

  // Compute the cofactor of the 3rd row, 3rd column.
  double cofactor_part_1 =
      matrix_->rc(0, 0) * matrix_->rc(1, 1) * matrix_->rc(3, 3);

  double cofactor_part_2 =
      matrix_->rc(0, 1) * matrix_->rc(1, 3) * matrix_->rc(3, 0);

  double cofactor_part_3 =
      matrix_->rc(0, 3) * matrix_->rc(1, 0) * matrix_->rc(3, 1);

  double cofactor_part_4 =
      matrix_->rc(0, 0) * matrix_->rc(1, 3) * matrix_->rc(3, 1);

  double cofactor_part_5 =
      matrix_->rc(0, 1) * matrix_->rc(1, 0) * matrix_->rc(3, 3);

  double cofactor_part_6 =
      matrix_->rc(0, 3) * matrix_->rc(1, 1) * matrix_->rc(3, 0);

  double cofactor33 = cofactor_part_1 + cofactor_part_2 + cofactor_part_3 -
                      cofactor_part_4 - cofactor_part_5 - cofactor_part_6;

  // Technically the transformed z component is cofactor33 / determinant.  But
  // we can avoid the costly division because we only care about the resulting
  // +/- sign; we can check this equivalently by multiplication.
  return cofactor33 * determinant < -kEpsilon;
}

bool Transform::GetInverse(Transform* transform) const {
  if (LIKELY(!matrix_)) {
    transform->matrix_ = nullptr;
    if (axis_2d_.IsInvertible()) {
      transform->axis_2d_ = axis_2d_;
      transform->axis_2d_.Invert();
      return true;
    }
    transform->axis_2d_ = AxisTransform2d();
    return false;
  }

  if (transform != this) {
    transform->matrix_ =
        std::make_unique<Matrix44>(Matrix44::kUninitialized_Constructor);
  }
  if (matrix_->invert(transform->matrix_.get()))
    return true;

  // Initialize the return value to identity if this matrix turned
  // out to be un-invertible.
  transform->MakeIdentity();
  return false;
}

bool Transform::Preserves2dAxisAlignment() const {
  if (LIKELY(!matrix_))
    return true;

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

  bool has_x_or_y_perspective =
      matrix_->rc(3, 0) != 0 || matrix_->rc(3, 1) != 0;

  int num_non_zero_in_row_0 = 0;
  int num_non_zero_in_row_1 = 0;
  int num_non_zero_in_col_0 = 0;
  int num_non_zero_in_col_1 = 0;

  if (std::abs(matrix_->rc(0, 0)) > kEpsilon) {
    num_non_zero_in_row_0++;
    num_non_zero_in_col_0++;
  }

  if (std::abs(matrix_->rc(0, 1)) > kEpsilon) {
    num_non_zero_in_row_0++;
    num_non_zero_in_col_1++;
  }

  if (std::abs(matrix_->rc(1, 0)) > kEpsilon) {
    num_non_zero_in_row_1++;
    num_non_zero_in_col_0++;
  }

  if (std::abs(matrix_->rc(1, 1)) > kEpsilon) {
    num_non_zero_in_row_1++;
    num_non_zero_in_col_1++;
  }

  return num_non_zero_in_row_0 <= 1 && num_non_zero_in_row_1 <= 1 &&
         num_non_zero_in_col_0 <= 1 && num_non_zero_in_col_1 <= 1 &&
         !has_x_or_y_perspective;
}

bool Transform::NonDegeneratePreserves2dAxisAlignment() const {
  if (LIKELY(!matrix_))
    return axis_2d_.scale().x() > kEpsilon && axis_2d_.scale().y() > kEpsilon;

  // See comments above for Preserves2dAxisAlignment.

  // This function differs from it by requiring:
  //  (1) that there are exactly two nonzero values on a diagonal in
  //      the upper left 2x2 submatrix, and
  //  (2) that the w perspective value is positive.

  bool has_x_or_y_perspective =
      matrix_->rc(3, 0) != 0 || matrix_->rc(3, 1) != 0;
  bool positive_w_perspective = matrix_->rc(3, 3) > kEpsilon;

  bool have_0_0 = std::abs(matrix_->rc(0, 0)) > kEpsilon;
  bool have_0_1 = std::abs(matrix_->rc(0, 1)) > kEpsilon;
  bool have_1_0 = std::abs(matrix_->rc(1, 0)) > kEpsilon;
  bool have_1_1 = std::abs(matrix_->rc(1, 1)) > kEpsilon;

  return have_0_0 == have_1_1 && have_0_1 == have_1_0 && have_0_0 != have_0_1 &&
         !has_x_or_y_perspective && positive_w_perspective;
}

void Transform::Transpose() {
  if (!IsScale())
    EnsureFullMatrix().transpose();
}

void Transform::FlattenTo2d() {
  if (LIKELY(!matrix_))
    return;
  matrix_->FlattenTo2d();
  DCHECK(IsFlat());
}

bool Transform::IsFlat() const {
  if (LIKELY(!matrix_))
    return true;
  return matrix_->rc(2, 0) == 0.0 && matrix_->rc(2, 1) == 0.0 &&
         matrix_->rc(0, 2) == 0.0 && matrix_->rc(1, 2) == 0.0 &&
         matrix_->rc(2, 2) == 1.0 && matrix_->rc(3, 2) == 0.0 &&
         matrix_->rc(2, 3) == 0.0;
}

Vector2dF Transform::To2dTranslation() const {
  if (LIKELY(!matrix_)) {
    return Vector2dF(ClampFloatGeometry(axis_2d_.translation().x()),
                     ClampFloatGeometry(axis_2d_.translation().y()));
  }
  return Vector2dF(ClampFloatGeometry(matrix_->rc(0, 3)),
                   ClampFloatGeometry(matrix_->rc(1, 3)));
}

Vector2dF Transform::To2dScale() const {
  if (LIKELY(!matrix_)) {
    return Vector2dF(ClampFloatGeometry(axis_2d_.scale().x()),
                     ClampFloatGeometry(axis_2d_.scale().y()));
  }
  return Vector2dF(ClampFloatGeometry(matrix_->rc(0, 0)),
                   ClampFloatGeometry(matrix_->rc(1, 1)));
}

Point Transform::MapPoint(const Point& point) const {
  return gfx::ToRoundedPoint(MapPoint(gfx::PointF(point)));
}

PointF Transform::MapPoint(const PointF& point) const {
  return LIKELY(!matrix_)
             ? axis_2d_.MapPoint(point)
             : MapPointInternal(*matrix_, Point3F(point)).AsPointF();
}

Point3F Transform::MapPoint(const Point3F& point) const {
  if (LIKELY(!matrix_)) {
    PointF result = axis_2d_.MapPoint(point.AsPointF());
    return Point3F(result.x(), result.y(), ClampFloatGeometry(point.z()));
  }
  return MapPointInternal(*matrix_, point);
}

Vector3dF Transform::MapVector(const Vector3dF& vector) const {
  if (LIKELY(!matrix_)) {
    return Vector3dF(ClampFloatGeometry(vector.x() * axis_2d_.scale().x()),
                     ClampFloatGeometry(vector.y() * axis_2d_.scale().y()),
                     ClampFloatGeometry(vector.z()));
  }
  double p[4] = {vector.x(), vector.y(), vector.z(), 0};
  matrix_->mapScalars(p);
  return Vector3dF(ClampFloatGeometry(p[0]), ClampFloatGeometry(p[1]),
                   ClampFloatGeometry(p[2]));
}

void Transform::TransformVector4(float vector[4]) const {
  DCHECK(vector);
  if (LIKELY(!matrix_)) {
    vector[0] = vector[0] * axis_2d_.scale().x() +
                vector[3] * axis_2d_.translation().x();
    vector[1] = vector[1] * axis_2d_.scale().y() +
                vector[3] * axis_2d_.translation().y();
    for (int i = 0; i < 4; i++)
      vector[i] = ClampFloatGeometry(vector[i]);
  } else {
    double v[4] = {vector[0], vector[1], vector[2], vector[3]};
    matrix_->mapScalars(v);
    for (int i = 0; i < 4; i++)
      vector[i] = ClampFloatGeometry(v[i]);
  }
}

void Transform::TransformVector4(double vector[4]) const {
  DCHECK(vector);
  // For now the only caller doesn't need clamping.
  // TODO(crbug.com/1359528): Revisit the clamping requirement.
  if (LIKELY(!matrix_)) {
    vector[0] = vector[0] * axis_2d_.scale().x() +
                vector[3] * axis_2d_.translation().x();
    vector[1] = vector[1] * axis_2d_.scale().y() +
                vector[3] * axis_2d_.translation().y();
  } else {
    matrix_->mapScalars(vector);
  }
}

absl::optional<PointF> Transform::InverseMapPoint(const PointF& point) const {
  if (LIKELY(!matrix_)) {
    if (!axis_2d_.IsInvertible())
      return absl::nullopt;
    return axis_2d_.InverseMapPoint(point);
  }
  Matrix44 inverse(Matrix44::kUninitialized_Constructor);
  if (!matrix_->invert(&inverse))
    return absl::nullopt;
  return MapPointInternal(inverse, Point3F(point)).AsPointF();
}

absl::optional<Point> Transform::InverseMapPoint(const Point& point) const {
  if (absl::optional<PointF> point_f = InverseMapPoint(PointF(point)))
    return ToRoundedPoint(*point_f);
  return absl::nullopt;
}

absl::optional<Point3F> Transform::InverseMapPoint(const Point3F& point) const {
  if (LIKELY(!matrix_)) {
    if (!axis_2d_.IsInvertible())
      return absl::nullopt;
    PointF result = axis_2d_.InverseMapPoint(point.AsPointF());
    return Point3F(result.x(), result.y(), ClampFloatGeometry(point.z()));
  }
  Matrix44 inverse(Matrix44::kUninitialized_Constructor);
  if (!matrix_->invert(&inverse))
    return absl::nullopt;
  return absl::make_optional(MapPointInternal(inverse, point));
}

RectF Transform::MapRect(const RectF& rect) const {
  if (IsIdentity())
    return rect;

  if (LIKELY(!matrix_) && axis_2d_.scale().x() >= 0 &&
      axis_2d_.scale().y() >= 0) {
    return axis_2d_.MapRect(rect);
  }

  // TODO(crbug.com/1359528): Use local implementation.
  SkRect src = RectFToSkRect(rect);
  TransformToFlattenedSkMatrix(*this).mapRect(&src);
  return RectF(ClampFloatGeometry(src.x()), ClampFloatGeometry(src.y()),
               ClampFloatGeometry(src.width()),
               ClampFloatGeometry(src.height()));
}

Rect Transform::MapRect(const Rect& rect) const {
  if (IsIdentity())
    return rect;

  return ToEnclosingRect(MapRect(RectF(rect)));
}

absl::optional<RectF> Transform::InverseMapRect(const RectF& rect) const {
  if (IsIdentity())
    return rect;

  if (LIKELY(!matrix_)) {
    if (!axis_2d_.IsInvertible())
      return absl::nullopt;
    if (axis_2d_.scale().x() > 0 && axis_2d_.scale().y() > 0)
      return axis_2d_.InverseMapRect(rect);
  }

  Transform inverse(kSkipInitialization);
  if (!GetInverse(&inverse))
    return absl::nullopt;

  // TODO(crbug.com/1359528): Use local implementation and clamp the results.
  SkRect src = RectFToSkRect(rect);
  TransformToFlattenedSkMatrix(inverse).mapRect(&src);
  return RectF(ClampFloatGeometry(src.x()), ClampFloatGeometry(src.y()),
               ClampFloatGeometry(src.width()),
               ClampFloatGeometry(src.height()));
}

absl::optional<Rect> Transform::InverseMapRect(const Rect& rect) const {
  if (IsIdentity())
    return rect;

  if (absl::optional<RectF> mapped = InverseMapRect(RectF(rect)))
    return ToEnclosingRect(mapped.value());
  return absl::nullopt;
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

bool Transform::Blend(const Transform& from, double progress) {
  DecomposedTransform to_decomp;
  DecomposedTransform from_decomp;
  if (!DecomposeTransform(&to_decomp, *this) ||
      !DecomposeTransform(&from_decomp, from))
    return false;

  to_decomp = BlendDecomposedTransforms(to_decomp, from_decomp, progress);

  *this = ComposeTransform(to_decomp);
  return true;
}

void Transform::RoundTranslationComponents() {
  if (LIKELY(!matrix_)) {
    axis_2d_ = AxisTransform2d::FromScaleAndTranslation(
        axis_2d_.scale(), Vector2dF(std::round(axis_2d_.translation().x()),
                                    std::round(axis_2d_.translation().y())));
  } else {
    matrix_->setRC(0, 3, std::round(matrix_->rc(0, 3)));
    matrix_->setRC(1, 3, std::round(matrix_->rc(1, 3)));
  }
}

Point3F Transform::MapPointInternal(const Matrix44& xform,
                                    const Point3F& point) const {
  DCHECK(matrix_);

  double p[4] = {point.x(), point.y(), point.z(), 1};

  xform.mapScalars(p);

  if (p[3] != 1.0 && std::isnormal(p[3])) {
    float w_inverse = 1.0 / p[3];
    return Point3F(ClampFloatGeometry(p[0] * w_inverse),
                   ClampFloatGeometry(p[1] * w_inverse),
                   ClampFloatGeometry(p[2] * w_inverse));
  }
  return Point3F(ClampFloatGeometry(p[0]), ClampFloatGeometry(p[1]),
                 ClampFloatGeometry(p[2]));
}

bool Transform::ApproximatelyEqual(const gfx::Transform& transform) const {
  auto approximately_equal = [](float a, float b) {
    return std::abs(a - b) <= 0.1f;
  };

  // We may have a larger discrepancy in the scroll components due to snapping
  // (floating point error might round the other way).
  auto translation_approximately_equal = [](float a, float b) {
    return std::abs(a - b) <= 1.f;
  };

  if (LIKELY(!matrix_) && LIKELY(!transform.matrix_)) {
    return approximately_equal(axis_2d_.scale().x(),
                               transform.axis_2d_.scale().x()) &&
           approximately_equal(axis_2d_.scale().y(),
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
      auto predicate = col == 3 && row < 3 ? translation_approximately_equal
                                           : approximately_equal;
      if (!predicate(rc(row, col), transform.rc(row, col)))
        return false;
    }
  }

  return true;
}

std::string Transform::ToString() const {
  return base::StringPrintf(
      "[ %+0.4f %+0.4f %+0.4f %+0.4f  \n"
      "  %+0.4f %+0.4f %+0.4f %+0.4f  \n"
      "  %+0.4f %+0.4f %+0.4f %+0.4f  \n"
      "  %+0.4f %+0.4f %+0.4f %+0.4f ]\n",
      rc(0, 0), rc(0, 1), rc(0, 2), rc(0, 3), rc(1, 0), rc(1, 1), rc(1, 2),
      rc(1, 3), rc(2, 0), rc(2, 1), rc(2, 2), rc(2, 3), rc(3, 0), rc(3, 1),
      rc(3, 2), rc(3, 3));
}

}  // namespace gfx
