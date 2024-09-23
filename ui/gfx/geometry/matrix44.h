// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef UI_GFX_GEOMETRY_MATRIX44_H_
#define UI_GFX_GEOMETRY_MATRIX44_H_

#include <optional>

#include "base/check_op.h"
#include "ui/gfx/geometry/double4.h"
#include "ui/gfx/geometry/geometry_skia_export.h"

namespace gfx {

struct DecomposedTransform;

// This is the underlying data structure of Transform. Don't use this type
// directly.
//
// Throughout this class, we will be speaking in column vector convention.
// i.e. Applying a transform T to vector V is T * V.
// The components of the matrix and the vector look like:
//    \  col
// r   \     0        1        2        3
// o  0 | scale_x  skew_xy  skew_xz  trans_x |   | x |
// w  1 | skew_yx  scale_y  skew_yz  trans_y | * | y |
//    2 | skew_zx  skew_zy  scale_z  trans_z |   | z |
//    3 | persp_x  persp_y  persp_z  persp_w |   | w |
//
// Note that the names are just for remembering and don't have the exact
// meanings when other components exist.
//
// The components correspond to the DOMMatrix mij (i,j = 1..4) components:
//   i = col + 1
//   j = row + 1
class GEOMETRY_SKIA_EXPORT Matrix44 {
 public:
  enum UninitializedTag { kUninitialized };

  explicit Matrix44(UninitializedTag) {}

  constexpr Matrix44()
      : matrix_{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}} {}

  // The parameters are in col-major order.
  // clang-format off
  constexpr Matrix44(double r0c0, double r1c0, double r2c0, double r3c0,
                     double r0c1, double r1c1, double r2c1, double r3c1,
                     double r0c2, double r1c2, double r2c2, double r3c2,
                     double r0c3, double r1c3, double r2c3, double r3c3)
      // matrix_ is indexed by [col][row] (i.e. col-major).
      : matrix_{{r0c0, r1c0, r2c0, r3c0},
                {r0c1, r1c1, r2c1, r3c1},
                {r0c2, r1c2, r2c2, r3c2},
                {r0c3, r1c3, r2c3, r3c3}} {}
  // clang-format on

  bool operator==(const Matrix44& other) const {
    return AllTrue(Col(0) == other.Col(0)) && AllTrue(Col(1) == other.Col(1)) &&
           AllTrue(Col(2) == other.Col(2)) && AllTrue(Col(3) == other.Col(3));
  }
  bool operator!=(const Matrix44& other) const { return !(other == *this); }

  // Returns true if the matrix is identity.
  bool IsIdentity() const { return *this == Matrix44(); }

  // Returns true if the matrix contains translate or is identity.
  bool IsIdentityOrTranslation() const {
    return AllTrue(Col(0) == Double4{1, 0, 0, 0}) &&
           AllTrue(Col(1) == Double4{0, 1, 0, 0}) &&
           AllTrue(Col(2) == Double4{0, 0, 1, 0}) && matrix_[3][3] == 1;
  }

  // Returns true if the matrix only contains scale or translate or is identity.
  bool IsScaleOrTranslation() const {
    return AllTrue(Double4{matrix_[0][1], matrix_[0][2], matrix_[0][3],
                           matrix_[1][0]} == Double4{0, 0, 0, 0}) &&
           AllTrue(Double4{matrix_[1][2], matrix_[1][3], matrix_[2][0],
                           matrix_[2][1]} == Double4{0, 0, 0, 0}) &&
           matrix_[2][3] == 0 && matrix_[3][3] == 1;
  }

  // Returns true if the matrix only contains scale or is identity.
  bool IsScale() const {
    return IsScaleOrTranslation() && AllTrue(Col(3) == Double4{0, 0, 0, 1});
  }

  bool IsFlat() const {
    return AllTrue(Col(2) == Double4{0, 0, 1, 0}) &&
           AllTrue(Double4{matrix_[0][2], matrix_[1][2], 0, matrix_[3][2]} ==
                   Double4{0, 0, 0, 0});
  }

