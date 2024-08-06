// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef UI_GFX_GEOMETRY_TRANSFORM_H_
#define UI_GFX_GEOMETRY_TRANSFORM_H_

#include <iosfwd>
#include <memory>
#include <optional>
#include <string>

#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/geometry_skia_export.h"
#include "ui/gfx/geometry/matrix44.h"

namespace gfx {

class BoxF;
class Rect;
class RectF;
class Point;
class PointF;
class Point3F;
class QuadF;
class Quaternion;
class Vector2dF;
class Vector3dF;
struct DecomposedTransform;

// 4x4 Transformation matrix. Depending on the complexity of the matrix, it may
// be internally stored as an AxisTransform2d (float precision) or a full
// Matrix44 (4x4 double precision). Which one is used only affects precision and
// performance.
// - On construction (including constructors and static functions returning a
//   new Transform object), AxisTransform2d will be used if it the matrix will
//   be 2d scale and/or translation, otherwise Matrix44, with some exceptions
//   (e.g. ColMajor()) described in the method comments.
// - On mutation, if the matrix has been using AxisTransform2d and the result
//   can still be 2d scale and/or translation, AxisTransform2d will still be
//   used, otherwise Matrix44, with some exceptions (e.g. set_rc()) described
//   in the method comments.
// - On assignment, the new matrix will keep the choice of the rhs matrix.
//
class GEOMETRY_SKIA_EXPORT Transform {
 public:
  constexpr Transform() : axis_2d_() {}

  explicit Transform(const AxisTransform2d& axis_2d) : axis_2d_(axis_2d) {}

  // Creates a transform from explicit 16 matrix elements in row-major order.
  // Always creates a double precision 4x4 matrix.
  // clang-format off
  static constexpr Transform RowMajor(
      double r0c0, double r0c1, double r0c2, double r0c3,
      double r1c0, double r1c1, double r1c2, double r1c3,
      double r2c0, double r2c1, double r2c2, double r2c3,
      double r3c0, double r3c1, double r3c2, double r3c3) {
    return Transform(r0c0, r1c0, r2c0, r3c0,   // col 0
                     r0c1, r1c1, r2c1, r3c1,   // col 1
                     r0c2, r1c2, r2c2, r3c2,   // col 2
                     r0c3, r1c3, r2c3, r3c3);  // col 3
  }

  // Creates a transform from explicit 16 matrix elements in col-major order.
  // Always creates a double precision 4x4 matrix.
  // See also ColMajor(double[]) and ColMajorF(float[]).
  static constexpr Transform ColMajor(
      double r0c0, double r1c0, double r2c0, double r3c0,
      double r0c1, double r1c1, double r2c1, double r3c1,
      double r0c2, double r1c2, double r2c2, double r3c2,
      double r0c3, double r1c3, double r2c3, double r3c3) {
    return Transform(r0c0, r1c0, r2c0, r3c0,   // col 0
                     r0c1, r1c1, r2c1, r3c1,   // col 1
                     r0c2, r1c2, r2c2, r3c2,   // col 2
                     r0c3, r1c3, r2c3, r3c3);  // col 3
  }
  // clang-format on

