// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/transform.h"

#include "base/check_op.h"
#include "base/strings/stringprintf.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/clamp_float_geometry.h"
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

const SkScalar kEpsilon = std::numeric_limits<float>::epsilon();

SkScalar TanDegrees(double degrees) {
  return SkDoubleToScalar(std::tan(gfx::DegToRad(degrees)));
}

inline bool ApproximatelyZero(SkScalar x, SkScalar tolerance) {
  return std::abs(x) <= tolerance;
}

inline bool ApproximatelyOne(SkScalar x, SkScalar tolerance) {
  return std::abs(x - 1) <= tolerance;
}

}  // namespace

// clang-format off
Transform::Transform(SkScalar r0c0, SkScalar r1c0, SkScalar r2c0, SkScalar r3c0,
                     SkScalar r0c1, SkScalar r1c1, SkScalar r2c1, SkScalar r3c1,
                     SkScalar r0c2, SkScalar r1c2, SkScalar r2c2, SkScalar r3c2,
                     SkScalar r0c3, SkScalar r1c3, SkScalar r2c3, SkScalar r3c3)
    // The parameters of SkMatrix's constructor is in row-major order.
    : matrix_(r0c0, r0c1, r0c2, r0c3,     // row 0
              r1c0, r1c1, r1c2, r1c3,     // row 1
              r2c0, r2c1, r2c2, r2c3,     // row 2
              r3c0, r3c1, r3c2, r3c3) {}  // row 3

Transform::Transform(const Quaternion& q)
    : matrix_(
        // Row 1.
        SkDoubleToScalar(1.0 - 2.0 * (q.y() * q.y() + q.z() * q.z())),
        SkDoubleToScalar(2.0 * (q.x() * q.y() - q.z() * q.w())),
        SkDoubleToScalar(2.0 * (q.x() * q.z() + q.y() * q.w())),
        0,
        // Row 2.
        SkDoubleToScalar(2.0 * (q.x() * q.y() + q.z() * q.w())),
        SkDoubleToScalar(1.0 - 2.0 * (q.x() * q.x() + q.z() * q.z())),
        SkDoubleToScalar(2.0 * (q.y() * q.z() - q.x() * q.w())),
        0,
        // Row 3.
        SkDoubleToScalar(2.0 * (q.x() * q.z() - q.y() * q.w())),
        SkDoubleToScalar(2.0 * (q.y() * q.z() + q.x() * q.w())),
        SkDoubleToScalar(1.0 - 2.0 * (q.x() * q.x() + q.y() * q.y())),
        0,
        // row 4.
        0, 0, 0, 1) {}
// clang-format on

// static
Transform Transform::ColMajorF(const float a[16]) {
  Transform t(kSkipInitialization);
  t.matrix_.setColMajor(a);
  return t;
}

void Transform::GetColMajorF(float a[16]) const {
  matrix_.getColMajor(a);
}

void Transform::RotateAboutXAxis(double degrees) {
  double radians = gfx::DegToRad(degrees);
  double sin_angle = std::sin(radians);
  double cos_angle = std::cos(radians);
  Transform t(kSkipInitialization);
  t.matrix_.setRotateAboutXAxisSinCos(SkDoubleToScalar(sin_angle),
                                      SkDoubleToScalar(cos_angle));
  PreConcat(t);
}

void Transform::RotateAboutYAxis(double degrees) {
  double radians = gfx::DegToRad(degrees);
  double sin_angle = std::sin(radians);
  double cos_angle = std::cos(radians);
  Transform t(kSkipInitialization);
  t.matrix_.setRotateAboutYAxisSinCos(SkDoubleToScalar(sin_angle),
                                      SkDoubleToScalar(cos_angle));
  PreConcat(t);
}