  bool HasPerspective() const {
    return !AllTrue(Double4{matrix_[0][3], matrix_[1][3], matrix_[2][3],
                            matrix_[3][3]} == Double4{0, 0, 0, 1});
  }

  bool Is2dTransform() const { return IsFlat() && !HasPerspective(); }

  // Gets a value at |row|, |col| from the matrix.
  constexpr double rc(int row, int col) const {
    DCHECK_LE(static_cast<unsigned>(row), 3u);
    DCHECK_LE(static_cast<unsigned>(col), 3u);
    return matrix_[col][row];
  }

  // Set a value in the matrix at |row|, |col|.
  void set_rc(int row, int col, double value) {
    DCHECK_LE(static_cast<unsigned>(row), 3u);
    DCHECK_LE(static_cast<unsigned>(col), 3u);
    matrix_[col][row] = value;
  }

  void GetColMajor(double[16]) const;
  void GetColMajorF(float[16]) const;

  // this = this * translation.
  void PreTranslate(double dx, double dy);
  void PreTranslate3d(double dx, double dy, double dz);
  // this = translation * this.
  void PostTranslate(double dx, double dy);
  void PostTranslate3d(double dx, double dy, double dz);

  // this = this * scale.
  void PreScale(double sx, double sy);
  void PreScale3d(double sx, double sy, double sz);
  // this = scale * this.
  void PostScale(double sx, double sy);
  void PostScale3d(double sx, double sy, double sz);

  // Rotates this matrix about the specified unit-length axis vector,
  // by an angle specified by its sin() and cos(). This does not attempt to
  // verify that axis(x, y, z).length() == 1 or that the sin, cos values are
  // correct. this = this * rotation.
  void RotateUnitSinCos(double x,
                        double y,
                        double z,
                        double sin_angle,
                        double cos_angle);

  // Special case for x, y or z axis of the above function.
  void RotateAboutXAxisSinCos(double sin_angle, double cos_angle);
  void RotateAboutYAxisSinCos(double sin_angle, double cos_angle);
  void RotateAboutZAxisSinCos(double sin_angle, double cos_angle);

  // this = this * skew.
  void Skew(double tan_skew_x, double tan_skew_y);

  //               |1 skew[0] skew[1] 0|
  // this = this * |0    1    skew[2] 0|
  //               |0    0      1     0|
  //               |0    0      0     1|
  void ApplyDecomposedSkews(const double skews[3]);

  // this = this * perspective.
  void ApplyPerspectiveDepth(double perspective);

  // this = this * m.
  void PreConcat(const Matrix44& m) { SetConcat(*this, m); }
  // this = m * this.
  void PostConcat(const Matrix44& m) { SetConcat(m, *this); }
  // this = a * b.
  void SetConcat(const Matrix44& a, const Matrix44& b);

  // Returns true and set |inverse| to the inverted matrix if this matrix
  // is invertible. Otherwise return false and leave the |inverse| parameter
  // unchanged.
  bool GetInverse(Matrix44& inverse) const;

  bool IsInvertible() const;
  double Determinant() const;

  // Transposes this matrix in place.
  void Transpose();

  // See Transform::Zoom().
  void Zoom(double zoom_factor);

  // Applies the matrix to the vector in place.
  void MapVector4(double vec[4]) const;

  // Same as above, but assumes the vec[2] is 0 and vec[3] is 1, discards
  // vec[2], and returns vec[3].
  double MapVector2(double vec[2]) const;

  void Flatten();

  std::optional<DecomposedTransform> Decompose() const;

 private:
  std::optional<DecomposedTransform> Decompose2d() const;

  ALWAYS_INLINE Double4 Col(int i) const { return LoadDouble4(matrix_[i]); }
  ALWAYS_INLINE void SetCol(int i, Double4 v) { StoreDouble4(v, matrix_[i]); }

  // This is indexed by [col][row].
  double matrix_[4][4];
};

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_MATRIX44_H_
