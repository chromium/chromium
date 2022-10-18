// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_TRANSFORM_H_
#define UI_GFX_GEOMETRY_TRANSFORM_H_

#include <iosfwd>
#include <memory>
#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/geometry_skia_export.h"
#include "ui/gfx/geometry/matrix44.h"

namespace gfx {

class AxisTransform2d;
class BoxF;
class Rect;
class RectF;
class Point;
class PointF;
class Point3F;
class Quaternion;
class Vector2dF;
class Vector3dF;

// 4x4 Transformation matrix. Depending on the complexity of the matrix, it may
// be internally stored as an AxisTransform2d or a full 4x4 matrix.
class GEOMETRY_SKIA_EXPORT Transform {
 public:
  Transform();
  ~Transform();

  // TODO(crbug.com/1359528): This is same as Transform(). Remove this.
  enum SkipInitialization { kSkipInitialization };
  explicit Transform(SkipInitialization);

  Transform(const Transform& rhs);
  Transform& operator=(const Transform& rhs);
  Transform(Transform&&);
  Transform& operator=(Transform&&);

  // Creates a transform from explicit 16 matrix elements in row-major order.
  static Transform RowMajor(double r0c0,
                            double r0c1,
                            double r0c2,
                            double r0c3,
                            double r1c0,
                            double r1c1,
                            double r1c2,
                            double r1c3,
                            double r2c0,
                            double r2c1,
                            double r2c2,
                            double r2c3,
                            double r3c0,
                            double r3c1,
                            double r3c2,
                            double r3c3) {
    return Transform(r0c0, r1c0, r2c0, r3c0,   // col 0
                     r0c1, r1c1, r2c1, r3c1,   // col 1
                     r0c2, r1c2, r2c2, r3c2,   // col 2
                     r0c3, r1c3, r2c3, r3c3);  // col 3
  }

  // Creates a transform from explicit 16 matrix elements in col-major order.
  static Transform ColMajor(double r0c0,
                            double r1c0,
                            double r2c0,
                            double r3c0,
                            double r0c1,
                            double r1c1,
                            double r2c1,
                            double r3c1,
                            double r0c2,
                            double r1c2,
                            double r2c2,
                            double r3c2,
                            double r0c3,
                            double r1c3,
                            double r2c3,
                            double r3c3) {
    return Transform(r0c0, r1c0, r2c0, r3c0,   // col 0
                     r0c1, r1c1, r2c1, r3c1,   // col 1
                     r0c2, r1c2, r2c2, r3c2,   // col 2
                     r0c3, r1c3, r2c3, r3c3);  // col 3
  }

  // Creates a transform from explicit 2d elements. All other matrix elements
  // remain the same as the corresponding elements of an identity matrix.
  static Transform Affine(double a,    // a.k.a. r0c0 or scale_x
                          double b,    // a.k.a. r1c0 or tan(skew_y)
                          double c,    // a.k.a. r0c1 or tan(skew_x) 
                          double d,    // a.k.a  r1c1 or scale_y
                          double e,    // a.k.a  r0c3 or translation_x
                          double f) {  // a.k.a  r1c3 or translaiton_y
    return ColMajor(a, b, 0, 0, c, d, 0, 0, 0, 0, 1, 0, e, f, 0, 1);
  }

  // Constructs a transform corresponding to the given quaternion.
  explicit Transform(const Quaternion& q);

  // Creates a transform as a 2d translation.
  static Transform MakeTranslation(double tx, double ty) {
    return Transform(1, 1, tx, ty);
  }
  // Creates a transform as a 2d scale.
  static Transform MakeScale(double scale) { return MakeScale(scale, scale); }
  static Transform MakeScale(double sx, double sy) {
    return Transform(sx, sy, 0, 0);
  }
  // Accurately rotate by 90, 180 or 270 degrees about the z axis.
  static Transform Make90degRotation() { return Affine(0, 1, -1, 0, 0, 0); }
  static Transform Make180degRotation() { return MakeScale(-1); }
  static Transform Make270degRotation() { return Affine(0, -1, 1, 0, 0, 0); }

  // Returns a const reference to an identity transform. If you just need an
  // identity transform as a value, the default constructor is better.
  static const Transform& Identity();

  // Resets this transform to the identity transform.
  // TODO(crbug.com/1359528): Rename this to SetIdentity or remove it.
  void MakeIdentity() {
    matrix_ = nullptr;
    axis_2d_ = AxisTransform2d();
  }