  // Creates a transform from explicit 2d elements. All other matrix elements
  // remain the same as the corresponding elements of an identity matrix.
  // Always creates a double precision 4x4 matrix.
  // TODO(crbug.com/40237414): Revisit the above statement. Evaluate performance
  // and precision requirements of SVG and CSS transform:matrix().
  static constexpr Transform Affine(double a,    // a.k.a. r0c0 or scale_x
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
  static constexpr Transform MakeTranslation(float tx, float ty) {
    return Transform(1, 1, tx, ty);
  }
  static Transform MakeTranslation(const Vector2dF& v) {
    return MakeTranslation(v.x(), v.y());
  }

  // Creates a transform as a 2d scale.
  static constexpr Transform MakeScale(float scale) {
    return MakeScale(scale, scale);
  }
  static constexpr Transform MakeScale(float sx, float sy) {
    return Transform(sx, sy, 0, 0);
  }

  // Accurately rotate by 90, 180 or 270 degrees about the z axis.
  static constexpr Transform Make90degRotation() {
    return Affine(0, 1, -1, 0, 0, 0);
  }
  static constexpr Transform Make180degRotation() { return MakeScale(-1); }
  static constexpr Transform Make270degRotation() {
    return Affine(0, -1, 1, 0, 0, 0);
  }

  // Resets this transform to the identity transform.
  void MakeIdentity() {
    full_matrix_ = false;
    axis_2d_ = AxisTransform2d();
  }

  bool operator==(const Transform& rhs) const {
    if (!full_matrix_ && !rhs.full_matrix_) [[likely]] {
      return axis_2d_ == rhs.axis_2d_;
    }
    if (full_matrix_ && rhs.full_matrix_)
      return matrix_ == rhs.matrix_;
    return GetFullMatrix() == rhs.GetFullMatrix();
  }
  bool operator!=(const Transform& rhs) const { return !(*this == rhs); }

  // Gets a value at |row|, |col| from the matrix.
  constexpr double rc(int row, int col) const {
    DCHECK_LE(static_cast<unsigned>(row), 3u);
    DCHECK_LE(static_cast<unsigned>(col), 3u);
    if (!full_matrix_) [[likely]] {
      float m[4][4] = {{axis_2d_.scale().x(), 0, 0, axis_2d_.translation().x()},
                       {0, axis_2d_.scale().y(), 0, axis_2d_.translation().y()},
                       {0, 0, 1, 0},
                       {0, 0, 0, 1}};
      return m[row][col];
    }
    return matrix_.rc(row, col);
  }

  // Sets a value in the matrix at |row|, |col|. It forces full double precision
  // 4x4 matrix.
  void set_rc(int row, int col, double v) {
    DCHECK_LE(static_cast<unsigned>(row), 3u);
    DCHECK_LE(static_cast<unsigned>(col), 3u);
    EnsureFullMatrix().set_rc(row, col, v);
  }

  // Constructs Transform from a double col-major array.
  // Always creates a double precision 4x4 matrix.
  static Transform ColMajor(const double a[16]);

  // Constructs Transform from a float col-major array. Creates an
  // AxisTransform2d or a Matrix44 depending on the values. GetColMajorF() and
  // ColMajorF() are used when passing a Transform through mojo.
  static Transform ColMajorF(const float a[16]);

  // Gets col-major data.
  void GetColMajor(double a[16]) const;
  void GetColMajorF(float a[16]) const;
  double ColMajorData(int index) const { return rc(index % 4, index / 4); }

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
  void Scale(float scale) { Scale(scale, scale); }
  void Scale(float x, float y);
  void Scale3d(float x, float y, float z);

  // Applies a scale to the current transformation and assigns the result to
  // |this|, i.e. this = scaling * this.
  void PostScale(float scale) { PostScale(scale, scale); }
  void PostScale(float x, float y);
  void PostScale3d(float x, float y, float z);

  // Applies the current transformation on a translation and assigns the result
  // to |this|, i.e. this = this * translation.
  void Translate(const Vector2dF& offset);
  void Translate(float x, float y);
  void Translate3d(const Vector3dF& offset);
  void Translate3d(float x, float y, float z);

  // Applies a translation to the current transformation and assigns the result
  // to |this|, i.e. this = translation * this.
  void PostTranslate(const Vector2dF& offset);
  void PostTranslate(float x, float y);
  void PostTranslate3d(const Vector3dF& offset);
  void PostTranslate3d(float x, float y, float z);

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
  void RotateAbout(double x, double y, double z, double degrees);
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
    if (!full_matrix_) [[likely]] {
      return axis_2d_ == AxisTransform2d();
    }
    return matrix_.IsIdentity();
  }

