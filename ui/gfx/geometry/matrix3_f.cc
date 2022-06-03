// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/matrix3_f.h"

#include <string.h>

#include <algorithm>
#include <cmath>
#include <limits>

#include "base/numerics/math_constants.h"
#include "base/strings/stringprintf.h"

namespace {

// This is only to make accessing indices self-explanatory.
enum MatrixCoordinates {
  M00,
  M01,
  M02,
  M10,
  M11,
  M12,
  M20,
  M21,
  M22,
  M_END
};

template<typename T>
double Determinant3x3(T data[M_END]) {
  // This routine is separated from the Matrix3F::Determinant because in
  // computing inverse we do want higher precision afforded by the explicit
  // use of 'double'.
  return
      static_cast<double>(data[M00]) * (
          static_cast<double>(data[M11]) * data[M22] -
          static_cast<double>(data[M12]) * data[M21]) +
      static_cast<double>(data[M01]) * (
          static_cast<double>(data[M12]) * data[M20] -
          static_cast<double>(data[M10]) * data[M22]) +
      static_cast<double>(data[M02]) * (
          static_cast<double>(data[M10]) * data[M21] -
          static_cast<double>(data[M11]) * data[M20]);
}

}  // namespace

namespace gfx {

Matrix3F::Matrix3F() {
}

Matrix3F::~Matrix3F() {
}

// static
Matrix3F Matrix3F::Zeros() {
  Matrix3F matrix;
  matrix.set(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  return matrix;
}

// static
Matrix3F Matrix3F::Ones() {
  Matrix3F matrix;
  matrix.set(1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
  return matrix;
}

// static
Matrix3F Matrix3F::Identity() {
  Matrix3F matrix;
  matrix.set(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
  return matrix;
}

// static
Matrix3F Matrix3F::FromOuterProduct(const Vector3dF& a, const Vector3dF& bt) {
  Matrix3F matrix;
  matrix.set(a.x() * bt.x(), a.x() * bt.y(), a.x() * bt.z(),
             a.y() * bt.x(), a.y() * bt.y(), a.y() * bt.z(),
             a.z() * bt.x(), a.z() * bt.y(), a.z() * bt.z());
  return matrix;
}

bool Matrix3F::IsEqual(const Matrix3F& rhs) const {
  return 0 == memcmp(data_, rhs.data_, sizeof(data_));
}

bool Matrix3F::IsNear(const Matrix3F& rhs, float precision) const {
  DCHECK(precision >= 0);
  for (int i = 0; i < M_END; ++i) {
    if (std::abs(data_[i] - rhs.data_[i]) > precision)
      return false;
  }
  return true;
}

Matrix3F Matrix3F::Add(const Matrix3F& rhs) const {
  Matrix3F result;
  for (int i = 0; i < M_END; ++i)
    result.data_[i] = data_[i] + rhs.data_[i];
  return result;
}

Matrix3F Matrix3F::Subtract(const Matrix3F& rhs) const {
  Matrix3F result;
  for (int i = 0; i < M_END; ++i)
    result.data_[i] = data_[i] - rhs.data_[i];
  return result;
}

Matrix3F Matrix3F::Inverse() const {
  Matrix3F inverse = Matrix3F::Zeros();
  double determinant = Determinant3x3(data_);
  if (std::numeric_limits<float>::epsilon() > std::abs(determinant))
    return inverse;  // Singular matrix. Return Zeros().

  inverse.set(
      static_cast<float>((data_[M11] * data_[M22] - data_[M12] * data_[M21]) /
          determinant),
      static_cast<float>((data_[M02] * data_[M21] - data_[M01] * data_[M22]) /
          determinant),
      static_cast<float>((data_[M01] * data_[M12] - data_[M02] * data_[M11]) /
          determinant),
      static_cast<float>((data_[M12] * data_[M20] - data_[M10] * data_[M22]) /
          determinant),
      static_cast<float>((data_[M00] * data_[M22] - data_[M02] * data_[M20]) /
          determinant),
      static_cast<float>((data_[M02] * data_[M10] - data_[M00] * data_[M12]) /
          determinant),
      static_cast<float>((data_[M10] * data_[M21] - data_[M11] * data_[M20]) /
          determinant),
      static_cast<float>((data_[M01] * data_[M20] - data_[M00] * data_[M21]) /
          determinant),
      static_cast<float>((data_[M00] * data_[M11] - data_[M01] * data_[M10]) /
          determinant));
  return inverse;
}

Matrix3F Matrix3F::Transpose() const {
  Matrix3F transpose;
  transpose.set(data_[M00], data_[M10], data_[M20], data_[M01], data_[M11],
                data_[M21], data_[M02], data_[M12], data_[M22]);
  return transpose;
}

float Matrix3F::Determinant() const {
  return static_cast<float>(Determinant3x3(data_));
}

Matrix3F MatrixProduct(const Matrix3F& lhs, const Matrix3F& rhs) {
  Matrix3F result = Matrix3F::Zeros();
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      result.set(i, j, DotProduct(lhs.get_row(i), rhs.get_column(j)));
    }
  }
  return result;
}

Vector3dF MatrixProduct(const Matrix3F& lhs, const Vector3dF& rhs) {
  return Vector3dF(DotProduct(lhs.get_row(0), rhs),
                   DotProduct(lhs.get_row(1), rhs),
                   DotProduct(lhs.get_row(2), rhs));
}

std::string Matrix3F::ToString() const {
  return base::StringPrintf(
      "[[%+0.4f, %+0.4f, %+0.4f],"
      " [%+0.4f, %+0.4f, %+0.4f],"
      " [%+0.4f, %+0.4f, %+0.4f]]",
      data_[M00], data_[M01], data_[M02], data_[M10], data_[M11], data_[M12],
      data_[M20], data_[M21], data_[M22]);
}

}  // namespace gfx
