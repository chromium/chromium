/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.vec.Mat3Test');
goog.setTestOnly();

const Mat3 = goog.require('goog.vec.Mat3');
const testSuite = goog.require('goog.testing.testSuite');
const vec = goog.require('goog.vec');

const randomMat3 = Mat3.createFloat32FromValues(
    0.8025078773498535, 0.7559120655059814, 0.15274643898010254,
    0.19196106493473053, 0.0890120416879654, 0.15422114729881287,
    0.09754583984613419, 0.44862601161003113, 0.9196512699127197);

testSuite({
  testDeprecatedConstructor() {
    const m0 = Mat3.create();
    assertElementsEquals([0, 0, 0, 0, 0, 0, 0, 0, 0], m0);

    const m1 = Mat3.createFromArray([1, 2, 3, 4, 5, 6, 7, 8, 9]);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], m1);

    const m2 = Mat3.createFromArray(m1);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], m1);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], m2);

    const m3 = Mat3.createFromValues(1, 2, 3, 4, 5, 6, 7, 8, 9);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], m3);

    const m4 = Mat3.createIdentity();
    assertElementsEquals([1, 0, 0, 0, 1, 0, 0, 0, 1], m4);
  },

  testConstructor() {
    const m0 = Mat3.createFloat32();
    assertElementsEquals([0, 0, 0, 0, 0, 0, 0, 0, 0], m0);

    const m1 = Mat3.createFloat32FromArray([1, 2, 3, 4, 5, 6, 7, 8, 9]);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], m1);

    const m2 = Mat3.createFloat32FromArray(m1);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], m1);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], m2);

    const m3 = Mat3.createFloat32FromValues(1, 2, 3, 4, 5, 6, 7, 8, 9);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], m3);

    const m4 = Mat3.createFloat32Identity();
    assertElementsEquals([1, 0, 0, 0, 1, 0, 0, 0, 1], m4);

    const n0 = Mat3.createFloat64();
    assertElementsEquals([0, 0, 0, 0, 0, 0, 0, 0, 0], n0);

    const n1 = Mat3.createFloat64FromArray([1, 2, 3, 4, 5, 6, 7, 8, 9]);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], n1);

    const n2 = Mat3.createFloat64FromArray(n1);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], n1);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], n2);

    const n3 = Mat3.createFloat64FromValues(1, 2, 3, 4, 5, 6, 7, 8, 9);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], n3);

    const n4 = Mat3.createFloat64Identity();
    assertElementsEquals([1, 0, 0, 0, 1, 0, 0, 0, 1], n4);
  },

  testSet() {
    const m0 = Mat3.createFloat32();
    const m1 = Mat3.createFromArray([1, 2, 3, 4, 5, 6, 7, 8, 9]);
    Mat3.setFromArray(m0, m1);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], m0);

    Mat3.setFromValues(m0, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    assertElementsEquals([2, 3, 4, 5, 6, 7, 8, 9, 10], m0);
  },

  testSetDiagonal() {
    const m0 = Mat3.createFloat32();
    Mat3.setDiagonalValues(m0, 1, 2, 3);
    assertElementsEquals([1, 0, 0, 0, 2, 0, 0, 0, 3], m0);

    Mat3.setDiagonal(m0, [4, 5, 6]);
    assertElementsEquals([4, 0, 0, 0, 5, 0, 0, 0, 6], m0);
  },

  testSetGetColumn() {
    const m0 = Mat3.createFloat32();
    Mat3.setColumn(m0, 0, [1, 2, 3]);
    Mat3.setColumn(m0, 1, [4, 5, 6]);
    Mat3.setColumn(m0, 2, [7, 8, 9]);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], m0);

    const v0 = [0, 0, 0];
    Mat3.getColumn(m0, 0, v0);
    assertElementsEquals([1, 2, 3], v0);
    Mat3.getColumn(m0, 1, v0);
    assertElementsEquals([4, 5, 6], v0);
    Mat3.getColumn(m0, 2, v0);
    assertElementsEquals([7, 8, 9], v0);
  },

  testSetGetColumns() {
    const m0 = Mat3.createFloat32();
    Mat3.setColumns(m0, [1, 2, 3], [4, 5, 6], [7, 8, 9]);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], m0);

    const v0 = [0, 0, 0];
    const v1 = [0, 0, 0];
    const v2 = [0, 0, 0];

    Mat3.getColumns(m0, v0, v1, v2);
    assertElementsEquals([1, 2, 3], v0);
    assertElementsEquals([4, 5, 6], v1);
    assertElementsEquals([7, 8, 9], v2);
  },

  testSetGetRow() {
    const m0 = Mat3.createFloat32();
    Mat3.setRow(m0, 0, [1, 2, 3]);
    Mat3.setRow(m0, 1, [4, 5, 6]);
    Mat3.setRow(m0, 2, [7, 8, 9]);
    assertElementsEquals([1, 4, 7, 2, 5, 8, 3, 6, 9], m0);

    const v0 = [0, 0, 0];
    Mat3.getRow(m0, 0, v0);
    assertElementsEquals([1, 2, 3], v0);
    Mat3.getRow(m0, 1, v0);
    assertElementsEquals([4, 5, 6], v0);
    Mat3.getRow(m0, 2, v0);
    assertElementsEquals([7, 8, 9], v0);
  },

  testSetGetRows() {
    const m0 = Mat3.createFloat32();
    Mat3.setRows(m0, [1, 2, 3], [4, 5, 6], [7, 8, 9]);
    assertElementsEquals([1, 4, 7, 2, 5, 8, 3, 6, 9], m0);

    const v0 = [0, 0, 0];
    const v1 = [0, 0, 0];
    const v2 = [0, 0, 0];

    Mat3.getRows(m0, v0, v1, v2);
    assertElementsEquals([1, 2, 3], v0);
    assertElementsEquals([4, 5, 6], v1);
    assertElementsEquals([7, 8, 9], v2);
  },

  testSetRowMajorArray() {
    const m0 = Mat3.createFloat32();
    Mat3.setFromRowMajorArray(m0, [1, 2, 3, 4, 5, 6, 7, 8, 9]);
    assertElementsEquals([1, 4, 7, 2, 5, 8, 3, 6, 9], m0);
  },

  testMakeZero() {
    const m0 = Mat3.createFromArray([1, 2, 3, 4, 5, 6, 7, 8, 9]);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], m0);
    Mat3.makeZero(m0);
    assertElementsEquals([0, 0, 0, 0, 0, 0, 0, 0, 0], m0);
  },

  testMakeIdentity() {
    const m0 = Mat3.createFloat32();
    Mat3.makeIdentity(m0);
    assertElementsEquals([1, 0, 0, 0, 1, 0, 0, 0, 1], m0);
  },

  testSetGetElement() {
    const m0 = Mat3.createFloat32();
    for (let r = 0; r < 3; r++) {
      for (let c = 0; c < 3; c++) {
        const value = c * 3 + r + 1;
        Mat3.setElement(m0, r, c, value);
        assertEquals(value, Mat3.getElement(m0, r, c));
      }
    }
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], m0);
  },

  testAddMat() {
    const m0 = Mat3.createFloat32FromValues(1, 2, 3, 4, 5, 6, 7, 8, 9);
    const m1 = Mat3.createFloat32FromValues(3, 4, 5, 6, 7, 8, 9, 1, 2);
    const m2 = Mat3.create();
    Mat3.addMat(m0, m1, m2);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], m0);
    assertElementsEquals([3, 4, 5, 6, 7, 8, 9, 1, 2], m1);
    assertElementsEquals([4, 6, 8, 10, 12, 14, 16, 9, 11], m2);

    Mat3.addMat(m0, m1, m0);
    assertElementsEquals([3, 4, 5, 6, 7, 8, 9, 1, 2], m1);
    assertElementsEquals([4, 6, 8, 10, 12, 14, 16, 9, 11], m0);
  },

  testSubMat() {
    const m0 = Mat3.createFloat32FromValues(1, 2, 3, 4, 5, 6, 7, 8, 9);
    const m1 = Mat3.createFloat32FromValues(3, 4, 5, 6, 7, 8, 9, 1, 2);
    const m2 = Mat3.create();

    Mat3.subMat(m0, m1, m2);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], m0);
    assertElementsEquals([3, 4, 5, 6, 7, 8, 9, 1, 2], m1);
    assertElementsEquals([-2, -2, -2, -2, -2, -2, -2, 7, 7], m2);

    Mat3.subMat(m1, m0, m1);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], m0);
    assertElementsEquals([2, 2, 2, 2, 2, 2, 2, -7, -7], m1);
  },

  testMultScalar() {
    const m0 = Mat3.createFloat32FromValues(1, 2, 3, 4, 5, 6, 7, 8, 9);
    const m1 = Mat3.createFloat32();

    Mat3.multScalar(m0, 5, m1);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], m0);
    assertElementsEquals([5, 10, 15, 20, 25, 30, 35, 40, 45], m1);

    Mat3.multScalar(m0, 5, m0);
    assertElementsEquals([5, 10, 15, 20, 25, 30, 35, 40, 45], m0);
  },

  testMultMat() {
    const m0 = Mat3.createFloat32FromValues(1, 2, 3, 4, 5, 6, 7, 8, 9);
    const m1 = Mat3.createFloat32FromValues(1, 2, 3, 4, 5, 6, 7, 8, 9);
    const m2 = Mat3.create();

    Mat3.multMat(m0, m1, m2);
    assertElementsEquals([30, 36, 42, 66, 81, 96, 102, 126, 150], m2);

    Mat3.addMat(m0, m1, m1);
    Mat3.multMat(m0, m1, m1);
    assertElementsEquals([60, 72, 84, 132, 162, 192, 204, 252, 300], m1);
  },

  testTranspose() {
    const m0 = Mat3.createFloat32FromValues(1, 2, 3, 4, 5, 6, 7, 8, 9);
    const m1 = Mat3.createFloat32();
    Mat3.transpose(m0, m1);
    assertElementsEquals([1, 4, 7, 2, 5, 8, 3, 6, 9], m1);
    Mat3.transpose(m1, m1);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], m1);
  },

  testInvert() {
    const m0 = Mat3.createFloat32FromValues(1, 1, 1, 1, 1, 1, 1, 1, 1);
    assertFalse(Mat3.invert(m0, m0));
    assertElementsEquals([1, 1, 1, 1, 1, 1, 1, 1, 1], m0);

    Mat3.setFromValues(m0, 1, 2, 3, 1, 3, 4, 3, 4, 5);
    assertTrue(Mat3.invert(m0, m0));
    assertElementsEquals([0.5, -1.0, 0.5, -3.5, 2.0, 0.5, 2.5, -1.0, -0.5], m0);

    Mat3.makeScale(m0, .01, .01, .01);
    assertTrue(Mat3.invert(m0, m0));
    const m1 = Mat3.create();
    Mat3.makeScale(m1, 100, 100, 100);
    assertElementsEquals(m1, m0);
  },

  testEquals() {
    const m0 = Mat3.createFloat32FromValues(1, 2, 3, 4, 5, 6, 7, 8, 9);
    const m1 = Mat3.createFromArray(m0);
    assertTrue(Mat3.equals(m0, m1));
    assertTrue(Mat3.equals(m1, m0));
    for (let i = 0; i < 9; i++) {
      m1[i] = 15;
      assertFalse(Mat3.equals(m0, m1));
      assertFalse(Mat3.equals(m1, m0));
      m1[i] = i + 1;
    }
  },

  testMultVec3() {
    const m0 = Mat3.createFloat32FromValues(1, 2, 3, 4, 5, 6, 7, 8, 9);
    const v0 = [1, 2, 3];
    const v1 = [0, 0, 0];

    Mat3.multVec3(m0, v0, v1);
    assertElementsEquals([30, 36, 42], v1);
    Mat3.multVec3(m0, v0, v0);
    assertElementsEquals([30, 36, 42], v0);
  },

  testSetValues() {
    const a0 = Mat3.createFloat32();
    assertElementsEquals([0, 0, 0, 0, 0, 0, 0, 0, 0], a0);
    Mat3.setFromValues(a0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], a0);

    const a1 = Mat3.createFloat32();
    Mat3.setDiagonalValues(a1, 1, 2, 3);
    assertElementsEquals([1, 0, 0, 0, 2, 0, 0, 0, 3], a1);

    Mat3.setColumnValues(a1, 0, 2, 3, 4);
    Mat3.setColumnValues(a1, 1, 5, 6, 7);
    Mat3.setColumnValues(a1, 2, 8, 9, 1);
    assertElementsEquals([2, 3, 4, 5, 6, 7, 8, 9, 1], a1);

    Mat3.setRowValues(a1, 0, 1, 4, 7);
    Mat3.setRowValues(a1, 1, 2, 5, 8);
    Mat3.setRowValues(a1, 2, 3, 6, 9);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], a1);
  },

  testMakeTranslate() {
    const m0 = Mat3.createFloat32();
    Mat3.makeTranslate(m0, 3, 4);
    assertElementsEquals([1, 0, 0, 0, 1, 0, 3, 4, 1], m0);
  },

  testMakeScale() {
    const m0 = Mat3.createFloat32();
    Mat3.makeScale(m0, 3, 4, 5);
    assertElementsEquals([3, 0, 0, 0, 4, 0, 0, 0, 5], m0);
  },

  testMakeRotate() {
    const m0 = Mat3.createFloat32();
    Mat3.makeRotate(m0, Math.PI / 2, 0, 0, 1);
    const v0 = [0, 1, 0, -1, 0, 0, 0, 0, 1];
    assertElementsRoughlyEqual(m0, v0, vec.EPSILON);

    const m1 = Mat3.createFloat32();
    Mat3.makeRotate(m1, -Math.PI / 4, 0, 0, 1);
    Mat3.multMat(m0, m1, m1);
    const v1 = [0.7071068, 0.7071068, 0, -0.7071068, 0.7071068, 0, 0, 0, 1];
    assertElementsRoughlyEqual(m1, v1, vec.EPSILON);
  },

  testMakeRotateX() {
    const m0 = Mat3.createFloat32();
    const m1 = Mat3.createFloat32();

    Mat3.makeRotateX(m0, Math.PI / 7);
    Mat3.makeRotate(m1, Math.PI / 7, 1, 0, 0);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },

  testMakeRotateY() {
    const m0 = Mat3.createFloat32();
    const m1 = Mat3.createFloat32();

    Mat3.makeRotateY(m0, Math.PI / 7);
    Mat3.makeRotate(m1, Math.PI / 7, 0, 1, 0);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },

  testMakeRotateZ() {
    const m0 = Mat3.createFloat32();
    const m1 = Mat3.createFloat32();

    Mat3.makeRotateZ(m0, Math.PI / 7);
    Mat3.makeRotate(m1, Math.PI / 7, 0, 0, 1);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },

  testRotate() {
    const m0 = Mat3.createIdentity();
    Mat3.rotate(m0, Math.PI / 2, 0, 0, 1);
    assertElementsRoughlyEqual([0, 1, 0, -1, 0, 0, 0, 0, 1], m0, vec.EPSILON);

    Mat3.rotate(m0, -Math.PI / 4, 0, 0, 1);
    assertElementsRoughlyEqual(
        [0.7071068, 0.7071068, 0, -0.7071068, 0.7071068, 0, 0, 0, 1], m0,
        vec.EPSILON);
  },

  testRotateX() {
    const m0 = Mat3.createFloat32();
    const m1 = Mat3.createFloat32FromArray(randomMat3);

    Mat3.makeRotateX(m0, Math.PI / 7);
    Mat3.multMat(m1, m0, m0);
    Mat3.rotateX(m1, Math.PI / 7);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },

  testRotateY() {
    const m0 = Mat3.createFloat32();
    const m1 = Mat3.createFloat32FromArray(randomMat3);

    Mat3.makeRotateY(m0, Math.PI / 7);
    Mat3.multMat(m1, m0, m0);
    Mat3.rotateY(m1, Math.PI / 7);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },

  testRotateZ() {
    const m0 = Mat3.createFloat32();
    const m1 = Mat3.createFloat32FromArray(randomMat3);

    Mat3.makeRotateZ(m0, Math.PI / 7);
    Mat3.multMat(m1, m0, m0);
    Mat3.rotateZ(m1, Math.PI / 7);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },

  testMakeEulerZXZ() {
    const m0 = Mat3.createFloat32();
    const roll = 0.200982 * 2 * Math.PI;
    const tilt = 0.915833 * Math.PI;
    const yaw = 0.839392 * 2 * Math.PI;

    Mat3.makeRotate(m0, roll, 0, 0, 1);
    Mat3.rotate(m0, tilt, 1, 0, 0);
    Mat3.rotate(m0, yaw, 0, 0, 1);

    const m1 = Mat3.createFloat32();
    Mat3.makeEulerZXZ(m1, roll, tilt, yaw);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);

    let euler = [0, 0, 0];
    Mat3.toEulerZXZ(m0, euler);

    assertRoughlyEquals(roll, euler[0], vec.EPSILON);
    assertRoughlyEquals(tilt, euler[1], vec.EPSILON);
    assertRoughlyEquals(yaw, euler[2], vec.EPSILON);

    // Test negative tilt now.
    Mat3.makeRotate(m0, roll, 0, 0, 1);
    Mat3.rotate(m0, -tilt, 1, 0, 0);
    Mat3.rotate(m0, yaw, 0, 0, 1);

    Mat3.makeEulerZXZ(m1, roll, -tilt, yaw);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);

    euler = [0, 0, 0];
    Mat3.toEulerZXZ(m0, euler, true);

    assertRoughlyEquals(roll, euler[0], vec.EPSILON);
    assertRoughlyEquals(-tilt, euler[1], vec.EPSILON);
    assertRoughlyEquals(yaw, euler[2], vec.EPSILON);
  },

  testEulerZXZExtrema() {
    const m0 = Mat3.createFloat32FromArray([1, 0, 0, 0, 0, -1, 0, 1, 0]);
    const m1 = Mat3.createFloat32FromArray([0, 0, 0, 0, 0, 0, 0, 0, 0]);

    const euler = [0, 0, 0];
    Mat3.toEulerZXZ(m0, euler);
    assertElementsRoughlyEqual(
        [Math.PI, Math.PI / 2, Math.PI], euler, vec.EPSILON);
    Mat3.makeEulerZXZ(m1, euler[0], euler[1], euler[2]);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },
});