  // Returns true if the matrix is either identity or pure translation.
  bool IsIdentityOrTranslation() const {
    if (!full_matrix_) [[likely]] {
      return axis_2d_.scale() == Vector2dF(1, 1);
    }
    return matrix_.IsIdentityOrTranslation();
  }

  // Returns true if the matrix is either the identity or a 2d translation.
  bool IsIdentityOr2dTranslation() const {
    if (!full_matrix_) [[likely]] {
      return axis_2d_.scale() == Vector2dF(1, 1);
    }
    return matrix_.IsIdentityOrTranslation() && matrix_.rc(2, 3) == 0;
  }

  // Returns true if the matrix is either identity or pure translation,
  // allowing for an amount of inaccuracy as specified by the parameter.
  bool IsApproximatelyIdentityOrTranslation(double tolerance) const;
  bool IsApproximatelyIdentityOrIntegerTranslation(double tolerance) const;

  // Returns true if the matrix is either a positive scale and/or a translation.
  bool IsPositiveScaleOrTranslation() const {
    if (!full_matrix_) [[likely]] {
      return axis_2d_.scale().x() > 0.0 && axis_2d_.scale().y() > 0.0;
    }

    if (!matrix_.IsScaleOrTranslation())
      return false;
    return matrix_.rc(0, 0) > 0.0 && matrix_.rc(1, 1) > 0.0 &&
           matrix_.rc(2, 2) > 0.0;
  }

  // Returns true if the matrix is 2d scale or translation, and if it has scale,
  // the scale proportionally scales up in x and y directions. This function
  // allows rare false-negatives.
  bool Is2dProportionalUpscaleAndOr2dTranslation() const;

  // Returns true if the matrix is identity or, if the matrix consists only
  // of a translation whose components can be represented as integers. Returns
  // false if the translation contains a fractional component or is too large to
  // fit in an integer.
  bool IsIdentityOrIntegerTranslation() const;
  bool IsIdentityOrInteger2dTranslation() const;

  // Returns whether this matrix can transform a z=0 plane to something
  // containing points where z != 0. This is primarily intended for metrics.
  bool Creates3d() const;

  // Returns true if the matrix has only x and y scaling components, including
  // identity.
  bool IsScale2d() const {
    if (!full_matrix_) [[likely]] {
      return axis_2d_.translation().IsZero();
    }
    return matrix_.IsScale() && matrix_.rc(2, 2) == 1;
  }

  // Returns true if the matrix is has only scaling and translation components,
  // including identity.
  bool IsScaleOrTranslation() const {
    if (!full_matrix_) [[likely]] {
      return true;
    }
    return matrix_.IsScaleOrTranslation();
  }

  // Returns true if, for 2d rects on the x/y plane, this matrix can be
  // represented as a 2d affine transform on the x/y plane.
  bool Preserves2dAffine() const;

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
    if (!full_matrix_) [[likely]] {
      return false;
    }
    return matrix_.HasPerspective();
  }

  // Returns true if this transform is non-singular.
  bool IsInvertible() const {
    if (!full_matrix_) [[likely]] {
      return axis_2d_.IsInvertible();
    }
    return matrix_.IsInvertible();
  }

  // If |this| is invertible, inverts |this| and stores the result in
  // |*transform|, and returns true. Otherwise sets |*transform| to identity
  // and returns false.
  [[nodiscard]] bool GetInverse(Transform* transform) const;

  // Same as above except that it assumes success, otherwise DCHECK fails.
  // This is suitable when the transform is known to be invertible.
  [[nodiscard]] Transform GetCheckedInverse() const;

  // Same as GetInverse() except that it returns the the inverse of |this| or
  // identity, instead of a bool. This is suitable when it's good to fallback
  // to identity silently.
  [[nodiscard]] Transform InverseOrIdentity() const;