void Transform::RotateAboutZAxis(double degrees) {
  double radians = gfx::DegToRad(degrees);
  double sin_angle = std::sin(radians);
  double cos_angle = std::cos(radians);
  Transform t(kSkipInitialization);
  t.matrix_.setRotateAboutZAxisSinCos(SkDoubleToScalar(sin_angle),
                                      SkDoubleToScalar(cos_angle));
  PreConcat(t);
}

void Transform::RotateAbout(const Vector3dF& axis, double degrees) {
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
  t.matrix_.setRotateUnitSinCos(
      SkDoubleToScalar(x), SkDoubleToScalar(y), SkDoubleToScalar(z),
      SkDoubleToScalar(sin_angle), SkDoubleToScalar(cos_angle));
  PreConcat(t);
}

double Transform::Determinant() const {
  return matrix_.determinant();
}

void Transform::Scale(SkScalar x, SkScalar y) {
  matrix_.preScale(x, y, 1);
}

void Transform::PostScale(SkScalar x, SkScalar y) {
  matrix_.postScale(x, y, 1);
}

void Transform::Scale3d(SkScalar x, SkScalar y, SkScalar z) {
  matrix_.preScale(x, y, z);
}

void Transform::PostScale3d(SkScalar x, SkScalar y, SkScalar z) {
  matrix_.postScale(x, y, z);
}

void Transform::Translate(const Vector2dF& offset) {
  Translate(offset.x(), offset.y());
}

void Transform::Translate(SkScalar x, SkScalar y) {
  matrix_.preTranslate(x, y, 0);
}

void Transform::PostTranslate(const Vector2dF& offset) {
  PostTranslate(offset.x(), offset.y());
}

void Transform::PostTranslate(SkScalar x, SkScalar y) {
  matrix_.postTranslate(x, y, 0);
}

void Transform::PostTranslate3d(const Vector3dF& offset) {
  PostTranslate3d(offset.x(), offset.y(), offset.z());
}

void Transform::PostTranslate3d(SkScalar x, SkScalar y, SkScalar z) {
  matrix_.postTranslate(x, y, z);
}

void Transform::Translate3d(const Vector3dF& offset) {
  Translate3d(offset.x(), offset.y(), offset.z());
}

void Transform::Translate3d(SkScalar x, SkScalar y, SkScalar z) {
  matrix_.preTranslate(x, y, z);
}

void Transform::Skew(double angle_x, double angle_y) {
  if (matrix_.isIdentity()) {
    matrix_.setRC(0, 1, TanDegrees(angle_x));
    matrix_.setRC(1, 0, TanDegrees(angle_y));
  } else {
    Matrix44 skew;
    skew.setRC(0, 1, TanDegrees(angle_x));
    skew.setRC(1, 0, TanDegrees(angle_y));
    matrix_.preConcat(skew);
  }
}

void Transform::ApplyPerspectiveDepth(SkScalar depth) {
  if (depth == 0)
    return;
  if (matrix_.isIdentity()) {
    matrix_.setRC(3, 2, -SK_Scalar1 / depth);
  } else {
    Matrix44 m;
    m.setRC(3, 2, -SK_Scalar1 / depth);
    matrix_.preConcat(m);
  }
}

void Transform::PreConcat(const Transform& transform) {
  matrix_.preConcat(transform.matrix_);
}

void Transform::PostConcat(const Transform& transform) {
  matrix_.postConcat(transform.matrix_);
}

void Transform::PreConcat(const AxisTransform2d& transform) {
  Translate(transform.translation());
  Scale(transform.scale().x(), transform.scale().y());
}

void Transform::PostConcat(const AxisTransform2d& transform) {
  PostScale(transform.scale().x(), transform.scale().y());
  PostTranslate(transform.translation());
}

