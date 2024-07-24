// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef UI_GFX_GEOMETRY_MATRIX3_F_H_
#define UI_GFX_GEOMETRY_MATRIX3_F_H_

#include "base/check.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace gfx {

class GEOMETRY_EXPORT Matrix3F {
 public:
  ~Matrix3F();

  static Matrix3F Zeros();
  static Matrix3F Ones();
  static Matrix3F Identity();
  static Matrix3F FromOuterProduct(const Vector3dF& a, const Vector3dF& bt);

  bool IsEqual(const Matrix3F& rhs) const;

  // Element-wise comparison with given precision.
  bool IsNear(const Matrix3F& rhs, float precision) const;

  float get(int i, int j) const {
    return data_[MatrixToArrayCoords(i, j)];
  }

  void set(int i, int j, float v) {
    data_[MatrixToArrayCoords(i, j)] = v;
  }

  void set(float m00, float m01, float m02,
           float m10, float m11, float m12,
           float m20, float m21, float m22) {
    data_[0] = m00;
    data_[1] = m01;
    data_[2] = m02;
    data_[3] = m10;
    data_[4] = m11;
    data_[5] = m12;
    data_[6] = m20;
    data_[7] = m21;
    data_[8] = m22;
  }

  Vector3dF get_row(int i) const {
    return Vector3dF(data_[MatrixToArrayCoords(i, 0)],
                     data_[MatrixToArrayCoords(i, 1)],
                     data_[MatrixToArrayCoords(i, 2)]);
  }

  Vector3dF get_column(int i) const {
    return Vector3dF(
      data_[MatrixToArrayCoords(0, i)],
      data_[MatrixToArrayCoords(1, i)],
      data_[MatrixToArrayCoords(2, i)]);
  }

  void set_column(int i, const Vector3dF& c) {
    data_[MatrixToArrayCoords(0, i)] = c.x();
    data_[MatrixToArrayCoords(1, i)] = c.y();
    data_[MatrixToArrayCoords(2, i)] = c.z();
  }

  // Produces a new matrix by adding the elements of |rhs| to this matrix
  Matrix3F Add(const Matrix3F& rhs) const;
  // Produces a new matrix by subtracting elements of |rhs| from this matrix.
  Matrix3F Subtract(const Matrix3F& rhs) const;

  // Returns an inverse of this if the matrix is non-singular, zero (== Zero())
  // otherwise.
  Matrix3F Inverse() const;

  // Returns a transpose of this matrix.
  Matrix3F Transpose() const;

  // Value of the determinant of the matrix.
  float Determinant() const;

  // Trace (sum of diagonal elements) of the matrix.
  float Trace() const {
    return data_[MatrixToArrayCoords(0, 0)] +
        data_[MatrixToArrayCoords(1, 1)] +
        data_[MatrixToArrayCoords(2, 2)];
  }

  std::string ToString() const;

 private:
  Matrix3F();  // Uninitialized default.

  static int MatrixToArrayCoords(int i, int j) {
    DCHECK(i >= 0 && i < 3);
    DCHECK(j >= 0 && j < 3);
    return i * 3 + j;
  }

  float data_[9];
};

inline bool operator==(const Matrix3F& lhs, const Matrix3F& rhs) {
  return lhs.IsEqual(rhs);
}

// Matrix addition. Produces a new matrix by adding the corresponding elements
// together.
inline Matrix3F operator+(const Matrix3F& lhs, const Matrix3F& rhs) {
  return lhs.Add(rhs);
}

// Matrix subtraction. Produces a new matrix by subtracting elements of rhs
// from corresponding elements of lhs.
inline Matrix3F operator-(const Matrix3F& lhs, const Matrix3F& rhs) {
  return lhs.Subtract(rhs);
}

GEOMETRY_EXPORT Matrix3F MatrixProduct(const Matrix3F& lhs,
                                       const Matrix3F& rhs);
GEOMETRY_EXPORT Vector3dF MatrixProduct(const Matrix3F& lhs,
                                        const Vector3dF& rhs);

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_MATRIX3_F_H_