  // Returns true if a layer with a forward-facing normal of (0, 0, 1) would
  // have its back side facing frontwards after applying the transform.
  bool IsBackFaceVisible() const;

  // Transposes this transform in place.
  void Transpose();

  // Changes the transform to apply as if the origin were at (x, y, z).
  void ApplyTransformOrigin(float x, float y, float z);

  // Changes the transform to:
  //     scale3d(z, z, z) * mat * scale3d(1/z, 1/z, 1/z)
  // Useful for mapping zoomed points to their zoomed transformed result:
  //     new_mat * (scale3d(z, z, z) * x) == scale3d(z, z, z) * (mat * x)
  void Zoom(float zoom_factor);

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
  void Flatten();

  // Returns true if the 3rd row and 3rd column are both (0, 0, 1, 0).
  bool IsFlat() const;

  // Returns true if the transform is flat and doesn't have perspective.
  bool Is2dTransform() const;

  // Returns the x and y translation components of the matrix, clamped with
  // ClampFloatGeometry().
  Vector2dF To2dTranslation() const;

  // Returns the x, y and z translation components of the matrix, clampe with
  // ClampFloatGeometry().
  Vector3dF To3dTranslation() const;

  // Returns the x and y scale components of the matrix, clamped with
  // ClampFloatGeometry().
  Vector2dF To2dScale() const;

  // Returns the point with the transformation applied to |point|, clamped
  // with ClampFloatGeometry().
  [[nodiscard]] Point3F MapPoint(const Point3F& point) const;
  // Maps [point.x(), point.y(), 0] to [result.x(), result.y(), discarded_z].
  [[nodiscard]] PointF MapPoint(const PointF& point) const;
  [[nodiscard]] Point MapPoint(const Point& point) const;

  // Returns the vector with the transformation applied to |vector|, clamped
  // with ClampFloatGeometry(). It differs from MapPoint() by that the
  // translation and perspective components of the matrix are ignored.
  [[nodiscard]] Vector3dF MapVector(const Vector3dF& vector) const;

  // Applies the transformation to the vector. The results are clamped with
  // ClampFloatGeometry().
  void TransformVector4(float vector[4]) const;

  // Returns the point with reverse transformation applied to `point`, clamped
  // with ClampFloatGeometry(), or `std::nullopt` if the transformation cannot
  // be inverted.
  [[nodiscard]] std::optional<PointF> InverseMapPoint(
      const PointF& point) const;
  [[nodiscard]] std::optional<Point3F> InverseMapPoint(
      const Point3F& point) const;

  // Applies the reverse transformation on `point`. Returns `std::nullopt` if
  // the transformation cannot be inverted. Rounds the result to the nearest
  // point.
  [[nodiscard]] std::optional<Point> InverseMapPoint(const Point& point) const;

  // Returns the rect that is the smallest axis aligned bounding rect
  // containing the transformed rect, clamped with ClampFloatGeometry().
  [[nodiscard]] RectF MapRect(const RectF& rect) const;
  [[nodiscard]] Rect MapRect(const Rect& rect) const;

  // Applies the reverse transformation on the given rect. Returns
  // `std::nullopt` if the transformation cannot be inverted, or the rect that
  // is the smallest axis aligned bounding rect containing the transformed rect,
  // clamped with ClampFloatGeometry().
  [[nodiscard]] std::optional<RectF> InverseMapRect(const RectF& rect) const;
  [[nodiscard]] std::optional<Rect> InverseMapRect(const Rect& rect) const;

  // Returns the box with transformation applied on the given box. The returned
  // box will be the smallest axis aligned bounding box containing the
  // transformed box, clamped with ClampFloatGeometry().
  [[nodiscard]] BoxF MapBox(const BoxF& box) const;

  // Applies transformation on the given quad by applying the transformation
  // on each point of the quad.
  [[nodiscard]] QuadF MapQuad(const QuadF& quad) const;