  bool operator==(const Transform& rhs) const {
    if (LIKELY(!matrix_ && !rhs.matrix_))
      return axis_2d_ == rhs.axis_2d_;
    return GetFullMatrix() == rhs.GetFullMatrix();
  }
  bool operator!=(const Transform& rhs) const { return !(*this == rhs); }

  // Gets a value at |row|, |col| from the matrix.
  double rc(int row, int col) const {
    if (LIKELY(!matrix_)) {
      float m[4][4] = {{axis_2d_.scale().x(), 0, 0, axis_2d_.translation().x()},
                       {0, axis_2d_.scale().y(), 0, axis_2d_.translation().y()},
                       {0, 0, 1, 0},
                       {0, 0, 0, 1}};
      return m[row][col];
    }
    return matrix_->rc(row, col);
  }

  // Set a value in the matrix at |row|, |col|.
  void set_rc(int row, int col, double v) {
    EnsureFullMatrix().setRC(row, col, v);
  }

  // TODO(crbug.com/1359528): Add ColMajor()/GetColMajor() with double parameter
  // when we use double as the type of the components.
  static Transform ColMajorF(const float a[16]);
  void GetColMajorF(float a[16]) const;

  // Applies a transformation on the current transformation,
  // i.e. this = this * transform.
  // "Pre" here means |this| is before the operator in the expression.
  // Corresponds to DOMMatrix.multiplySelf().
  void PreConcat(const Transform& transform);

  // Applies a transformation on the current transformation,
  // i.e. this = transform * this.
  // Corresponds to DOMMatrix.preMultiplySelf() (note the difference about
  // "Pre" and "Post"). "Post" here means |this| is after the operator in the
  // expression.
  void PostConcat(const Transform& transform);

  // Applies a 2d-axis transform on the current transformation,
  // i.e. this = this * transform.
  void PreConcat(const AxisTransform2d& transform);

  // Applies a transformation on the current transformation,
  // i.e. this = transform * this.
  void PostConcat(const AxisTransform2d& transform);

  // Applies the current transformation on a scaling and assigns the result
  // to |this|, i.e. this = this * scaling.
  void Scale(double scale) { Scale(scale, scale); }
  void Scale(double x, double y);
  void Scale3d(double x, double y, double z);

  // Applies a scale to the current transformation and assigns the result to
  // |this|, i.e. this = scaling * this.
  void PostScale(double scale) { PostScale(scale, scale); }
  void PostScale(double x, double y);
  void PostScale3d(double x, double y, double z);

  // Applies the current transformation on a translation and assigns the result
  // to |this|, i.e. this = this * translation.
  void Translate(const Vector2dF& offset);
  void Translate(double x, double y);
  void Translate3d(const Vector3dF& offset);
  void Translate3d(double x, double y, double z);

  // Applies a translation to the current transformation and assigns the result
  // to |this|, i.e. this = translation * this.
  void PostTranslate(const Vector2dF& offset);
  void PostTranslate(double x, double y);
  void PostTranslate3d(const Vector3dF& offset);
  void PostTranslate3d(double x, double y, double z);

  // The following methods have the "Pre" semantics,
  // i.e. this = this * operation.

  // Applies the current transformation on a 2d rotation and assigns the result
  // to |this|, i.e. this = this * rotation.
  void Rotate(double degrees) { RotateAboutZAxis(degrees); }

  // Applies the current transformation on an axis-angle rotation and assigns
  // the result to |this|.
  void RotateAboutXAxis(double degrees);
  void RotateAboutYAxis(double degrees);
  void RotateAboutZAxis(double degrees);
  void RotateAbout(const Vector3dF& axis, double degrees);

  // Applies the current transformation on a skew and assigns the result
  // to |this|, i.e. this = this * skew.
  void Skew(double degrees_x, double degrees_y);
  void SkewX(double degrees) { Skew(degrees, 0); }
  void SkewY(double degrees) { Skew(0, degrees); }

  // Applies the current transformation on a perspective transform and assigns
  // the result to |this|.
  void ApplyPerspectiveDepth(double depth);

  // Returns true if this is the identity matrix.
  // This function modifies a mutable variable in |matrix_|.
  bool IsIdentity() const {
    return LIKELY(!matrix_) ? axis_2d_ == AxisTransform2d()
                            : matrix_->isIdentity();
  }

  // Returns true if the matrix is either identity or pure translation.
  bool IsIdentityOrTranslation() const {
    return LIKELY(!matrix_) ? axis_2d_.scale() == Vector2dF(1, 1)
                            : matrix_->isTranslate();
  }

