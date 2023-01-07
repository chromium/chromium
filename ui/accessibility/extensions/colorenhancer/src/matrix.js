// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Class that represents a 3x3 matrix. */
class Matrix3x3 {
  /** @private */
  constructor() {
    /** @private {!Array<!Array<number>>} */
    this.data_ = [[0, 0, 0], [0, 0, 0], [0, 0, 0]];
  }

  // ========== Constructor methods ===========

  /**
   * Returns a new matrix, with elements specified by the elementCalculator
   * function.
   * @param {!function(!Matrix3x3.Index, !Matrix3x3.Index): number}
   *     elementCalculator Given the i and j indices of the element, calculates
   *     the value for the new matrix.
   * @return {!Matrix3x3}
   */
  static fromElementwiseConstruction(elementCalculator) {
    const result = new Matrix3x3();
    for (const i of Matrix3x3.Indices) {
      for (const j of Matrix3x3.Indices) {
        result.data_[i][j] = elementCalculator(i, j);
      }
    }
    return result;
  }

  /**
   * Returns a new matrix with the provided data array.
   * @param {!Array<!Array<number>>} data A 3x3 array of numbers.
   * @return {!Matrix3x3}
   */
  static fromData(data) {
    const result = new Matrix3x3();
    result.data_ = data;
    return result;
  }

  // =============== operations ================

  /**
   * Adds the provided matrix with this matrix, and returns a new matrix
   * containing the result.
   * @param {!Matrix3x3} other
   * @return {!Matrix3x3} The matrix this + other.
   */
  add(other) {
    const adder = (i, j) => this.data_[i][j] + other.data_[i][j];
    return Matrix3x3.fromElementwiseConstruction(adder);
  }

  /**
   * Subtracts the given matrix from this one, and returns a new matrix
   * containing the result.
   * @param {!Matrix3x3} other
   * @return {!Matrix3x3} The matrix this - other.
   */
  subtract(other) {
    const subtracter = (i, j) => this.data_[i][j] - other.data_[i][j];
    return Matrix3x3.fromElementwiseConstruction(subtracter);
  }

  /**
   * Multiplies this matrix times the given matrix, and returns a new matrix
   * containing the result.
   * @param {!Matrix3x3} other
   * @return {!Matrix3x3} The matrix this * other.
   */
  multiply(other) {
    const multiplier = (i, j) => {
      let sum = 0;
      for (const k of Matrix3x3.Indices) {
        sum += this.data_[i][k] * other.data_[k][j];
      }
      return sum;
    };
    return Matrix3x3.fromElementwiseConstruction(multiplier);
  }

  /**
   * Scales this matrix by the provided scalar value, and returns a new matrix
   * containing the result.
   * @param {number} scaleFactor
   * @return {!Matrix3x3} The matrix scaleFactor * this.
   */
  scale(scaleFactor) {
    const scaler = (i, j) => scaleFactor * this.data_[i][j];
    return Matrix3x3.fromElementwiseConstruction(scaler);
  }

  // ============== Utils =============

  /**
   * Returns the value at a given pair of indices.
   * @param {Matrix3x3.Index} i
   * @param {Matrix3x3.Index} j
   * @return {number}
   */
  at(i, j) {
    return this.data_[i][j];
  }

  /**
   * Makes a human readable string for this matrix.
   * @override
   */
  toString() {
    // Fix the decimal precision at 2 digits.
    const fixedPrecisionData =
        this.data_.map(row => row.map(number => number.toFixed(2)));
    return JSON.stringify(fixedPrecisionData);
  }

  /**
   * Makes the SVG matrix string (of 20 values) for this matrix.
   * @return {string} The SVG matrix string.
   */
  toSvgString() {
    const outputRows = [];
    for (const i of Matrix3x3.Indices) {
      outputRows.push(this.data_[i].join(' ') + ' 0 0');
    }
    // Add the alpha row.
    outputRows.push('0 0 0 1 0');
    return outputRows.join(' ');
  }
}

/** @const {!Matrix3x3} */
Matrix3x3.IDENTITY = Matrix3x3.fromData([[1, 0, 0], [0, 1, 0], [0, 0, 1]]);

/** @enum {number} */
Matrix3x3.Index = {
  ZERO: 0,
  ONE: 1,
  TWO: 2,
};

Matrix3x3.Indices = Object.values(Matrix3x3.Index);
