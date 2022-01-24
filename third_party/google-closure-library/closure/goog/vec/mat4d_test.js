/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

////////////////////////// NOTE ABOUT EDITING THIS FILE ///////////////////////
//                                                                           //
// Any edits to this file must be applied to mat4f_test.js by running:       //
//   swap_type.sh mat4d_test.js > mat4f_test.js                              //
//                                                                           //
////////////////////////// NOTE ABOUT EDITING THIS FILE ///////////////////////

goog.module('goog.vec.mat4dTest');
goog.setTestOnly();

const Quaternion = goog.require('goog.vec.Quaternion');
const mat4d = goog.require('goog.vec.mat4d');
const testSuite = goog.require('goog.testing.testSuite');
const vec = goog.require('goog.vec');
const vec3d = goog.require('goog.vec.vec3d');
const vec4d = goog.require('goog.vec.vec4d');

const randommat4d = mat4d.setFromValues(
    mat4d.create(), 0.8025078773498535, 0.7559120655059814, 0.15274643898010254,
    0.19196106493473053, 0.0890120416879654, 0.15422114729881287,
    0.09754583984613419, 0.44862601161003113, 0.9196512699127197,
    0.5310639142990112, 0.8962187170982361, 0.280601441860199,
    0.594650387763977, 0.4134795069694519, 0.06632178276777267,
    0.8837796449661255);

