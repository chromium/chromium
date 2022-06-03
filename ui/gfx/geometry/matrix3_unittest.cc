// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <limits>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/matrix3_f.h"

namespace gfx {
namespace {

TEST(Matrix3fTest, Constructors) {
  Matrix3F zeros = Matrix3F::Zeros();
  Matrix3F ones = Matrix3F::Ones();
  Matrix3F identity = Matrix3F::Identity();

  Matrix3F product_ones = Matrix3F::FromOuterProduct(
      Vector3dF(1.0f, 1.0f, 1.0f), Vector3dF(1.0f, 1.0f, 1.0f));
  Matrix3F product_zeros = Matrix3F::FromOuterProduct(
      Vector3dF(1.0f, 1.0f, 1.0f), Vector3dF(0.0f, 0.0f, 0.0f));
  EXPECT_EQ(ones, product_ones);
  EXPECT_EQ(zeros, product_zeros);

  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j)
      EXPECT_EQ(i == j ? 1.0f : 0.0f, identity.get(i, j));
  }
}

TEST(Matrix3fTest, DataAccess) {
  Matrix3F matrix = Matrix3F::Ones();
  Matrix3F identity = Matrix3F::Identity();

  EXPECT_EQ(Vector3dF(0.0f, 1.0f, 0.0f), identity.get_column(1));
  EXPECT_EQ(Vector3dF(0.0f, 1.0f, 0.0f), identity.get_row(1));
  matrix.set(0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f);
  EXPECT_EQ(Vector3dF(2.0f, 5.0f, 8.0f), matrix.get_column(2));
  EXPECT_EQ(Vector3dF(6.0f, 7.0f, 8.0f), matrix.get_row(2));
  matrix.set_column(0, Vector3dF(0.1f, 0.2f, 0.3f));
  matrix.set_column(0, Vector3dF(0.1f, 0.2f, 0.3f));
  EXPECT_EQ(Vector3dF(0.1f, 0.2f, 0.3f), matrix.get_column(0));
  EXPECT_EQ(Vector3dF(0.1f, 1.0f, 2.0f), matrix.get_row(0));

  EXPECT_EQ(0.1f, matrix.get(0, 0));
  EXPECT_EQ(5.0f, matrix.get(1, 2));
}

TEST(Matrix3fTest, Determinant) {
  EXPECT_EQ(1.0f, Matrix3F::Identity().Determinant());
  EXPECT_EQ(0.0f, Matrix3F::Zeros().Determinant());
  EXPECT_EQ(0.0f, Matrix3F::Ones().Determinant());

  // Now for something non-trivial...
  Matrix3F matrix = Matrix3F::Zeros();
  matrix.set(0, 5, 6, 8, 7, 0, 1, 9, 0);
  EXPECT_EQ(390.0f, matrix.Determinant());
  matrix.set(2, 0, 3 * matrix.get(0, 0));
  matrix.set(2, 1, 3 * matrix.get(0, 1));
  matrix.set(2, 2, 3 * matrix.get(0, 2));
  EXPECT_EQ(0, matrix.Determinant());

  matrix.set(0.57f,  0.205f,  0.942f,
             0.314f,  0.845f,  0.826f,
             0.131f,  0.025f,  0.962f);
  EXPECT_NEAR(0.3149f, matrix.Determinant(), 0.0001f);
}

TEST(Matrix3fTest, Inverse) {
  Matrix3F identity = Matrix3F::Identity();
  Matrix3F inv_identity = identity.Inverse();
  EXPECT_EQ(identity, inv_identity);

  Matrix3F singular = Matrix3F::Zeros();
  singular.set(1.0f, 3.0f, 4.0f,
               2.0f, 11.0f, 5.0f,
               0.5f, 1.5f, 2.0f);
  EXPECT_EQ(0, singular.Determinant());
  EXPECT_EQ(Matrix3F::Zeros(), singular.Inverse());

  Matrix3F regular = Matrix3F::Zeros();
  regular.set(0.57f,  0.205f,  0.942f,
              0.314f,  0.845f,  0.826f,
              0.131f,  0.025f,  0.962f);
  Matrix3F inv_regular = regular.Inverse();
  regular.set(2.51540616f, -0.55138018f, -1.98968043f,
              -0.61552266f,  1.34920184f, -0.55573636f,
              -0.32653861f,  0.04002158f,  1.32488726f);
  EXPECT_TRUE(regular.IsNear(inv_regular, 0.00001f));
}

TEST(Matrix3fTest, Transpose) {
  Matrix3F matrix = Matrix3F::Zeros();

  matrix.set(0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f);

  Matrix3F transpose = matrix.Transpose();
  EXPECT_EQ(Vector3dF(0.0f, 1.0f, 2.0f), transpose.get_column(0));
  EXPECT_EQ(Vector3dF(3.0f, 4.0f, 5.0f), transpose.get_column(1));
  EXPECT_EQ(Vector3dF(6.0f, 7.0f, 8.0f), transpose.get_column(2));

  EXPECT_TRUE(matrix.IsEqual(transpose.Transpose()));
}

TEST(Matrix3fTest, Operators) {
  Matrix3F matrix1 = Matrix3F::Zeros();
  matrix1.set(1, 2, 3, 4, 5, 6, 7, 8, 9);
  EXPECT_EQ(matrix1 + Matrix3F::Zeros(), matrix1);

  Matrix3F matrix2 = Matrix3F::Zeros();
  matrix2.set(-1, -2, -3, -4, -5, -6, -7, -8, -9);
  EXPECT_EQ(matrix1 + matrix2, Matrix3F::Zeros());

  EXPECT_EQ(Matrix3F::Zeros() - matrix1, matrix2);

  Matrix3F result = Matrix3F::Zeros();
  result.set(2, 4, 6, 8, 10, 12, 14, 16, 18);
  EXPECT_EQ(matrix1 - matrix2, result);
  result.set(-2, -4, -6, -8, -10, -12, -14, -16, -18);
  EXPECT_EQ(matrix2 - matrix1, result);
}

}  // namespace
}  // namespace gfx