  // Maps a point on the z=0 plane into a point on the plane with which the
  // transform applied, by extending a ray perpendicular to the source plane and
  // computing the local x,y position of the point where that ray intersects
  // with the destination plane. If such a point exists, sets |*clamped| (if
  // provided) to false and returns the point. Otherwise sets |*clamped| (if
  // provided) to true and:
  // - If the ray is parallel with the destination plane, returns PointF().
  // - If the opposite ray intersects with the destination plane, returns
  //   a point containing signed big values (simulating infinities).
  //
  // See https://bit.ly/perspective-projection-clamping for an illustration of
  // clamping with perspective.
  //
  // When |this| is invertible and the result |*clamped| is false, this
  // function is equivalent to:
  //   inverse(flatten(inverse(this))).MapPoint(point)
  // and
  //   MapPoint(Point3F(point.x(), point.y(), unknown_z)) to
  //   Point3F(result.x(), result.y(), 0).
  [[nodiscard]] PointF ProjectPoint(const PointF& point,
                                    bool* clamped = nullptr) const;

  // Projects the four corners of the quad with ProjectPoint(). Returns an
  // empty quad if all of the vertices are clamped.
  [[nodiscard]] QuadF ProjectQuad(const QuadF& quad) const;

  // Decomposes |this| into |decomp|. Returns nullopt if |this| can't be
  // decomposed. |decomp| must be identity on input.
  //
  // Uses routines described in the following specs:
  // 2d: https://www.w3.org/TR/css-transforms-1/#decomposing-a-2d-matrix
  // 3d: https://www.w3.org/TR/css-transforms-2/#decomposing-a-3d-matrix
  //
  // Note: when the determinant is negative, the 2d spec calls for flipping one
  // of the axis, while the general 3d spec calls for flipping all of the
  // scales. The latter not only introduces rotation in the case of a trivial
  // scale inversion, but causes transformed objects to needlessly shrink and
  // grow as they transform through scale = 0 along multiple axes. Thus 2d
  // transforms should follow the 2d spec regarding matrix decomposition.
  std::optional<DecomposedTransform> Decompose() const;

  // Composes a transform from the given |decomp|, following the routines
  // detailed in this specs:
  // https://www.w3.org/TR/css-transforms-2/#recomposing-to-a-3d-matrix
  static Transform Compose(const DecomposedTransform& decomp);

  // Decomposes |this| and |from|, interpolates the decomposed values, and
  // sets |this| to the reconstituted result. Returns false and leaves |this|
  // unchanged if either matrix can't be decomposed.
  // Uses routines described in this spec:
  // https://www.w3.org/TR/css-transforms-2/#matrix-interpolation
  //
  // Note: this call is expensive for complex transforms since we need to
  // decompose the transforms. If you're going to be calling this rapidly
  // (e.g., in an animation) for complex transforms, you should decompose once
  // using Decompose() and reuse your DecomposedTransform with
  // BlendDecomposedTransforms() (see transform_util.h).
  bool Blend(const Transform& from, double progress);

  // Decomposes |this| and |from|, accumulates the decomposed values, and
  // sets |this| to the reconstituted result. Returns false and leaves |this|
  // unchanged if either matrix can't be decomposed.
  // Uses routines described in this spec:
  // https://www.w3.org/TR/css-transforms-2/#combining-transform-lists.
  //
  // Note: this function has the same performance characteristics as Blend().
  // When possible, you should also reuse DecomposedTransform with
  // AccumulateDecomposedTransforms() (see transform_util.h).
  bool Accumulate(const Transform& other);

  double Determinant() const;

  // Rounds 2d translation components rc(0, 3), rc(1, 3) to integers.
  void Round2dTranslationComponents();

  // Rounds translation components to integers, and all other components to
  // identity. Normally this function is meaningful only if
  // IsApproximatelyIdentityOrIntegerTranslation() is true.
  void RoundToIdentityOrIntegerTranslation();