testSuite({
  testCreate() {
    const m = mat4d.create();
    assertElementsEquals([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], m);
  },

  testCreateIdentity() {
    const m = mat4d.createIdentity();
    assertElementsEquals([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1], m);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSet() {
    const m0 = mat4d.create();
    const m1 = mat4d.setFromArray(
        mat4d.create(),
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]);
    mat4d.setFromArray(m0, m1);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m0);

    mat4d.setFromValues(
        m0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17);
    assertElementsEquals(
        [2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17], m0);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSetDiagonal() {
    const m0 = mat4d.create();
    mat4d.setDiagonalValues(m0, 1, 2, 3, 4);
    assertElementsEquals([1, 0, 0, 0, 0, 2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 4], m0);

    mat4d.setDiagonal(m0, [4, 5, 6, 7]);
    assertElementsEquals([4, 0, 0, 0, 0, 5, 0, 0, 0, 0, 6, 0, 0, 0, 0, 7], m0);
  },

  testGetDiagonal() {
    const v0 = vec4d.create();
    const m0 = mat4d.create();
    mat4d.setFromArray(
        m0, [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]);

    mat4d.getDiagonal(m0, v0);
    assertElementsEquals([0, 5, 10, 15], v0);

    vec4d.setFromArray(v0, [0, 0, 0, 0]);
    mat4d.getDiagonal(m0, v0, 1);
    assertElementsEquals([4, 9, 14, 0], v0);

    vec4d.setFromArray(v0, [0, 0, 0, 0]);
    mat4d.getDiagonal(m0, v0, 2);
    assertElementsEquals([8, 13, 0, 0], v0);

    vec4d.setFromArray(v0, [0, 0, 0, 0]);
    mat4d.getDiagonal(m0, v0, 3);
    assertElementsEquals([12, 0, 0, 0], v0);

    vec4d.setFromArray(v0, [0, 0, 0, 0]);
    mat4d.getDiagonal(m0, v0, 4);
    assertElementsEquals([0, 0, 0, 0], v0);

    vec4d.setFromArray(v0, [0, 0, 0, 0]);
    mat4d.getDiagonal(m0, v0, -1);
    assertElementsEquals([1, 6, 11, 0], v0);

    vec4d.setFromArray(v0, [0, 0, 0, 0]);
    mat4d.getDiagonal(m0, v0, -2);
    assertElementsEquals([2, 7, 0, 0], v0);

    vec4d.setFromArray(v0, [0, 0, 0, 0]);
    mat4d.getDiagonal(m0, v0, -3);
    assertElementsEquals([3, 0, 0, 0], v0);

    vec4d.setFromArray(v0, [0, 0, 0, 0]);
    mat4d.getDiagonal(m0, v0, -4);
    assertElementsEquals([0, 0, 0, 0], v0);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSetGetColumn() {
    const m0 = mat4d.create();
    mat4d.setColumn(m0, 0, [1, 2, 3, 4]);
    mat4d.setColumn(m0, 1, [5, 6, 7, 8]);
    mat4d.setColumn(m0, 2, [9, 10, 11, 12]);
    mat4d.setColumn(m0, 3, [13, 14, 15, 16]);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m0);

    const v0 = [0, 0, 0, 0];
    mat4d.getColumn(m0, 0, v0);
    assertElementsEquals([1, 2, 3, 4], v0);
    mat4d.getColumn(m0, 1, v0);
    assertElementsEquals([5, 6, 7, 8], v0);
    mat4d.getColumn(m0, 2, v0);
    assertElementsEquals([9, 10, 11, 12], v0);
    mat4d.getColumn(m0, 3, v0);
    assertElementsEquals([13, 14, 15, 16], v0);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSetGetColumns() {
    const m0 = mat4d.create();
    mat4d.setColumns(
        m0, [1, 2, 3, 4], [5, 6, 7, 8], [9, 10, 11, 12], [13, 14, 15, 16]);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m0);

    const v0 = [0, 0, 0, 0];
    const v1 = [0, 0, 0, 0];

    const v2 = [0, 0, 0, 0];
    const v3 = [0, 0, 0, 0];

    mat4d.getColumns(m0, v0, v1, v2, v3);
    assertElementsEquals([1, 2, 3, 4], v0);
    assertElementsEquals([5, 6, 7, 8], v1);
    assertElementsEquals([9, 10, 11, 12], v2);
    assertElementsEquals([13, 14, 15, 16], v3);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSetGetRow() {
    const m0 = mat4d.create();
    mat4d.setRow(m0, 0, [1, 2, 3, 4]);
    mat4d.setRow(m0, 1, [5, 6, 7, 8]);
    mat4d.setRow(m0, 2, [9, 10, 11, 12]);
    mat4d.setRow(m0, 3, [13, 14, 15, 16]);
    assertElementsEquals(
        [1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15, 4, 8, 12, 16], m0);

    const v0 = [0, 0, 0, 0];
    mat4d.getRow(m0, 0, v0);
    assertElementsEquals([1, 2, 3, 4], v0);
    mat4d.getRow(m0, 1, v0);
    assertElementsEquals([5, 6, 7, 8], v0);
    mat4d.getRow(m0, 2, v0);
    assertElementsEquals([9, 10, 11, 12], v0);
    mat4d.getRow(m0, 3, v0);
    assertElementsEquals([13, 14, 15, 16], v0);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSetGetRows() {
    const m0 = mat4d.create();
    mat4d.setRows(
        m0, [1, 2, 3, 4], [5, 6, 7, 8], [9, 10, 11, 12], [13, 14, 15, 16]);
    assertElementsEquals(
        [1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15, 4, 8, 12, 16], m0);

    const v0 = [0, 0, 0, 0];
    const v1 = [0, 0, 0, 0];

    const v2 = [0, 0, 0, 0];
    const v3 = [0, 0, 0, 0];

    mat4d.getRows(m0, v0, v1, v2, v3);
    assertElementsEquals([1, 2, 3, 4], v0);
    assertElementsEquals([5, 6, 7, 8], v1);
    assertElementsEquals([9, 10, 11, 12], v2);
    assertElementsEquals([13, 14, 15, 16], v3);
  },

  testMakeZero() {
    const m0 = mat4d.setFromArray(
        mat4d.create(),
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m0);
    mat4d.makeZero(m0);
    assertElementsEquals([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], m0);
  },

  testMakeIdentity() {
    const m0 = mat4d.create();
    mat4d.makeIdentity(m0);
    assertElementsEquals([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1], m0);
  },

  testSetGetElement() {
    const m0 = mat4d.create();
    for (let r = 0; r < 4; r++) {
      for (let c = 0; c < 4; c++) {
        const value = c * 4 + r + 1;
        mat4d.setElement(m0, r, c, value);
        assertEquals(value, mat4d.getElement(m0, r, c));
      }
    }
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m0);
  },

  testAddMat() {
    const m0 = mat4d.setFromValues(
        mat4d.create(), 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const m1 = mat4d.setFromValues(
        mat4d.create(), 9, 10, 11, 12, 13, 14, 15, 16, 1, 2, 3, 4, 5, 6, 7, 8);
    const m2 = mat4d.create();
    mat4d.addMat(m0, m1, m2);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m0);
    assertElementsEquals(
        [9, 10, 11, 12, 13, 14, 15, 16, 1, 2, 3, 4, 5, 6, 7, 8], m1);
    assertElementsEquals(
        [10, 12, 14, 16, 18, 20, 22, 24, 10, 12, 14, 16, 18, 20, 22, 24], m2);

    mat4d.addMat(m0, m1, m0);
    assertElementsEquals(
        [9, 10, 11, 12, 13, 14, 15, 16, 1, 2, 3, 4, 5, 6, 7, 8], m1);
    assertElementsEquals(
        [10, 12, 14, 16, 18, 20, 22, 24, 10, 12, 14, 16, 18, 20, 22, 24], m0);
  },

  testSubMat() {
    const m0 = mat4d.setFromValues(
        mat4d.create(), 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const m1 = mat4d.setFromValues(
        mat4d.create(), 9, 10, 11, 12, 13, 14, 15, 16, 1, 2, 3, 4, 5, 6, 7, 8);
    const m2 = mat4d.create();

    mat4d.subMat(m0, m1, m2);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m0);
    assertElementsEquals(
        [9, 10, 11, 12, 13, 14, 15, 16, 1, 2, 3, 4, 5, 6, 7, 8], m1);
    assertElementsEquals(
        [-8, -8, -8, -8, -8, -8, -8, -8, 8, 8, 8, 8, 8, 8, 8, 8], m2);

    mat4d.subMat(m1, m0, m1);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m0);
    assertElementsEquals(
        [8, 8, 8, 8, 8, 8, 8, 8, -8, -8, -8, -8, -8, -8, -8, -8], m1);
  },

  testMultScalar() {
    const m0 = mat4d.setFromValues(
        mat4d.create(), 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const m1 = mat4d.create();

    mat4d.multScalar(m0, 2, m1);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m0);
    assertElementsEquals(
        [2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32], m1);

    mat4d.multScalar(m0, 5, m0);
    assertElementsEquals(
        [5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80], m0);
  },

  testMultMat() {
    const m0 = mat4d.setFromValues(
        mat4d.create(), 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const m1 = mat4d.setFromValues(
        mat4d.create(), 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const m2 = mat4d.create();

    mat4d.multMat(m0, m1, m2);
    assertElementsEquals(
        [
          90, 100, 110, 120, 202, 228, 254, 280, 314, 356, 398, 440, 426, 484,
          542, 600
        ],
        m2);

    mat4d.multScalar(m1, 2, m1);
    mat4d.multMat(m1, m0, m1);
    assertElementsEquals(
        [
          180, 200, 220, 240, 404, 456, 508, 560, 628, 712, 796, 880, 852, 968,
          1084, 1200
        ],
        m1);
  },

  testTranspose() {
    const m0 = mat4d.setFromValues(
        mat4d.create(), 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const m1 = mat4d.create();
    mat4d.transpose(m0, m1);
    assertElementsEquals(
        [1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15, 4, 8, 12, 16], m1);

    mat4d.transpose(m1, m1);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m1);
  },

  testDeterminant() {
    const m0 = mat4d.setFromValues(
        mat4d.create(), 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1);
    assertEquals(0, mat4d.determinant(m0));
    assertElementsEquals([1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1], m0);

    mat4d.setFromValues(m0, 1, 2, 3, 4, 2, 3, 4, 1, 3, 4, 1, 2, 4, 1, 2, 3);
    assertEquals(160, mat4d.determinant(m0));
    assertElementsEquals([1, 2, 3, 4, 2, 3, 4, 1, 3, 4, 1, 2, 4, 1, 2, 3], m0);
  },

  testInvert() {
    const m0 = mat4d.setFromValues(
        mat4d.create(), 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1);
    assertFalse(mat4d.invert(m0, m0));
    assertElementsEquals([1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1], m0);

    mat4d.setFromValues(m0, 1, 2, 3, 4, 2, 3, 4, 1, 3, 4, 1, 2, 4, 1, 2, 3);
    assertTrue(mat4d.invert(m0, m0));
    assertElementsRoughlyEqual(
        [
          -0.225, 0.025, 0.025, 0.275, 0.025, 0.025, 0.275, -0.225, 0.025,
          0.275, -0.225, 0.025, 0.275, -0.225, 0.025, 0.025
        ],
        m0, vec.EPSILON);

    mat4d.makeScale(m0, .01, .01, .01);
    assertTrue(mat4d.invert(m0, m0));
    const m1 = mat4d.create();
    mat4d.makeScale(m1, 100, 100, 100);
    assertElementsEquals(m1, m0);
  },

  testEquals() {
    const m0 = mat4d.setFromValues(
        mat4d.create(), 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const m1 = mat4d.setFromMat4d(mat4d.create(), m0);
    assertTrue(mat4d.equals(m0, m1));
    assertTrue(mat4d.equals(m1, m0));
    for (let i = 0; i < 16; i++) {
      m1[i] = 18;
      assertFalse(mat4d.equals(m0, m1));
      assertFalse(mat4d.equals(m1, m0));
      m1[i] = i + 1;
    }
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testMultVec3() {
    const m0 = mat4d.setFromValues(
        mat4d.create(), 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const v0 = [1, 2, 3];
    const v1 = [0, 0, 0];

    mat4d.multVec3(m0, v0, v1);
    assertElementsEquals([1, 2, 3], v0);
    assertElementsEquals([51, 58, 65], v1);

    mat4d.multVec3(m0, v0, v0);
    assertElementsEquals([51, 58, 65], v0);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testMultVec3NoTranslate() {
    const m0 = mat4d.setFromValues(
        mat4d.create(), 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const v0 = [1, 2, 3];
    const v1 = [0, 0, 0];

    mat4d.multVec3NoTranslate(m0, v0, v1);
    assertElementsEquals([1, 2, 3], v0);
    assertElementsEquals([38, 44, 50], v1);

    mat4d.multVec3NoTranslate(m0, v0, v0);
    assertElementsEquals([38, 44, 50], v0);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testMultVec3Projective() {
    const m0 = mat4d.setFromValues(
        mat4d.create(), 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const v0 = [1, 2, 3];
    const v1 = [0, 0, 0];
    const invw = 1 / 72;

    mat4d.multVec3Projective(m0, v0, v1);
    assertElementsEquals([1, 2, 3], v0);
    assertElementsEquals([51 * invw, 58 * invw, 65 * invw], v1);

    mat4d.multVec3Projective(m0, v0, v0);
    assertElementsEquals([51 * invw, 58 * invw, 65 * invw], v0);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testMultVec4() {
    const m0 = mat4d.setFromValues(
        mat4d.create(), 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const v0 = [1, 2, 3, 4];
    const v1 = [0, 0, 0, 0];

    mat4d.multVec4(m0, v0, v1);
    assertElementsEquals([90, 100, 110, 120], v1);
    mat4d.multVec4(m0, v0, v0);
    assertElementsEquals([90, 100, 110, 120], v0);
  },

  testSetValues() {
    let a0 = mat4d.create();
    assertElementsEquals([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], a0);
    a0 = mat4d.setFromArray(
        mat4d.create(),
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], a0);

    const a1 = mat4d.create();
    mat4d.setDiagonalValues(a1, 1, 2, 3, 4);
    assertElementsEquals([1, 0, 0, 0, 0, 2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 4], a1);

    mat4d.setColumnValues(a1, 0, 2, 3, 4, 5);
    mat4d.setColumnValues(a1, 1, 6, 7, 8, 9);
    mat4d.setColumnValues(a1, 2, 10, 11, 12, 13);
    mat4d.setColumnValues(a1, 3, 14, 15, 16, 1);
    assertElementsEquals(
        [2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 1], a1);

    mat4d.setRowValues(a1, 0, 1, 5, 9, 13);
    mat4d.setRowValues(a1, 1, 2, 6, 10, 14);
    mat4d.setRowValues(a1, 2, 3, 7, 11, 15);
    mat4d.setRowValues(a1, 3, 4, 8, 12, 16);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], a1);
  },

  testMakeTranslate() {
    const m0 = mat4d.create();
    mat4d.makeTranslate(m0, 3, 4, 5);
    assertElementsEquals([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 3, 4, 5, 1], m0);
  },

  testMakeScale() {
    const m0 = mat4d.create();
    mat4d.makeScale(m0, 3, 4, 5);
    assertElementsEquals([3, 0, 0, 0, 0, 4, 0, 0, 0, 0, 5, 0, 0, 0, 0, 1], m0);
  },

  testMakeRotate() {
    const m0 = mat4d.create();
    mat4d.makeRotate(m0, Math.PI / 2, 0, 0, 1);
    assertElementsRoughlyEqual(
        [0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1], m0, vec.EPSILON);

    const m1 = mat4d.create();
    mat4d.makeRotate(m1, -Math.PI / 4, 0, 0, 1);
    mat4d.multMat(m0, m1, m1);
    assertElementsRoughlyEqual(
        [
          0.7071068, 0.7071068, 0, 0, -0.7071068, 0.7071068, 0, 0, 0, 0, 1, 0,
          0, 0, 0, 1
        ],
        m1, vec.EPSILON);
  },

  testMakeRotateX() {
    const m0 = mat4d.create();
    const m1 = mat4d.create();

    mat4d.makeRotateX(m0, Math.PI / 7);
    mat4d.makeRotate(m1, Math.PI / 7, 1, 0, 0);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },

  testMakeRotateY() {
    const m0 = mat4d.create();
    const m1 = mat4d.create();

    mat4d.makeRotateY(m0, Math.PI / 7);
    mat4d.makeRotate(m1, Math.PI / 7, 0, 1, 0);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },

  testMakeRotateZ() {
    const m0 = mat4d.create();
    const m1 = mat4d.create();

    mat4d.makeRotateZ(m0, Math.PI / 7);
    mat4d.makeRotate(m1, Math.PI / 7, 0, 0, 1);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },

  testTranslate() {
    const m0 = mat4d.makeIdentity(mat4d.create());
    mat4d.translate(m0, 3, 4, 5);
    assertElementsEquals([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 3, 4, 5, 1], m0);

    mat4d.setFromValues(m0, 1, 2, 3, 4, 2, 3, 4, 1, 3, 4, 1, 2, 4, 1, 2, 3);

    const m1 = mat4d.create();
    mat4d.makeTranslate(m1, 5, 6, 7);
    const m2 = mat4d.create();
    mat4d.multMat(m0, m1, m2);
    mat4d.translate(m0, 5, 6, 7);
    assertElementsEquals(m2, m0);
  },

  testScale() {
    const m0 = mat4d.makeIdentity(mat4d.create());
    mat4d.scale(m0, 3, 4, 5);
    assertElementsEquals([3, 0, 0, 0, 0, 4, 0, 0, 0, 0, 5, 0, 0, 0, 0, 1], m0);
  },

  testRotate() {
    const m0 = mat4d.makeIdentity(mat4d.create());
    mat4d.rotate(m0, Math.PI / 2, 0, 0, 1);
    assertElementsRoughlyEqual(
        [0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1], m0, vec.EPSILON);

    mat4d.rotate(m0, -Math.PI / 4, 0, 0, 1);
    assertElementsRoughlyEqual(
        [
          0.7071068, 0.7071068, 0, 0, -0.7071068, 0.7071068, 0, 0, 0, 0, 1, 0,
          0, 0, 0, 1
        ],
        m0, vec.EPSILON);
  },

  testRotateX() {
    const m0 = mat4d.create();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const m1 = mat4d.setFromArray(mat4d.create(), randommat4d);

    mat4d.makeRotateX(m0, Math.PI / 7);
    mat4d.multMat(m1, m0, m0);
    mat4d.rotateX(m1, Math.PI / 7);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },

  testRotateY() {
    const m0 = mat4d.create();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const m1 = mat4d.setFromArray(mat4d.create(), randommat4d);

    mat4d.makeRotateY(m0, Math.PI / 7);
    mat4d.multMat(m1, m0, m0);
    mat4d.rotateY(m1, Math.PI / 7);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },

  testRotateZ() {
    const m0 = mat4d.create();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const m1 = mat4d.setFromArray(mat4d.create(), randommat4d);

    mat4d.makeRotateZ(m0, Math.PI / 7);
    mat4d.multMat(m1, m0, m0);
    mat4d.rotateZ(m1, Math.PI / 7);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },

  testMakeRotationTranslation() {
    // Create manually.
    const m0 = mat4d.makeIdentity(mat4d.create());
    mat4d.translate(m0, 3, 4, 5);
    mat4d.rotate(m0, Math.PI / 2, 3 / 13, 4 / 13, 12 / 13);

    // Create using makeRotationTranslation.
    const m1 = mat4d.create();
    const q = Quaternion.createFloat64();
    const axis = vec3d.createFromValues(3 / 13, 4 / 13, 12 / 13);
    Quaternion.fromAngleAxis(Math.PI / 2, axis, q);
    const v = vec3d.createFromValues(3, 4, 5);
    mat4d.makeRotationTranslation(m1, q, v);

    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },

  testMakeRotationTranslationScale() {
    // Create manually.
    const m0 = mat4d.makeIdentity(mat4d.create());
    mat4d.translate(m0, 3, 4, 5);
    mat4d.rotate(m0, Math.PI / 2, 3 / 13, 4 / 13, 12 / 13);
    mat4d.scale(m0, 6, 7, 8);

    // Create using makeRotationTranslationScale.
    const m1 = mat4d.create();
    const q = Quaternion.createFloat64();
    const axis = vec3d.createFromValues(3 / 13, 4 / 13, 12 / 13);
    Quaternion.fromAngleAxis(Math.PI / 2, axis, q);
    const v = vec3d.createFromValues(3, 4, 5);
    const s = vec3d.createFromValues(6, 7, 8);
    mat4d.makeRotationTranslationScale(m1, q, v, s);

    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },

  testMakeRotationTranslationScaleOrigin() {
    // Create manually.
    const m0 = mat4d.makeIdentity(mat4d.create());
    mat4d.translate(m0, 3, 4, 5);
    mat4d.translate(m0, 9, 10, -11);  // Origin.
    mat4d.rotate(m0, Math.PI / 2, 3 / 13, 4 / 13, 12 / 13);
    mat4d.scale(m0, 6, 7, 8);
    mat4d.translate(m0, -9, -10, 11);  // -Origin.

    // Create using makeRotationTranslationScaleOrigin.
    const m1 = mat4d.create();
    const q = Quaternion.createFloat64();
    const axis = vec3d.createFromValues(3 / 13, 4 / 13, 12 / 13);
    Quaternion.fromAngleAxis(Math.PI / 2, axis, q);
    const v = vec3d.createFromValues(3, 4, 5);
    const s = vec3d.createFromValues(6, 7, 8);
    const o = vec3d.createFromValues(9, 10, -11);
    mat4d.makeRotationTranslationScaleOrigin(m1, q, v, s, o);

    assertElementsRoughlyEqual(m0, m1, 0.00001);  // Slightly larger epsilon.
  },

  testGetTranslation() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const mat = mat4d.setFromArray(mat4d.create(), randommat4d);
    const translation = vec3d.create();
    mat4d.getTranslation(mat, translation);
    assertElementsRoughlyEqual(
        [0.59465038776, 0.413479506969, 0.0663217827677], translation,
        vec.EPSILON);
  },

  testMakeFrustum() {
    const m0 = mat4d.create();
    mat4d.makeFrustum(m0, -1, 2, -2, 1, .1, 1.1);
    assertElementsRoughlyEqual(
        [
          0.06666666, 0, 0, 0, 0, 0.06666666, 0, 0, 0.33333333, -0.33333333,
          -1.2, -1, 0, 0, -0.22, 0
        ],
        m0, vec.EPSILON);
  },

  testMakePerspective() {
    const m0 = mat4d.create();
    mat4d.makePerspective(m0, 90 * Math.PI / 180, 2, 0.1, 1.1);
    assertElementsRoughlyEqual(
        [0.5, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1.2, -1, 0, 0, -0.22, 0], m0,
        vec.EPSILON);
  },

  testMakeOrtho() {
    const m0 = mat4d.create();
    mat4d.makeOrtho(m0, -1, 2, -2, 1, 0.1, 1.1);
    assertElementsRoughlyEqual(
        [
          0.6666666, 0, 0, 0, 0, 0.6666666, 0, 0, 0, 0, -2, 0, -0.333333,
          0.3333333, -1.2, 1
        ],
        m0, vec.EPSILON);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testMakeEulerZXZ() {
    const m0 = mat4d.create();
    const roll = 0.200982 * 2 * Math.PI;
    const tilt = 0.915833 * Math.PI;
    const yaw = 0.839392 * 2 * Math.PI;

    mat4d.makeRotate(m0, roll, 0, 0, 1);
    mat4d.rotate(m0, tilt, 1, 0, 0);
    mat4d.rotate(m0, yaw, 0, 0, 1);

    const m1 = mat4d.create();
    mat4d.makeEulerZXZ(m1, roll, tilt, yaw);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);

    let euler = [0, 0, 0];
    mat4d.toEulerZXZ(m0, euler);

    assertRoughlyEquals(roll, euler[0], vec.EPSILON);
    assertRoughlyEquals(tilt, euler[1], vec.EPSILON);
    assertRoughlyEquals(yaw, euler[2], vec.EPSILON);

    // Test negative tilt now.
    mat4d.makeRotate(m0, roll, 0, 0, 1);
    mat4d.rotate(m0, -tilt, 1, 0, 0);
    mat4d.rotate(m0, yaw, 0, 0, 1);

    mat4d.makeEulerZXZ(m1, roll, -tilt, yaw);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);

    euler = [0, 0, 0];
    mat4d.toEulerZXZ(m0, euler, true);

    assertRoughlyEquals(roll, euler[0], vec.EPSILON);
    assertRoughlyEquals(-tilt, euler[1], vec.EPSILON);
    assertRoughlyEquals(yaw, euler[2], vec.EPSILON);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testEulerZXZExtrema() {
    const m0 = mat4d.setFromArray(
        mat4d.create(), [1, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 1]);
    const m1 = mat4d.setFromArray(
        mat4d.create(), [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);

    const euler = [0, 0, 0];
    mat4d.toEulerZXZ(m0, euler);
    assertElementsRoughlyEqual(
        [Math.PI, Math.PI / 2, Math.PI], euler, vec.EPSILON);
    mat4d.makeEulerZXZ(m1, euler[0], euler[1], euler[2]);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testLookAt() {
    const viewMatrix = mat4d.create();
    mat4d.makeLookAt(viewMatrix, [0, 0, 0], [1, 0, 0], [0, 1, 0]);
    assertElementsRoughlyEqual(
        [0, 0, -1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1], viewMatrix,
        vec.EPSILON);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testToLookAt() {
    // This test does not use the default precision goog.vec.EPSILON due to
    // precision issues in some browsers leading to flaky tests.
    const EPSILON = 1e-4;

    const eyeExp = [0, 0, 0];
    const fwdExp = [1, 0, 0];
    const upExp = [0, 1, 0];

    const centerExp = [0, 0, 0];
    vec3d.add(eyeExp, fwdExp, centerExp);

    const view = mat4d.create();
    mat4d.makeLookAt(view, eyeExp, centerExp, upExp);

    const eyeRes = [0, 0, 0];
    const fwdRes = [0, 0, 0];
    const upRes = [0, 0, 0];
    mat4d.toLookAt(view, eyeRes, fwdRes, upRes);
    assertElementsRoughlyEqual(eyeExp, eyeRes, EPSILON);
    assertElementsRoughlyEqual(fwdExp, fwdRes, EPSILON);
    assertElementsRoughlyEqual(upExp, upRes, EPSILON);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testLookAtDecomposition() {
    // This test does not use the default precision goog.vec.EPSILON due to
    // precision issues in some browsers leading to flaky tests.
    const EPSILON = 1e-4;

    const viewExp = mat4d.create();
    const viewRes = mat4d.create();

    // Get a valid set of random vectors eye, forward, up by decomposing
    // a random matrix into a set of lookAt vectors.
    /** @suppress {checkTypes} suppression added to enable type checking */
    const tmp = mat4d.setFromArray(mat4d.create(), randommat4d);
    const eyeExp = [0, 0, 0];
    const fwdExp = [0, 0, 0];
    const upExp = [0, 0, 0];
    const centerExp = [0, 0, 0];
    // Project the random matrix into a real modelview matrix.
    mat4d.toLookAt(tmp, eyeExp, fwdExp, upExp);
    vec3d.add(eyeExp, fwdExp, centerExp);

    // Compute the expected modelview matrix from a set of valid random vectors.
    mat4d.makeLookAt(viewExp, eyeExp, centerExp, upExp);

    const eyeRes = [0, 0, 0];
    const fwdRes = [0, 0, 0];
    const upRes = [0, 0, 0];
    const centerRes = [0, 0, 0];
    mat4d.toLookAt(viewExp, eyeRes, fwdRes, upRes);
    vec3d.add(eyeRes, fwdRes, centerRes);

    mat4d.makeLookAt(viewRes, eyeRes, centerRes, upRes);

    assertElementsRoughlyEqual(eyeExp, eyeRes, EPSILON);
    assertElementsRoughlyEqual(fwdExp, fwdRes, EPSILON);
    assertElementsRoughlyEqual(upExp, upRes, EPSILON);
    assertElementsRoughlyEqual(viewExp, viewRes, EPSILON);
  },
});
