// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['matrix.js']);

GEN_INCLUDE(['../../webstore_extension_test_base.js']);

/** Test fixture for matrix.js. */
Matrix3x3UnitTest = class extends WebstoreExtensionTest {};

/**
 * @param {!Array<!Array<number>>} expected
 * @param {!Matrix3x3} actual
 */
function checkMatricesAreEqual(expected, actual) {
  assertTrue(!!actual);
  for (const i of [0, 1, 2]) {
    for (const j of [0, 1, 2]) {
      assertEquals(expected[i][j], actual.at(i, j));
    }
  }
}

TEST_F('Matrix3x3UnitTest', 'Constructors', function() {
  // Check that the identity matrix was constructed correctly.
  checkMatricesAreEqual([[1, 0, 0], [0, 1, 0], [0, 0, 1]], Matrix3x3.IDENTITY);

  // Check that construction from a data array is as expected.
  const dataArray = [[1, 2, 3], [4, 5, 6], [7, 8, 9]];
  checkMatricesAreEqual(dataArray, Matrix3x3.fromData(dataArray));

  // Check that elementwise construction is as expected.
  const elementBuilder = (i, j) => 3 * i + j;
  checkMatricesAreEqual(
      [[0, 1, 2], [3, 4, 5], [6, 7, 8]],
      Matrix3x3.fromElementwiseConstruction(elementBuilder));
});

TEST_F('Matrix3x3UnitTest', 'Operations', function() {
  // Check that adding two matrices together gets the desired results.
  const matrix1 = Matrix3x3.fromElementwiseConstruction((i, j) => 3 * i + j);
  const matrix2 = Matrix3x3.fromElementwiseConstruction((i, j) => 1);
  checkMatricesAreEqual(
      [[1, 2, 3], [4, 5, 6], [7, 8, 9]], matrix1.add(matrix2));

  // Check that subtracting one matrix from another gets the desired result.
  checkMatricesAreEqual(
      [[-1, 0, 1], [2, 3, 4], [5, 6, 7]], matrix1.subtract(matrix2));

  // Check that multiplying one matrix by the other gets the desired result.
  // Specifically, multiplying by a matrix that is all ones will result in every
  // element in the row having the same value -- the sum of the elements in the
  // non-one matrix provided.
  const row1 = 0 + 1 + 2;
  const row2 = 3 + 4 + 5;
  const row3 = 6 + 7 + 8;
  checkMatricesAreEqual(
      [[row1, row1, row1], [row2, row2, row2], [row3, row3, row3]],
      matrix1.multiply(matrix2));

  // If we reverse the order of the matrices, we instead get the columns having
  // the same value (the sum of the elements in the column).
  const col1 = 0 + 3 + 6;
  const col2 = 1 + 4 + 7;
  const col3 = 2 + 5 + 8;
  checkMatricesAreEqual(
      [[col1, col2, col3], [col1, col2, col3], [col1, col2, col3]],
      matrix2.multiply(matrix1));

  // Check that scaling the matrix gives the desired results.
  checkMatricesAreEqual(
      [[0, 10, 20], [30, 40, 50], [60, 70, 80]], matrix1.scale(10));
});

TEST_F('Matrix3x3UnitTest', 'ToSvgString', function() {
  assertEquals(
      '1 0 0 0 0 ' +
          '0 1 0 0 0 ' +
          '0 0 1 0 0 ' +
          '0 0 0 1 0',
      Matrix3x3.IDENTITY.toSvgString());
});