bool Transform::IsApproximatelyIdentityOrTranslation(SkScalar tolerance) const {
  DCHECK_GE(tolerance, 0);
  return ApproximatelyOne(matrix_.rc(0, 0), tolerance) &&
         ApproximatelyZero(matrix_.rc(1, 0), tolerance) &&
         ApproximatelyZero(matrix_.rc(2, 0), tolerance) &&
         matrix_.rc(3, 0) == 0 &&
         ApproximatelyZero(matrix_.rc(0, 1), tolerance) &&
         ApproximatelyOne(matrix_.rc(1, 1), tolerance) &&
         ApproximatelyZero(matrix_.rc(2, 1), tolerance) &&
         matrix_.rc(3, 1) == 0 &&
         ApproximatelyZero(matrix_.rc(0, 2), tolerance) &&
         ApproximatelyZero(matrix_.rc(1, 2), tolerance) &&
         ApproximatelyOne(matrix_.rc(2, 2), tolerance) &&
         matrix_.rc(3, 2) == 0 && matrix_.rc(3, 3) == 1;
}

bool Transform::IsApproximatelyIdentityOrIntegerTranslation(
    SkScalar tolerance) const {
  if (!IsApproximatelyIdentityOrTranslation(tolerance))
    return false;

  for (float t : {matrix_.rc(0, 3), matrix_.rc(1, 3), matrix_.rc(2, 3)}) {
    if (!base::IsValueInRangeForNumericType<int>(t) ||
        std::abs(std::round(t) - t) > tolerance)
      return false;
  }
  return true;
}

bool Transform::IsIdentityOrIntegerTranslation() const {
  if (!IsIdentityOrTranslation())
    return false;

  for (float t : {matrix_.rc(0, 3), matrix_.rc(1, 3), matrix_.rc(2, 3)}) {
    if (!base::IsValueInRangeForNumericType<int>(t) || static_cast<int>(t) != t)
      return false;
  }
  return true;
}

bool Transform::IsBackFaceVisible() const {
  // Compute whether a layer with a forward-facing normal of (0, 0, 1, 0)
  // would have its back face visible after applying the transform.
  if (matrix_.isIdentity())
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

  double determinant = matrix_.determinant();

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
  if (!matrix_.invert(&transform->matrix_)) {
    // Initialize the return value to identity if this matrix turned
    // out to be un-invertible.
    transform->MakeIdentity();
    return false;
  }

  return true;
}

bool Transform::Preserves2dAxisAlignment() const {
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
  matrix_.transpose();
}

void Transform::FlattenTo2d() {
  matrix_.FlattenTo2d();
  DCHECK(IsFlat());
}

bool Transform::IsFlat() const {
  return matrix_.rc(2, 0) == 0.0 && matrix_.rc(2, 1) == 0.0 &&
         matrix_.rc(0, 2) == 0.0 && matrix_.rc(1, 2) == 0.0 &&
         matrix_.rc(2, 2) == 1.0 && matrix_.rc(3, 2) == 0.0 &&
         matrix_.rc(2, 3) == 0.0;
}

Vector2dF Transform::To2dTranslation() const {
  return gfx::Vector2dF(ClampFloatGeometry(matrix_.rc(0, 3)),
                        ClampFloatGeometry(matrix_.rc(1, 3)));
}

Vector2dF Transform::To2dScale() const {
  return gfx::Vector2dF(ClampFloatGeometry(matrix_.rc(0, 0)),
                        ClampFloatGeometry(matrix_.rc(1, 1)));
}

Point Transform::MapPoint(const Point& point) const {
  return MapPointInternal(matrix_, point);
}

PointF Transform::MapPoint(const PointF& point) const {
  return MapPointInternal(matrix_, point);
}

Point3F Transform::MapPoint(const Point3F& point) const {
  return MapPointInternal(matrix_, point);
}

Vector3dF Transform::MapVector(const Vector3dF& vector) const {
  if (IsIdentity())
    return vector;

  SkScalar p[4] = {vector.x(), vector.y(), vector.z(), 0};
  matrix_.mapScalars(p);
  return Vector3dF(ClampFloatGeometry(p[0]), ClampFloatGeometry(p[1]),
                   ClampFloatGeometry(p[2]));
}