  // Returns true if the matrix is either the identity or a 2d translation.
  bool IsIdentityOr2DTranslation() const {
    return LIKELY(!matrix_) ? axis_2d_.scale() == Vector2dF(1, 1)
                            : matrix_->isTranslate() && matrix_->rc(2, 3) == 0;
  }

  // Returns true if the matrix is either identity or pure translation,
  // allowing for an amount of inaccuracy as specified by the parameter.
  bool IsApproximatelyIdentityOrTranslation(double tolerance) const;
  bool IsApproximatelyIdentityOrIntegerTranslation(double tolerance) const;

  // Returns true if the matrix is either a positive scale and/or a translation.
  bool IsPositiveScaleOrTranslation() const {
    if (LIKELY(!matrix_))
      return axis_2d_.scale().x() > 0.0 && axis_2d_.scale().y() > 0.0;

    if (!matrix_->isScaleTranslate())
      return false;
    return matrix_->rc(0, 0) > 0.0 && matrix_->rc(1, 1) > 0.0 &&
           matrix_->rc(2, 2) > 0.0;
  }

  // Returns true if the matrix is identity or, if the matrix consists only
  // of a translation whose components can be represented as integers. Returns
  // false if the translation contains a fractional component or is too large to
  // fit in an integer.
  bool IsIdentityOrIntegerTranslation() const;

  // Returns true if the matrix has only scaling components.
  bool IsScale() const {
    return LIKELY(!matrix_) ? axis_2d_.translation().IsZero()
                            : matrix_->isScale();
  }

  // Returns true if the matrix has only x and y scaling components.
  bool IsScale2d() const {
    return LIKELY(!matrix_) ? axis_2d_.translation().IsZero()
                            : matrix_->isScale() && matrix_->rc(2, 2) == 1;
  }

  // Returns true if the matrix is has only scaling and translation components.
  bool IsScaleOrTranslation() const {
    return LIKELY(!matrix_) || matrix_->isScaleTranslate();
  }

  // Returns true if axis-aligned 2d rects will remain axis-aligned after being
  // transformed by this matrix.
  bool Preserves2dAxisAlignment() const;

  // Returns true if axis-aligned 2d rects will remain axis-aligned and not
  // clipped by perspective (w > 0) after being transformed by this matrix,
  // and distinct points in the x/y plane will remain distinct after being
  // transformed by this matrix and mapped back to the x/y plane.
  bool NonDegeneratePreserves2dAxisAlignment() const;

  // Returns true if the matrix has any perspective component that would
  // change the w-component of a homogeneous point.
  bool HasPerspective() const {
    return UNLIKELY(matrix_) && matrix_->hasPerspective();
  }

  // Returns true if this transform is non-singular.
  bool IsInvertible() const {
    return LIKELY(!matrix_) ? axis_2d_.IsInvertible()
                            : matrix_->invert(nullptr);
  }

  // Returns true if a layer with a forward-facing normal of (0, 0, 1) would
  // have its back side facing frontwards after applying the transform.
  bool IsBackFaceVisible() const;

  // Inverts the transform which is passed in. Returns true if successful, or
  // sets |transform| to the identify matrix on failure.
  [[nodiscard]] bool GetInverse(Transform* transform) const;

  // Transposes this transform in place.
  void Transpose();

  // Set 3rd row and 3rd column to (0, 0, 1, 0). Note that this flattening
  // operation is not quite the same as an orthographic projection and is
  // technically not a linear operation.
  //
  // One useful interpretation of doing this operation:
  //  - For x and y values, the new transform behaves effectively like an
  //    orthographic projection was added to the matrix sequence.
  //  - For z values, the new transform overrides any effect that the transform
  //    had on z, and instead it preserves the z value for any points that are
  //    transformed.
  //  - Because of linearity of transforms, this flattened transform also
  //    preserves the effect that any subsequent (multiplied from the right)
  //    transforms would have on z values.
  //
  void FlattenTo2d();

  // Returns true if the 3rd row and 3rd column are both (0, 0, 1, 0).
  bool IsFlat() const;

  // Returns the x and y translation components of the matrix, clamped with
  // ClampFloatGeometry().
  Vector2dF To2dTranslation() const;

  // Returns the x and y scale components of the matrix, clamped with
  // ClampFloatGeometry().
  Vector2dF To2dScale() const;

