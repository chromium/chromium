/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.vec.Mat4Test');
goog.setTestOnly();

const Mat4 = goog.require('goog.vec.Mat4');
const Vec3 = goog.require('goog.vec.Vec3');
const Vec4 = goog.require('goog.vec.Vec4');
const testSuite = goog.require('goog.testing.testSuite');
const vec = goog.require('goog.vec');

const randomMat4 = Mat4.createFloat32FromValues(
    0.8025078773498535, 0.7559120655059814, 0.15274643898010254,
    0.19196106493473053, 0.0890120416879654, 0.15422114729881287,
    0.09754583984613419, 0.44862601161003113, 0.9196512699127197,
    0.5310639142990112, 0.8962187170982361, 0.280601441860199,
    0.594650387763977, 0.4134795069694519, 0.06632178276777267,
    0.8837796449661255);

testSuite({
  testDeprecatedConstructor() {
    const m0 = Mat4.create();
    assertElementsEquals([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], m0);

    const m1 = Mat4.createFromArray(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m1);

    const m2 = Mat4.clone(m1);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m1);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m2);

    const m3 = Mat4.createFromValues(
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m3);

    const m4 = Mat4.createIdentity();
    assertElementsEquals([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1], m4);
  },

  testConstructor() {
    const m0 = Mat4.createFloat32();
    assertElementsEquals([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], m0);

    const m1 = Mat4.createFloat32FromArray(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m1);

    const m2 = Mat4.clone(m1);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m1);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m2);

    const m3 = Mat4.createFloat32FromValues(
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m3);

    const m4 = Mat4.createIdentity();
    assertElementsEquals([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1], m4);
  },

  testSet() {
    const m0 = Mat4.createFloat32();
    const m1 = Mat4.createFloat32FromArray(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]);
    Mat4.setFromArray(m0, m1);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m0);

    Mat4.setFromValues(
        m0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17);
    assertElementsEquals(
        [2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17], m0);
  },

  testSetDiagonal() {
    const m0 = Mat4.createFloat32();
    Mat4.setDiagonalValues(m0, 1, 2, 3, 4);
    assertElementsEquals([1, 0, 0, 0, 0, 2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 4], m0);

    Mat4.setDiagonal(m0, [4, 5, 6, 7]);
    assertElementsEquals([4, 0, 0, 0, 0, 5, 0, 0, 0, 0, 6, 0, 0, 0, 0, 7], m0);
  },

  testGetDiagonal() {
    const v0 = Vec4.create();
    const m0 = Mat4.createFloat32();
    Mat4.setFromArray(
        m0, [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]);

    Mat4.getDiagonal(m0, v0);
    assertElementsEquals([0, 5, 10, 15], v0);

    Vec4.setFromArray(v0, [0, 0, 0, 0]);
    Mat4.getDiagonal(m0, v0, 1);
    assertElementsEquals([4, 9, 14, 0], v0);

    Vec4.setFromArray(v0, [0, 0, 0, 0]);
    Mat4.getDiagonal(m0, v0, 2);
    assertElementsEquals([8, 13, 0, 0], v0);

    Vec4.setFromArray(v0, [0, 0, 0, 0]);
    Mat4.getDiagonal(m0, v0, 3);
    assertElementsEquals([12, 0, 0, 0], v0);

    Vec4.setFromArray(v0, [0, 0, 0, 0]);
    Mat4.getDiagonal(m0, v0, 4);
    assertElementsEquals([0, 0, 0, 0], v0);

    Vec4.setFromArray(v0, [0, 0, 0, 0]);
    Mat4.getDiagonal(m0, v0, -1);
    assertElementsEquals([1, 6, 11, 0], v0);

    Vec4.setFromArray(v0, [0, 0, 0, 0]);
    Mat4.getDiagonal(m0, v0, -2);
    assertElementsEquals([2, 7, 0, 0], v0);

    Vec4.setFromArray(v0, [0, 0, 0, 0]);
    Mat4.getDiagonal(m0, v0, -3);
    assertElementsEquals([3, 0, 0, 0], v0);

    Vec4.setFromArray(v0, [0, 0, 0, 0]);
    Mat4.getDiagonal(m0, v0, -4);
    assertElementsEquals([0, 0, 0, 0], v0);
  },

  testSetGetColumn() {
    const m0 = Mat4.createFloat32();
    Mat4.setColumn(m0, 0, [1, 2, 3, 4]);
    Mat4.setColumn(m0, 1, [5, 6, 7, 8]);
    Mat4.setColumn(m0, 2, [9, 10, 11, 12]);
    Mat4.setColumn(m0, 3, [13, 14, 15, 16]);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m0);

    const v0 = [0, 0, 0, 0];
    Mat4.getColumn(m0, 0, v0);
    assertElementsEquals([1, 2, 3, 4], v0);
    Mat4.getColumn(m0, 1, v0);
    assertElementsEquals([5, 6, 7, 8], v0);
    Mat4.getColumn(m0, 2, v0);
    assertElementsEquals([9, 10, 11, 12], v0);
    Mat4.getColumn(m0, 3, v0);
    assertElementsEquals([13, 14, 15, 16], v0);
  },

  testSetGetColumns() {
    const m0 = Mat4.createFloat32();
    Mat4.setColumns(
        m0, [1, 2, 3, 4], [5, 6, 7, 8], [9, 10, 11, 12], [13, 14, 15, 16]);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m0);

    const v0 = [0, 0, 0, 0];
    const v1 = [0, 0, 0, 0];

    const v2 = [0, 0, 0, 0];
    const v3 = [0, 0, 0, 0];

    Mat4.getColumns(m0, v0, v1, v2, v3);
    assertElementsEquals([1, 2, 3, 4], v0);
    assertElementsEquals([5, 6, 7, 8], v1);
    assertElementsEquals([9, 10, 11, 12], v2);
    assertElementsEquals([13, 14, 15, 16], v3);
  },

  testSetGetRow() {
    const m0 = Mat4.createFloat32();
    Mat4.setRow(m0, 0, [1, 2, 3, 4]);
    Mat4.setRow(m0, 1, [5, 6, 7, 8]);
    Mat4.setRow(m0, 2, [9, 10, 11, 12]);
    Mat4.setRow(m0, 3, [13, 14, 15, 16]);
    assertElementsEquals(
        [1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15, 4, 8, 12, 16], m0);

    const v0 = [0, 0, 0, 0];
    Mat4.getRow(m0, 0, v0);
    assertElementsEquals([1, 2, 3, 4], v0);
    Mat4.getRow(m0, 1, v0);
    assertElementsEquals([5, 6, 7, 8], v0);
    Mat4.getRow(m0, 2, v0);
    assertElementsEquals([9, 10, 11, 12], v0);
    Mat4.getRow(m0, 3, v0);
    assertElementsEquals([13, 14, 15, 16], v0);
  },

  testSetGetRows() {
    const m0 = Mat4.createFloat32();
    Mat4.setRows(
        m0, [1, 2, 3, 4], [5, 6, 7, 8], [9, 10, 11, 12], [13, 14, 15, 16]);
    assertElementsEquals(
        [1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15, 4, 8, 12, 16], m0);

    const v0 = [0, 0, 0, 0];
    const v1 = [0, 0, 0, 0];

    const v2 = [0, 0, 0, 0];
    const v3 = [0, 0, 0, 0];

    Mat4.getRows(m0, v0, v1, v2, v3);
    assertElementsEquals([1, 2, 3, 4], v0);
    assertElementsEquals([5, 6, 7, 8], v1);
    assertElementsEquals([9, 10, 11, 12], v2);
    assertElementsEquals([13, 14, 15, 16], v3);
  },

  testSetRowMajorArray() {
    const m0 = Mat4.createFloat32();
    Mat4.setFromRowMajorArray(
        m0, [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]);
    assertElementsEquals(
        [1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15, 4, 8, 12, 16], m0);
  },

  testMakeZero() {
    const m0 = Mat4.createFloat32FromArray(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m0);
    Mat4.makeZero(m0);
    assertElementsEquals([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], m0);
  },

  testMakeIdentity() {
    const m0 = Mat4.createFloat32();
    Mat4.makeIdentity(m0);
    assertElementsEquals([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1], m0);
  },

  testSetGetElement() {
    const m0 = Mat4.createFloat32();
    for (let r = 0; r < 4; r++) {
      for (let c = 0; c < 4; c++) {
        const value = c * 4 + r + 1;
        Mat4.setElement(m0, r, c, value);
        assertEquals(value, Mat4.getElement(m0, r, c));
      }
    }
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m0);
  },

  testAddMat() {
    const m0 = Mat4.createFloat32FromValues(
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const m1 = Mat4.createFloat32FromValues(
        9, 10, 11, 12, 13, 14, 15, 16, 1, 2, 3, 4, 5, 6, 7, 8);
    const m2 = Mat4.createFloat32();
    Mat4.addMat(m0, m1, m2);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m0);
    assertElementsEquals(
        [9, 10, 11, 12, 13, 14, 15, 16, 1, 2, 3, 4, 5, 6, 7, 8], m1);
    assertElementsEquals(
        [10, 12, 14, 16, 18, 20, 22, 24, 10, 12, 14, 16, 18, 20, 22, 24], m2);

    Mat4.addMat(m0, m1, m0);
    assertElementsEquals(
        [9, 10, 11, 12, 13, 14, 15, 16, 1, 2, 3, 4, 5, 6, 7, 8], m1);
    assertElementsEquals(
        [10, 12, 14, 16, 18, 20, 22, 24, 10, 12, 14, 16, 18, 20, 22, 24], m0);
  },

  testSubMat() {
    const m0 = Mat4.createFloat32FromValues(
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const m1 = Mat4.createFloat32FromValues(
        9, 10, 11, 12, 13, 14, 15, 16, 1, 2, 3, 4, 5, 6, 7, 8);
    const m2 = Mat4.createFloat32();

    Mat4.subMat(m0, m1, m2);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m0);
    assertElementsEquals(
        [9, 10, 11, 12, 13, 14, 15, 16, 1, 2, 3, 4, 5, 6, 7, 8], m1);
    assertElementsEquals(
        [-8, -8, -8, -8, -8, -8, -8, -8, 8, 8, 8, 8, 8, 8, 8, 8], m2);

    Mat4.subMat(m1, m0, m1);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m0);
    assertElementsEquals(
        [8, 8, 8, 8, 8, 8, 8, 8, -8, -8, -8, -8, -8, -8, -8, -8], m1);
  },

  testMultScalar() {
    const m0 = Mat4.createFloat32FromValues(
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const m1 = Mat4.createFloat32();

    Mat4.multScalar(m0, 2, m1);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m0);
    assertElementsEquals(
        [2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32], m1);

    Mat4.multScalar(m0, 5, m0);
    assertElementsEquals(
        [5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80], m0);
  },

  testMultMat() {
    const m0 = Mat4.createFloat32FromValues(
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const m1 = Mat4.createFloat32FromValues(
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const m2 = Mat4.createFloat32();

    Mat4.multMat(m0, m1, m2);
    assertElementsEquals(
        [
          90, 100, 110, 120, 202, 228, 254, 280, 314, 356, 398, 440, 426, 484,
          542, 600
        ],
        m2);

    Mat4.multScalar(m1, 2, m1);
    Mat4.multMat(m1, m0, m1);
    assertElementsEquals(
        [
          180, 200, 220, 240, 404, 456, 508, 560, 628, 712, 796, 880, 852, 968,
          1084, 1200
        ],
        m1);
  },

  testTranspose() {
    const m0 = Mat4.createFloat32FromValues(
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const m1 = Mat4.createFloat32();
    Mat4.transpose(m0, m1);
    assertElementsEquals(
        [1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15, 4, 8, 12, 16], m1);

    Mat4.transpose(m1, m1);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m1);
  },

  testDeterminant() {
    const m0 = Mat4.createFloat32FromValues(
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1);
    assertEquals(0, Mat4.determinant(m0));
    assertElementsEquals([1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1], m0);

    Mat4.setFromValues(m0, 1, 2, 3, 4, 2, 3, 4, 1, 3, 4, 1, 2, 4, 1, 2, 3);
    assertEquals(160, Mat4.determinant(m0));
    assertElementsEquals([1, 2, 3, 4, 2, 3, 4, 1, 3, 4, 1, 2, 4, 1, 2, 3], m0);
  },

  testInvert() {
    const m0 = Mat4.createFloat32FromValues(
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1);
    assertFalse(Mat4.invert(m0, m0));
    assertElementsEquals([1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1], m0);

    Mat4.setFromValues(m0, 1, 2, 3, 4, 2, 3, 4, 1, 3, 4, 1, 2, 4, 1, 2, 3);
    assertTrue(Mat4.invert(m0, m0));
    assertElementsRoughlyEqual(
        [
          -0.225, 0.025, 0.025, 0.275, 0.025, 0.025, 0.275, -0.225, 0.025,
          0.275, -0.225, 0.025, 0.275, -0.225, 0.025, 0.025
        ],
        m0, vec.EPSILON);

    Mat4.makeScale(m0, .01, .01, .01);
    assertTrue(Mat4.invert(m0, m0));
    const m1 = Mat4.createFloat32();
    Mat4.makeScale(m1, 100, 100, 100);
    assertElementsEquals(m1, m0);
  },

  testEquals() {
    const m0 = Mat4.createFloat32FromValues(
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const m1 = Mat4.clone(m0);
    assertTrue(Mat4.equals(m0, m1));
    assertTrue(Mat4.equals(m1, m0));
    for (let i = 0; i < 16; i++) {
      m1[i] = 18;
      assertFalse(Mat4.equals(m0, m1));
      assertFalse(Mat4.equals(m1, m0));
      m1[i] = i + 1;
    }
  },

  testMultVec3() {
    const m0 = Mat4.createFloat32FromValues(
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const v0 = [1, 2, 3];
    const v1 = [0, 0, 0];

    Mat4.multVec3(m0, v0, v1);
    assertElementsEquals([1, 2, 3], v0);
    assertElementsEquals([51, 58, 65], v1);

    Mat4.multVec3(m0, v0, v0);
    assertElementsEquals([51, 58, 65], v0);
  },

  testMultVec3NoTranslate() {
    const m0 = Mat4.createFloat32FromValues(
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const v0 = [1, 2, 3];
    const v1 = [0, 0, 0];

    Mat4.multVec3NoTranslate(m0, v0, v1);
    assertElementsEquals([1, 2, 3], v0);
    assertElementsEquals([38, 44, 50], v1);

    Mat4.multVec3NoTranslate(m0, v0, v0);
    assertElementsEquals([38, 44, 50], v0);
  },

  testMultVec3Projective() {
    const m0 = Mat4.createFloat32FromValues(
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const v0 = [1, 2, 3];
    const v1 = [0, 0, 0];
    const invw = 1 / 72;

    Mat4.multVec3Projective(m0, v0, v1);
    assertElementsEquals([1, 2, 3], v0);
    assertElementsEquals([51 * invw, 58 * invw, 65 * invw], v1);

    Mat4.multVec3Projective(m0, v0, v0);
    assertElementsEquals([51 * invw, 58 * invw, 65 * invw], v0);
  },

  testMultVec4() {
    const m0 = Mat4.createFloat32FromValues(
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    const v0 = [1, 2, 3, 4];
    const v1 = [0, 0, 0, 0];

    Mat4.multVec4(m0, v0, v1);
    assertElementsEquals([90, 100, 110, 120], v1);
    Mat4.multVec4(m0, v0, v0);
    assertElementsEquals([90, 100, 110, 120], v0);
  },

  testSetValues() {
    let a0 = Mat4.createFloat32();
    assertElementsEquals([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], a0);
    a0 = Mat4.createFloat32FromArray(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], a0);

    const a1 = Mat4.createFloat32();
    Mat4.setDiagonalValues(a1, 1, 2, 3, 4);
    assertElementsEquals([1, 0, 0, 0, 0, 2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 4], a1);

    Mat4.setColumnValues(a1, 0, 2, 3, 4, 5);
    Mat4.setColumnValues(a1, 1, 6, 7, 8, 9);
    Mat4.setColumnValues(a1, 2, 10, 11, 12, 13);
    Mat4.setColumnValues(a1, 3, 14, 15, 16, 1);
    assertElementsEquals(
        [2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 1], a1);

    Mat4.setRowValues(a1, 0, 1, 5, 9, 13);
    Mat4.setRowValues(a1, 1, 2, 6, 10, 14);
    Mat4.setRowValues(a1, 2, 3, 7, 11, 15);
    Mat4.setRowValues(a1, 3, 4, 8, 12, 16);
    assertElementsEquals(
        [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], a1);
  },

  testMakeTranslate() {
    const m0 = Mat4.createFloat32();
    Mat4.makeTranslate(m0, 3, 4, 5);
    assertElementsEquals([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 3, 4, 5, 1], m0);
  },

  testMakeScale() {
    const m0 = Mat4.createFloat32();
    Mat4.makeScale(m0, 3, 4, 5);
    assertElementsEquals([3, 0, 0, 0, 0, 4, 0, 0, 0, 0, 5, 0, 0, 0, 0, 1], m0);
  },

  testMakeRotate() {
    const m0 = Mat4.createFloat32();
    Mat4.makeRotate(m0, Math.PI / 2, 0, 0, 1);
    assertElementsRoughlyEqual(
        [0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1], m0, vec.EPSILON);

    const m1 = Mat4.createFloat32();
    Mat4.makeRotate(m1, -Math.PI / 4, 0, 0, 1);
    Mat4.multMat(m0, m1, m1);
    assertElementsRoughlyEqual(
        [
          0.7071068, 0.7071068, 0, 0, -0.7071068, 0.7071068, 0, 0, 0, 0, 1, 0,
          0, 0, 0, 1
        ],
        m1, vec.EPSILON);
  },

  testMakeRotateX() {
    const m0 = Mat4.createFloat32();
    const m1 = Mat4.createFloat32();

    Mat4.makeRotateX(m0, Math.PI / 7);
    Mat4.makeRotate(m1, Math.PI / 7, 1, 0, 0);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },

  testMakeRotateY() {
    const m0 = Mat4.createFloat32();
    const m1 = Mat4.createFloat32();

    Mat4.makeRotateY(m0, Math.PI / 7);
    Mat4.makeRotate(m1, Math.PI / 7, 0, 1, 0);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },

  testMakeRotateZ() {
    const m0 = Mat4.createFloat32();
    const m1 = Mat4.createFloat32();

    Mat4.makeRotateZ(m0, Math.PI / 7);
    Mat4.makeRotate(m1, Math.PI / 7, 0, 0, 1);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },

  testTranslate() {
    const m0 = Mat4.createIdentity();
    Mat4.translate(m0, 3, 4, 5);
    assertElementsEquals([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 3, 4, 5, 1], m0);

    Mat4.setFromValues(m0, 1, 2, 3, 4, 2, 3, 4, 1, 3, 4, 1, 2, 4, 1, 2, 3);

    const m1 = Mat4.createFloat32();
    Mat4.makeTranslate(m1, 5, 6, 7);
    const m2 = Mat4.createFloat32();
    Mat4.multMat(m0, m1, m2);
    Mat4.translate(m0, 5, 6, 7);
    assertElementsEquals(m2, m0);
  },

  testScale() {
    const m0 = Mat4.createIdentity();
    Mat4.scale(m0, 3, 4, 5);
    assertElementsEquals([3, 0, 0, 0, 0, 4, 0, 0, 0, 0, 5, 0, 0, 0, 0, 1], m0);
  },

  testRotate() {
    const m0 = Mat4.createIdentity();
    Mat4.rotate(m0, Math.PI / 2, 0, 0, 1);
    assertElementsRoughlyEqual(
        [0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1], m0, vec.EPSILON);

    Mat4.rotate(m0, -Math.PI / 4, 0, 0, 1);
    assertElementsRoughlyEqual(
        [
          0.7071068, 0.7071068, 0, 0, -0.7071068, 0.7071068, 0, 0, 0, 0, 1, 0,
          0, 0, 0, 1
        ],
        m0, vec.EPSILON);
  },

  testRotateX() {
    const m0 = Mat4.createFloat32();
    const m1 = Mat4.createFloat32FromArray(randomMat4);

    Mat4.makeRotateX(m0, Math.PI / 7);
    Mat4.multMat(m1, m0, m0);
    Mat4.rotateX(m1, Math.PI / 7);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },

  testRotateY() {
    const m0 = Mat4.createFloat32();
    const m1 = Mat4.createFloat32FromArray(randomMat4);

    Mat4.makeRotateY(m0, Math.PI / 7);
    Mat4.multMat(m1, m0, m0);
    Mat4.rotateY(m1, Math.PI / 7);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },

  testRotateZ() {
    const m0 = Mat4.createFloat32();
    const m1 = Mat4.createFloat32FromArray(randomMat4);

    Mat4.makeRotateZ(m0, Math.PI / 7);
    Mat4.multMat(m1, m0, m0);
    Mat4.rotateZ(m1, Math.PI / 7);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },

  testGetTranslation() {
    const mat = Mat4.createFloat32FromArray(randomMat4);
    const translation = Vec3.createFloat32();
    Mat4.getTranslation(mat, translation);
    assertElementsRoughlyEqual(
        [0.59465038776, 0.413479506969, 0.0663217827677], translation,
        vec.EPSILON);
  },

  testMakeFrustum() {
    const m0 = Mat4.createFloat32();
    Mat4.makeFrustum(m0, -1, 2, -2, 1, .1, 1.1);
    assertElementsRoughlyEqual(
        [
          0.06666666, 0, 0, 0, 0, 0.06666666, 0, 0, 0.33333333, -0.33333333,
          -1.2, -1, 0, 0, -0.22, 0
        ],
        m0, vec.EPSILON);
  },

  testMakePerspective() {
    const m0 = Mat4.createFloat32();
    Mat4.makePerspective(m0, 90 * Math.PI / 180, 2, 0.1, 1.1);
    assertElementsRoughlyEqual(
        [0.5, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1.2, -1, 0, 0, -0.22, 0], m0,
        vec.EPSILON);
  },

  testMakeOrtho() {
    const m0 = Mat4.createFloat32();
    Mat4.makeOrtho(m0, -1, 2, -2, 1, 0.1, 1.1);
    assertElementsRoughlyEqual(
        [
          0.6666666, 0, 0, 0, 0, 0.6666666, 0, 0, 0, 0, -2, 0, -0.333333,
          0.3333333, -1.2, 1
        ],
        m0, vec.EPSILON);
  },

  testMakeEulerZXZ() {
    const m0 = Mat4.createFloat32();
    const roll = 0.200982 * 2 * Math.PI;
    const tilt = 0.915833 * Math.PI;
    const yaw = 0.839392 * 2 * Math.PI;

    Mat4.makeRotate(m0, roll, 0, 0, 1);
    Mat4.rotate(m0, tilt, 1, 0, 0);
    Mat4.rotate(m0, yaw, 0, 0, 1);

    const m1 = Mat4.createFloat32();
    Mat4.makeEulerZXZ(m1, roll, tilt, yaw);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);

    let euler = [0, 0, 0];
    Mat4.toEulerZXZ(m0, euler);

    assertRoughlyEquals(roll, euler[0], vec.EPSILON);
    assertRoughlyEquals(tilt, euler[1], vec.EPSILON);
    assertRoughlyEquals(yaw, euler[2], vec.EPSILON);

    // Test negative tilt now.
    Mat4.makeRotate(m0, roll, 0, 0, 1);
    Mat4.rotate(m0, -tilt, 1, 0, 0);
    Mat4.rotate(m0, yaw, 0, 0, 1);

    Mat4.makeEulerZXZ(m1, roll, -tilt, yaw);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);

    euler = [0, 0, 0];
    Mat4.toEulerZXZ(m0, euler, true);

    assertRoughlyEquals(roll, euler[0], vec.EPSILON);
    assertRoughlyEquals(-tilt, euler[1], vec.EPSILON);
    assertRoughlyEquals(yaw, euler[2], vec.EPSILON);
  },

  testEulerZXZExtrema() {
    const m0 = Mat4.createFloat32FromArray(
        [1, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 1]);
    const m1 = Mat4.createFloat32FromArray(
        [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);

    const euler = [0, 0, 0];
    Mat4.toEulerZXZ(m0, euler);
    assertElementsRoughlyEqual(
        [Math.PI, Math.PI / 2, Math.PI], euler, vec.EPSILON);
    Mat4.makeEulerZXZ(m1, euler[0], euler[1], euler[2]);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },

  testLookAt() {
    const viewMatrix = Mat4.createFloat32();
    Mat4.makeLookAt(viewMatrix, [0, 0, 0], [1, 0, 0], [0, 1, 0]);
    assertElementsRoughlyEqual(
        [0, 0, -1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1], viewMatrix,
        vec.EPSILON);
  },

  testToLookAt() {
    // This test does not use the default precision goog.vec.EPSILON due to
    // precision issues in some browsers leading to flaky tests.
    const EPSILON = 1e-4;

    const eyeExp = [0, 0, 0];
    const fwdExp = [1, 0, 0];
    const upExp = [0, 1, 0];

    const centerExp = [0, 0, 0];
    Vec3.add(eyeExp, fwdExp, centerExp);

    const view = Mat4.createFloat32();
    Mat4.makeLookAt(view, eyeExp, centerExp, upExp);

    const eyeRes = [0, 0, 0];
    const fwdRes = [0, 0, 0];
    const upRes = [0, 0, 0];
    Mat4.toLookAt(view, eyeRes, fwdRes, upRes);
    assertElementsRoughlyEqual(eyeExp, eyeRes, EPSILON);
    assertElementsRoughlyEqual(fwdExp, fwdRes, EPSILON);
    assertElementsRoughlyEqual(upExp, upRes, EPSILON);
  },

  testLookAtDecomposition() {
    // This test does not use the default precision goog.vec.EPSILON due to
    // precision issues in some browsers leading to flaky tests.
    const EPSILON = 1e-4;

    const viewExp = Mat4.createFloat32();
    const viewRes = Mat4.createFloat32();

    // Get a valid set of random vectors eye, forward, up by decomposing
    // a random matrix into a set of lookAt vectors.
    const tmp = Mat4.createFloat32FromArray(randomMat4);
    const eyeExp = [0, 0, 0];
    const fwdExp = [0, 0, 0];
    const upExp = [0, 0, 0];
    const centerExp = [0, 0, 0];
    // Project the random matrix into a real modelview matrix.
    Mat4.toLookAt(tmp, eyeExp, fwdExp, upExp);
    Vec3.add(eyeExp, fwdExp, centerExp);

    // Compute the expected modelview matrix from a set of valid random vectors.
    Mat4.makeLookAt(viewExp, eyeExp, centerExp, upExp);

    const eyeRes = [0, 0, 0];
    const fwdRes = [0, 0, 0];
    const upRes = [0, 0, 0];
    const centerRes = [0, 0, 0];
    Mat4.toLookAt(viewExp, eyeRes, fwdRes, upRes);
    Vec3.add(eyeRes, fwdRes, centerRes);

    Mat4.makeLookAt(viewRes, eyeRes, centerRes, upRes);

    assertElementsRoughlyEqual(eyeExp, eyeRes, EPSILON);
    assertElementsRoughlyEqual(fwdExp, fwdRes, EPSILON);
    assertElementsRoughlyEqual(upExp, upRes, EPSILON);
    assertElementsRoughlyEqual(viewExp, viewRes, EPSILON);
  },
});