void Transform::TransformVector4(float vector[4]) const {
  DCHECK(vector);
  matrix_.mapScalars(vector);
  for (int i = 0; i < 4; i++)
    vector[i] = ClampFloatGeometry(vector[i]);
}

absl::optional<PointF> Transform::InverseMapPoint(const PointF& point) const {
  // TODO(sad): Try to avoid trying to invert the matrix.
  Matrix44 inverse(Matrix44::kUninitialized_Constructor);
  if (!matrix_.invert(&inverse))
    return absl::nullopt;
  return absl::make_optional(MapPointInternal(inverse, point));
}

absl::optional<Point> Transform::InverseMapPoint(const Point& point) const {
  // TODO(sad): Try to avoid trying to invert the matrix.
  Matrix44 inverse(Matrix44::kUninitialized_Constructor);
  if (!matrix_.invert(&inverse))
    return absl::nullopt;
  return absl::make_optional(MapPointInternal(inverse, point));
}

absl::optional<Point3F> Transform::InverseMapPoint(const Point3F& point) const {
  // TODO(sad): Try to avoid trying to invert the matrix.
  Matrix44 inverse(Matrix44::kUninitialized_Constructor);
  if (!matrix_.invert(&inverse))
    return absl::nullopt;
  return absl::make_optional(MapPointInternal(inverse, point));
}

RectF Transform::MapRect(const RectF& rect) const {
  if (IsIdentity())
    return rect;

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
  matrix_.setRC(0, 3, std::round(matrix_.rc(0, 3)));
  matrix_.setRC(1, 3, std::round(matrix_.rc(1, 3)));
}

Point3F Transform::MapPointInternal(const Matrix44& xform,
                                    const Point3F& point) const {
  if (xform.isIdentity())
    return point;

  SkScalar p[4] = {point.x(), point.y(), point.z(), 1};

  xform.mapScalars(p);

  if (p[3] != SK_Scalar1 && std::isnormal(p[3])) {
    float w_inverse = SK_Scalar1 / p[3];
    return gfx::Point3F(ClampFloatGeometry(p[0] * w_inverse),
                        ClampFloatGeometry(p[1] * w_inverse),
                        ClampFloatGeometry(p[2] * w_inverse));
  }
  return gfx::Point3F(ClampFloatGeometry(p[0]), ClampFloatGeometry(p[1]),
                      ClampFloatGeometry(p[2]));
}

PointF Transform::MapPointInternal(const Matrix44& xform,
                                   const PointF& point) const {
  if (xform.isIdentity())
    return point;

  SkScalar p[4] = {SkIntToScalar(point.x()), SkIntToScalar(point.y()), 0, 1};
  xform.mapScalars(p);

  if (p[3] != SK_Scalar1 && std::isnormal(p[3])) {
    float w_inverse = SK_Scalar1 / p[3];
    return gfx::PointF(ClampFloatGeometry(p[0] * w_inverse),
                       ClampFloatGeometry(p[1] * w_inverse));
  }
  return gfx::PointF(ClampFloatGeometry(p[0]), ClampFloatGeometry(p[1]));
}

Point Transform::MapPointInternal(const Matrix44& xform,
                                  const Point& point) const {
  return ToRoundedPoint(MapPointInternal(xform, PointF(point)));
}

bool Transform::ApproximatelyEqual(const gfx::Transform& transform) const {
  static const float component_tolerance = 0.1f;

  // We may have a larger discrepancy in the scroll components due to snapping
  // (floating point error might round the other way).
  static const float translation_tolerance = 1.f;

  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 4; col++) {
      const float delta = std::abs(rc(row, col) - transform.rc(row, col));
      const float tolerance =
          col == 3 && row < 3 ? translation_tolerance : component_tolerance;
      if (delta > tolerance)
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