  // Returns |this| * |other|.
  Transform operator*(const Transform& other) const;

  // Sets |this| = |this| * |other|
  Transform& operator*=(const Transform& other) {
    PreConcat(other);
    return *this;
  }

  // Checks whether `this` approximately equals `transform`.
  // Returns true if all following conditions are met:
  // - For (x, y) in all translation components of (this, transform):
  //   abs(x - y) <= abs_translation_tolerance
  // - For (x, y) in all other components of (this, transform):
  //   abs(x - y) <= abs_other_tolerance
  // - If rel_scale_tolerance is not zero, for (x, y) in all scale components:
  //   abs(x - y) <= (abs(x) + abs(y)) * rel_scale_tolerance.
  bool ApproximatelyEqual(const gfx::Transform& transform,
                          float abs_translation_tolerance,
                          float abs_other_tolerance,
                          float rel_scale_tolerance) const;
  // Checks approximate equality with one tolerance for all components.
  bool ApproximatelyEqual(const gfx::Transform& transform,
                          float abs_tolerance) const {
    return ApproximatelyEqual(transform, abs_tolerance, abs_tolerance, 0.0f);
  }
  // Checks approximate equality with default tolerances. Note that the
  // tolerance for translation is big to tolerate scroll components due to
  // snapping (floating point error might round the other way).
  bool ApproximatelyEqual(const gfx::Transform& transform) const {
    return ApproximatelyEqual(transform, 1.0f, 0.1f, 0.0f);
  }

  void EnsureFullMatrixForTesting() { EnsureFullMatrix(); }

  // Returns a string in the format of "[ row0\n, row1\n, row2\n, row3 ]\n".
  std::string ToString() const;

  // Returns a string containing decomposed components.
  std::string ToDecomposedString() const;

 private:
  // Used internally to construct Transform with parameters in col-major order.
  // clang-format off
  constexpr Transform(double r0c0, double r1c0, double r2c0, double r3c0,
                      double r0c1, double r1c1, double r2c1, double r3c1,
                      double r0c2, double r1c2, double r2c2, double r3c2,
                      double r0c3, double r1c3, double r2c3, double r3c3)
      : full_matrix_(true),
        matrix_(r0c0, r1c0, r2c0, r3c0,
                r0c1, r1c1, r2c1, r3c1,
                r0c2, r1c2, r2c2, r3c2,
                r0c3, r1c3, r2c3, r3c3) {}
  // clang-format on

  constexpr Transform(float scale_x,
                      float scale_y,
                      float trans_x,
                      float trans_y)
      : axis_2d_(AxisTransform2d::FromScaleAndTranslation(
            Vector2dF(scale_x, scale_y),
            Vector2dF(trans_x, trans_y))) {}

  // Used internally to construct a Transform with uninitialized full matrix.
  explicit Transform(Matrix44::UninitializedTag tag)
      : full_matrix_(true), matrix_(tag) {}

  PointF MapPointInternal(const Matrix44& matrix, const PointF& point) const;
  Point3F MapPointInternal(const Matrix44& matrix, const Point3F& point) const;

  Matrix44 GetFullMatrix() const;
  Matrix44& EnsureFullMatrix();

  // axis_2d_ is used if full_matrix_ is false, otherwise matrix_ is used.
  // See the class documentation for more details about how we use them.
  bool full_matrix_ = false;
  union {
    // Each constructor must explicitly initialize one of the following,
    // according to the value of full_matrix_.
    AxisTransform2d axis_2d_;
    Matrix44 matrix_;
  };
};

// This is declared here for use in gtest-based unit tests but is defined in
// the //ui/gfx:test_support target. Depend on that to use this in your unit
// test. This should not be used in production code - call ToString() instead.
void PrintTo(const Transform& transform, ::std::ostream* os);

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_TRANSFORM_H_
