/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

////////////////////////// NOTE ABOUT EDITING THIS FILE ///////////////////////
//                                                                           //
// Any edits to this file must be applied to mat3d_test.js by running:       //
//   swap_type.sh mat3f_test.js > mat3d_test.js                              //
//                                                                           //
////////////////////////// NOTE ABOUT EDITING THIS FILE ///////////////////////

goog.module('goog.vec.mat3fTest');
goog.setTestOnly();

const mat3f = goog.require('goog.vec.mat3f');
const testSuite = goog.require('goog.testing.testSuite');
const vec = goog.require('goog.vec');

const randommat3f = mat3f.setFromValues(
    mat3f.create(), 0.8025078773498535, 0.7559120655059814, 0.15274643898010254,
    0.19196106493473053, 0.0890120416879654, 0.15422114729881287,
    0.09754583984613419, 0.44862601161003113, 0.9196512699127197);

testSuite({
  testCreate() {
    const m = mat3f.create();
    assertElementsEquals([0, 0, 0, 0, 0, 0, 0, 0, 0], m);
  },

  testCreateIdentity() {
    const m = mat3f.createIdentity();
    assertElementsEquals([1, 0, 0, 0, 1, 0, 0, 0, 1], m);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSet() {
    const m0 = mat3f.create();
    const m1 = mat3f.setFromArray(mat3f.create(), [1, 2, 3, 4, 5, 6, 7, 8, 9]);
    mat3f.setFromArray(m0, m1);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], m0);

    mat3f.setFromValues(m0, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    assertElementsEquals([2, 3, 4, 5, 6, 7, 8, 9, 10], m0);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSetDiagonal() {
    const m0 = mat3f.create();
    mat3f.setDiagonalValues(m0, 1, 2, 3);
    assertElementsEquals([1, 0, 0, 0, 2, 0, 0, 0, 3], m0);

    mat3f.setDiagonal(m0, [4, 5, 6]);
    assertElementsEquals([4, 0, 0, 0, 5, 0, 0, 0, 6], m0);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSetGetColumn() {
    const m0 = mat3f.create();
    mat3f.setColumn(m0, 0, [1, 2, 3]);
    mat3f.setColumn(m0, 1, [4, 5, 6]);
    mat3f.setColumn(m0, 2, [7, 8, 9]);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], m0);

    const v0 = [0, 0, 0];
    mat3f.getColumn(m0, 0, v0);
    assertElementsEquals([1, 2, 3], v0);
    mat3f.getColumn(m0, 1, v0);
    assertElementsEquals([4, 5, 6], v0);
    mat3f.getColumn(m0, 2, v0);
    assertElementsEquals([7, 8, 9], v0);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSetGetColumns() {
    const m0 = mat3f.create();
    mat3f.setColumns(m0, [1, 2, 3], [4, 5, 6], [7, 8, 9]);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], m0);

    const v0 = [0, 0, 0];
    const v1 = [0, 0, 0];
    const v2 = [0, 0, 0];

    mat3f.getColumns(m0, v0, v1, v2);
    assertElementsEquals([1, 2, 3], v0);
    assertElementsEquals([4, 5, 6], v1);
    assertElementsEquals([7, 8, 9], v2);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSetGetRow() {
    const m0 = mat3f.create();
    mat3f.setRow(m0, 0, [1, 2, 3]);
    mat3f.setRow(m0, 1, [4, 5, 6]);
    mat3f.setRow(m0, 2, [7, 8, 9]);
    assertElementsEquals([1, 4, 7, 2, 5, 8, 3, 6, 9], m0);

    const v0 = [0, 0, 0];
    mat3f.getRow(m0, 0, v0);
    assertElementsEquals([1, 2, 3], v0);
    mat3f.getRow(m0, 1, v0);
    assertElementsEquals([4, 5, 6], v0);
    mat3f.getRow(m0, 2, v0);
    assertElementsEquals([7, 8, 9], v0);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSetGetRows() {
    const m0 = mat3f.create();
    mat3f.setRows(m0, [1, 2, 3], [4, 5, 6], [7, 8, 9]);
    assertElementsEquals([1, 4, 7, 2, 5, 8, 3, 6, 9], m0);

    const v0 = [0, 0, 0];
    const v1 = [0, 0, 0];
    const v2 = [0, 0, 0];

    mat3f.getRows(m0, v0, v1, v2);
    assertElementsEquals([1, 2, 3], v0);
    assertElementsEquals([4, 5, 6], v1);
    assertElementsEquals([7, 8, 9], v2);
  },

  testMakeZero() {
    const m0 = mat3f.setFromArray(mat3f.create(), [1, 2, 3, 4, 5, 6, 7, 8, 9]);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], m0);
    mat3f.makeZero(m0);
    assertElementsEquals([0, 0, 0, 0, 0, 0, 0, 0, 0], m0);
  },

  testMakeIdentity() {
    const m0 = mat3f.create();
    mat3f.makeIdentity(m0);
    assertElementsEquals([1, 0, 0, 0, 1, 0, 0, 0, 1], m0);
  },

  testSetGetElement() {
    const m0 = mat3f.create();
    for (let r = 0; r < 3; r++) {
      for (let c = 0; c < 3; c++) {
        const value = c * 3 + r + 1;
        mat3f.setElement(m0, r, c, value);
        assertEquals(value, mat3f.getElement(m0, r, c));
      }
    }
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], m0);
  },

  testAddMat() {
    const m0 = mat3f.setFromValues(mat3f.create(), 1, 2, 3, 4, 5, 6, 7, 8, 9);
    const m1 = mat3f.setFromValues(mat3f.create(), 3, 4, 5, 6, 7, 8, 9, 1, 2);
    const m2 = mat3f.create();
    mat3f.addMat(m0, m1, m2);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], m0);
    assertElementsEquals([3, 4, 5, 6, 7, 8, 9, 1, 2], m1);
    assertElementsEquals([4, 6, 8, 10, 12, 14, 16, 9, 11], m2);

    mat3f.addMat(m0, m1, m0);
    assertElementsEquals([3, 4, 5, 6, 7, 8, 9, 1, 2], m1);
    assertElementsEquals([4, 6, 8, 10, 12, 14, 16, 9, 11], m0);
  },

  testSubMat() {
    const m0 = mat3f.setFromValues(mat3f.create(), 1, 2, 3, 4, 5, 6, 7, 8, 9);
    const m1 = mat3f.setFromValues(mat3f.create(), 3, 4, 5, 6, 7, 8, 9, 1, 2);
    const m2 = mat3f.create();

    mat3f.subMat(m0, m1, m2);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], m0);
    assertElementsEquals([3, 4, 5, 6, 7, 8, 9, 1, 2], m1);
    assertElementsEquals([-2, -2, -2, -2, -2, -2, -2, 7, 7], m2);

    mat3f.subMat(m1, m0, m1);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], m0);
    assertElementsEquals([2, 2, 2, 2, 2, 2, 2, -7, -7], m1);
  },

  testMultScalar() {
    const m0 = mat3f.setFromValues(mat3f.create(), 1, 2, 3, 4, 5, 6, 7, 8, 9);
    const m1 = mat3f.create();

    mat3f.multScalar(m0, 5, m1);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], m0);
    assertElementsEquals([5, 10, 15, 20, 25, 30, 35, 40, 45], m1);

    mat3f.multScalar(m0, 5, m0);
    assertElementsEquals([5, 10, 15, 20, 25, 30, 35, 40, 45], m0);
  },

  testMultMat() {
    const m0 = mat3f.setFromValues(mat3f.create(), 1, 2, 3, 4, 5, 6, 7, 8, 9);
    const m1 = mat3f.setFromValues(mat3f.create(), 1, 2, 3, 4, 5, 6, 7, 8, 9);
    const m2 = mat3f.create();

    mat3f.multMat(m0, m1, m2);
    assertElementsEquals([30, 36, 42, 66, 81, 96, 102, 126, 150], m2);

    mat3f.addMat(m0, m1, m1);
    mat3f.multMat(m0, m1, m1);
    assertElementsEquals([60, 72, 84, 132, 162, 192, 204, 252, 300], m1);
  },

  testTranspose() {
    const m0 = mat3f.setFromValues(mat3f.create(), 1, 2, 3, 4, 5, 6, 7, 8, 9);
    const m1 = mat3f.create();
    mat3f.transpose(m0, m1);
    assertElementsEquals([1, 4, 7, 2, 5, 8, 3, 6, 9], m1);
    mat3f.transpose(m1, m1);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], m1);
  },

  testInvert() {
    const m0 = mat3f.setFromValues(mat3f.create(), 1, 1, 1, 1, 1, 1, 1, 1, 1);
    assertFalse(mat3f.invert(m0, m0));
    assertElementsEquals([1, 1, 1, 1, 1, 1, 1, 1, 1], m0);

    mat3f.setFromValues(m0, 1, 2, 3, 1, 3, 4, 3, 4, 5);
    assertTrue(mat3f.invert(m0, m0));
    assertElementsEquals([0.5, -1.0, 0.5, -3.5, 2.0, 0.5, 2.5, -1.0, -0.5], m0);

    mat3f.makeScale(m0, .01, .01, .01);
    assertTrue(mat3f.invert(m0, m0));
    const m1 = mat3f.create();
    mat3f.makeScale(m1, 100, 100, 100);
    assertElementsEquals(m1, m0);
  },

  testEquals() {
    const m0 = mat3f.setFromValues(mat3f.create(), 1, 2, 3, 4, 5, 6, 7, 8, 9);
    /** @suppress {checkTypes} suppression added to enable type checking */
    const m1 = mat3f.setFromArray(mat3f.create(), m0);
    assertTrue(mat3f.equals(m0, m1));
    assertTrue(mat3f.equals(m1, m0));
    for (let i = 0; i < 9; i++) {
      m1[i] = 15;
      assertFalse(mat3f.equals(m0, m1));
      assertFalse(mat3f.equals(m1, m0));
      m1[i] = i + 1;
    }
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testMultVec3() {
    const m0 = mat3f.setFromValues(mat3f.create(), 1, 2, 3, 4, 5, 6, 7, 8, 9);
    const v0 = [1, 2, 3];
    const v1 = [0, 0, 0];

    mat3f.multVec3(m0, v0, v1);
    assertElementsEquals([30, 36, 42], v1);
    mat3f.multVec3(m0, v0, v0);
    assertElementsEquals([30, 36, 42], v0);
  },

  testSetValues() {
    const a0 = mat3f.create();
    assertElementsEquals([0, 0, 0, 0, 0, 0, 0, 0, 0], a0);
    mat3f.setFromValues(a0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], a0);

    const a1 = mat3f.create();
    mat3f.setDiagonalValues(a1, 1, 2, 3);
    assertElementsEquals([1, 0, 0, 0, 2, 0, 0, 0, 3], a1);

    mat3f.setColumnValues(a1, 0, 2, 3, 4);
    mat3f.setColumnValues(a1, 1, 5, 6, 7);
    mat3f.setColumnValues(a1, 2, 8, 9, 1);
    assertElementsEquals([2, 3, 4, 5, 6, 7, 8, 9, 1], a1);

    mat3f.setRowValues(a1, 0, 1, 4, 7);
    mat3f.setRowValues(a1, 1, 2, 5, 8);
    mat3f.setRowValues(a1, 2, 3, 6, 9);
    assertElementsEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], a1);
  },

  testMakeTranslate() {
    const m0 = mat3f.create();
    mat3f.makeTranslate(m0, 3, 4);
    assertElementsEquals([1, 0, 0, 0, 1, 0, 3, 4, 1], m0);
  },

  testMakeScale() {
    const m0 = mat3f.create();
    mat3f.makeScale(m0, 3, 4, 5);
    assertElementsEquals([3, 0, 0, 0, 4, 0, 0, 0, 5], m0);
  },

  testMakeRotate() {
    const m0 = mat3f.create();
    mat3f.makeRotate(m0, Math.PI / 2, 0, 0, 1);
    const v0 = [0, 1, 0, -1, 0, 0, 0, 0, 1];
    assertElementsRoughlyEqual(m0, v0, vec.EPSILON);

    const m1 = mat3f.create();
    mat3f.makeRotate(m1, -Math.PI / 4, 0, 0, 1);
    mat3f.multMat(m0, m1, m1);
    const v1 = [0.7071068, 0.7071068, 0, -0.7071068, 0.7071068, 0, 0, 0, 1];
    assertElementsRoughlyEqual(m1, v1, vec.EPSILON);
  },

  testMakeRotateX() {
    const m0 = mat3f.create();
    const m1 = mat3f.create();

    mat3f.makeRotateX(m0, Math.PI / 7);
    mat3f.makeRotate(m1, Math.PI / 7, 1, 0, 0);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },

  testMakeRotateY() {
    const m0 = mat3f.create();
    const m1 = mat3f.create();

    mat3f.makeRotateY(m0, Math.PI / 7);
    mat3f.makeRotate(m1, Math.PI / 7, 0, 1, 0);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },

  testMakeRotateZ() {
    const m0 = mat3f.create();
    const m1 = mat3f.create();

    mat3f.makeRotateZ(m0, Math.PI / 7);
    mat3f.makeRotate(m1, Math.PI / 7, 0, 0, 1);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },

  testRotate() {
    const m0 = mat3f.makeIdentity(mat3f.create());
    mat3f.rotate(m0, Math.PI / 2, 0, 0, 1);
    assertElementsRoughlyEqual([0, 1, 0, -1, 0, 0, 0, 0, 1], m0, vec.EPSILON);

    mat3f.rotate(m0, -Math.PI / 4, 0, 0, 1);
    assertElementsRoughlyEqual(
        [0.7071068, 0.7071068, 0, -0.7071068, 0.7071068, 0, 0, 0, 1], m0,
        vec.EPSILON);
  },

  testRotateX() {
    const m0 = mat3f.create();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const m1 = mat3f.setFromArray(mat3f.create(), randommat3f);

    mat3f.makeRotateX(m0, Math.PI / 7);
    mat3f.multMat(m1, m0, m0);
    mat3f.rotateX(m1, Math.PI / 7);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },

  testRotateY() {
    const m0 = mat3f.create();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const m1 = mat3f.setFromArray(mat3f.create(), randommat3f);

    mat3f.makeRotateY(m0, Math.PI / 7);
    mat3f.multMat(m1, m0, m0);
    mat3f.rotateY(m1, Math.PI / 7);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },

  testRotateZ() {
    const m0 = mat3f.create();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const m1 = mat3f.setFromArray(mat3f.create(), randommat3f);

    mat3f.makeRotateZ(m0, Math.PI / 7);
    mat3f.multMat(m1, m0, m0);
    mat3f.rotateZ(m1, Math.PI / 7);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testMakeEulerZXZ() {
    const m0 = mat3f.create();
    const roll = 0.200982 * 2 * Math.PI;
    const tilt = 0.915833 * Math.PI;
    const yaw = 0.839392 * 2 * Math.PI;

    mat3f.makeRotate(m0, roll, 0, 0, 1);
    mat3f.rotate(m0, tilt, 1, 0, 0);
    mat3f.rotate(m0, yaw, 0, 0, 1);

    const m1 = mat3f.create();
    mat3f.makeEulerZXZ(m1, roll, tilt, yaw);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);

    let euler = [0, 0, 0];
    mat3f.toEulerZXZ(m0, euler);

    assertRoughlyEquals(roll, euler[0], vec.EPSILON);
    assertRoughlyEquals(tilt, euler[1], vec.EPSILON);
    assertRoughlyEquals(yaw, euler[2], vec.EPSILON);

    // Test negative tilt now.
    mat3f.makeRotate(m0, roll, 0, 0, 1);
    mat3f.rotate(m0, -tilt, 1, 0, 0);
    mat3f.rotate(m0, yaw, 0, 0, 1);

    mat3f.makeEulerZXZ(m1, roll, -tilt, yaw);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);

    euler = [0, 0, 0];
    mat3f.toEulerZXZ(m0, euler, true);

    assertRoughlyEquals(roll, euler[0], vec.EPSILON);
    assertRoughlyEquals(-tilt, euler[1], vec.EPSILON);
    assertRoughlyEquals(yaw, euler[2], vec.EPSILON);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testEulerZXZExtrema() {
    const m0 = mat3f.setFromArray(mat3f.create(), [1, 0, 0, 0, 0, -1, 0, 1, 0]);
    const m1 = mat3f.setFromArray(mat3f.create(), [0, 0, 0, 0, 0, 0, 0, 0, 0]);

    const euler = [0, 0, 0];
    mat3f.toEulerZXZ(m0, euler);
    assertElementsRoughlyEqual(
        [Math.PI, Math.PI / 2, Math.PI], euler, vec.EPSILON);
    mat3f.makeEulerZXZ(m1, euler[0], euler[1], euler[2]);
    assertElementsRoughlyEqual(m0, m1, vec.EPSILON);
  },
});