  // Returns the point with the transformation applied to |point|, clamped
  // with ClampFloatGeometry().
  [[nodiscard]] Point3F MapPoint(const Point3F& point) const;
  [[nodiscard]] PointF MapPoint(const PointF& point) const;
  [[nodiscard]] Point MapPoint(const Point& point) const;

  // Returns the vector with the transformation applied to |vector|, clamped
  // with ClampFloatGeometry(). It differs from MapPoint() by that the
  // translation and perspective components of the matrix are ignored.
  [[nodiscard]] Vector3dF MapVector(const Vector3dF& vector) const;

  // Applies the transformation to the vector. The results are clamped with
  // ClampFloatGeometry().
  void TransformVector4(float vector[4]) const;
  void TransformVector4(double vector[4]) const;

  // Returns the point with reverse transformation applied to `point`, clamped
  // with ClampFloatGeometry(), or `absl::nullopt` if the transformation cannot
  // be inverted.
  [[nodiscard]] absl::optional<PointF> InverseMapPoint(
      const PointF& point) const;
  [[nodiscard]] absl::optional<Point3F> InverseMapPoint(
      const Point3F& point) const;

  // Applies the reverse transformation on `point`. Returns `absl::nullopt` if
  // the transformation cannot be inverted. Rounds the result to the nearest
  // point.
  [[nodiscard]] absl::optional<Point> InverseMapPoint(const Point& point) const;

  // Returns the rect that is the smallest axis aligned bounding rect
  // containing the transformed rect, clamped with ClampFloatGeometry().
  [[nodiscard]] RectF MapRect(const RectF& rect) const;
  [[nodiscard]] Rect MapRect(const Rect& rect) const;

  // Applies the reverse transformation on the given rect. Returns
  // `absl::nullopt` if the transformation cannot be inverted, or the rect that
  // is the smallest axis aligned bounding rect containing the transformed rect,
  // clamped with ClampFloatGeometry().
  [[nodiscard]] absl::optional<RectF> InverseMapRect(const RectF& rect) const;
  [[nodiscard]] absl::optional<Rect> InverseMapRect(const Rect& rect) const;

  // Returns the box with transformation applied on the given box. The returned
  // box will be the smallest axis aligned bounding box containing the
  // transformed box, clamped with ClampFloatGeometry().
  [[nodiscard]] BoxF MapBox(const BoxF& box) const;

  // Decomposes |this| and |from|, interpolates the decomposed values, and
  // sets |this| to the reconstituted result. Returns false if either matrix
  // can't be decomposed. Uses routines described in this spec:
  // http://www.w3.org/TR/css3-3d-transforms/.
  //
  // Note: this call is expensive since we need to decompose the transform. If
  // you're going to be calling this rapidly (e.g., in an animation) you should
  // decompose once using gfx::DecomposeTransforms and reuse your
  // DecomposedTransform.
  bool Blend(const Transform& from, double progress);

  double Determinant() const;

  void RoundTranslationComponents();

  // Returns |this| * |other|.
  Transform operator*(const Transform& other) const {
    Transform t = *this;
    t.PreConcat(other);
    return t;
  }

  // Sets |this| = |this| * |other|
  Transform& operator*=(const Transform& other) {
    PreConcat(other);
    return *this;
  }

  bool ApproximatelyEqual(const gfx::Transform& transform) const;

  void EnsureFullMatrixForTesting() { EnsureFullMatrix(); }

  std::string ToString() const;

 private:
  // Used internally to construct Transform with parameters in col-major order.
  Transform(double r0c0,
            double r1c0,
            double r2c0,
            double r3c0,
            double r0c1,
            double r1c1,
            double r2c1,
            double r3c1,
            double r0c2,
            double r1c2,
            double r2c2,
            double r3c2,
            double r0c3,
            double r1c3,
            double r2c3,
            double r3c3);
  Transform(float scale_x, float scale_y, float trans_x, float trans_y);

  Point3F MapPointInternal(const Matrix44& xform, const Point3F& point) const;

  Matrix44 GetFullMatrix() const;
  Matrix44& EnsureFullMatrix();

  // axis_2d_ is used if matrix_is nullptr, otherwise *matrix_ is used.
  AxisTransform2d axis_2d_;
  std::unique_ptr<Matrix44> matrix_;
};

// This is declared here for use in gtest-based unit tests but is defined in
// the //ui/gfx:test_support target. Depend on that to use this in your unit
// test. This should not be used in production code - call ToString() instead.
void PrintTo(const Transform& transform, ::std::ostream* os);

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_TRANSFORM_H_
