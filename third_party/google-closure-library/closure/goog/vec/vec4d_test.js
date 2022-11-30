/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

////////////////////////// NOTE ABOUT EDITING THIS FILE ///////////////////////
//                                                                           //
// Any edits to this file must be applied to vec4f_test.js by running:       //
//   swap_type.sh vec4d_test.js > vec4f_test.js                              //
//                                                                           //
////////////////////////// NOTE ABOUT EDITING THIS FILE ///////////////////////

goog.module('goog.vec.vec4dTest');
goog.setTestOnly();

const testSuite = goog.require('goog.testing.testSuite');
const vec4d = goog.require('goog.vec.vec4d');

testSuite({
  testCreate() {
    const v = vec4d.create();
    assertElementsEquals([0, 0, 0, 0], v);
  },

  testCreateFromArray() {
    const v = vec4d.createFromArray([1, 2, 3, 4]);
    assertElementsEquals([1, 2, 3, 4], v);
  },

  testCreateFromValues() {
    const v = vec4d.createFromValues(1, 2, 3, 4);
    assertElementsEquals([1, 2, 3, 4], v);
  },

  testClone() {
    const v0 = vec4d.createFromValues(1, 2, 3, 4);
    const v1 = vec4d.clone(v0);
    assertElementsEquals([1, 2, 3, 4], v1);
  },

  testSet() {
    const v = vec4d.create();
    vec4d.setFromValues(v, 1, 2, 3, 4);
    assertElementsEquals([1, 2, 3, 4], v);

    vec4d.setFromArray(v, [4, 5, 6, 7]);
    assertElementsEquals([4, 5, 6, 7], v);
  },

  testAdd() {
    const v0 = vec4d.setFromArray(vec4d.create(), [1, 2, 3, 4]);
    const v1 = vec4d.setFromArray(vec4d.create(), [5, 6, 7, 8]);
    const v2 = vec4d.setFromVec4d(vec4d.create(), v0);

    vec4d.add(v2, v1, v2);
    assertElementsEquals([1, 2, 3, 4], v0);
    assertElementsEquals([5, 6, 7, 8], v1);
    assertElementsEquals([6, 8, 10, 12], v2);

    vec4d.add(vec4d.add(v0, v1, v2), v0, v2);
    assertElementsEquals([7, 10, 13, 16], v2);
  },

  testSubtract() {
    const v0 = vec4d.setFromArray(vec4d.create(), [4, 3, 2, 1]);
    const v1 = vec4d.setFromArray(vec4d.create(), [5, 6, 7, 8]);
    const v2 = vec4d.setFromVec4d(vec4d.create(), v0);

    vec4d.subtract(v2, v1, v2);
    assertElementsEquals([4, 3, 2, 1], v0);
    assertElementsEquals([5, 6, 7, 8], v1);
    assertElementsEquals([-1, -3, -5, -7], v2);

    vec4d.setFromValues(v2, 0, 0, 0, 0);
    vec4d.subtract(v1, v0, v2);
    assertElementsEquals([1, 3, 5, 7], v2);

    vec4d.subtract(vec4d.subtract(v1, v0, v2), v0, v2);
    assertElementsEquals([-3, 0, 3, 6], v2);
  },

  testNegate() {
    const v0 = vec4d.setFromArray(vec4d.create(), [1, 2, 3, 4]);
    const v1 = vec4d.create();

    vec4d.negate(v0, v1);
    assertElementsEquals([-1, -2, -3, -4], v1);
    assertElementsEquals([1, 2, 3, 4], v0);

    vec4d.negate(v0, v0);
    assertElementsEquals([-1, -2, -3, -4], v0);
  },

  testAbs() {
    const v0 = vec4d.setFromArray(vec4d.create(), [-1, -2, -3, -4]);
    const v1 = vec4d.create();

    vec4d.abs(v0, v1);
    assertElementsEquals([1, 2, 3, 4], v1);
    assertElementsEquals([-1, -2, -3, -4], v0);

    vec4d.abs(v0, v0);
    assertElementsEquals([1, 2, 3, 4], v0);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testScale() {
    const v0 = vec4d.setFromArray(vec4d.create(), [1, 2, 3, 4]);
    const v1 = vec4d.create();

    vec4d.scale(v0, 4, v1);
    assertElementsEquals([4, 8, 12, 16], v1);
    assertElementsEquals([1, 2, 3, 4], v0);

    vec4d.setFromArray(v1, v0);
    vec4d.scale(v1, 5, v1);
    assertElementsEquals([5, 10, 15, 20], v1);
  },

  testMagnitudeSquared() {
    const v0 = vec4d.setFromArray(vec4d.create(), [1, 2, 3, 4]);
    assertEquals(30, vec4d.magnitudeSquared(v0));
  },

  testMagnitude() {
    const v0 = vec4d.setFromArray(vec4d.create(), [1, 2, 3, 4]);
    assertEquals(Math.sqrt(30), vec4d.magnitude(v0));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testNormalize() {
    const v0 = vec4d.setFromArray(vec4d.create(), [2, 3, 4, 5]);
    const v1 = vec4d.create();
    const v2 = vec4d.create();
    vec4d.scale(v0, 1 / vec4d.magnitude(v0), v2);

    vec4d.normalize(v0, v1);
    assertElementsEquals(v2, v1);
    assertElementsEquals([2, 3, 4, 5], v0);

    vec4d.setFromArray(v1, v0);
    vec4d.normalize(v1, v1);
    assertElementsEquals(v2, v1);
  },

  testDot() {
    const v0 = vec4d.setFromArray(vec4d.create(), [1, 2, 3, 4]);
    const v1 = vec4d.setFromArray(vec4d.create(), [5, 6, 7, 8]);
    assertEquals(70, vec4d.dot(v0, v1));
    assertEquals(70, vec4d.dot(v1, v0));
  },

  testLerp() {
    const v0 = vec4d.setFromValues(vec4d.create(), 1, 2, 3, 4);
    const v1 = vec4d.setFromValues(vec4d.create(), 10, 20, 30, 40);
    const v2 = vec4d.setFromVec4d(vec4d.create(), v0);

    vec4d.lerp(v2, v1, 0, v2);
    assertElementsEquals([1, 2, 3, 4], v2);
    vec4d.lerp(v2, v1, 1, v2);
    assertElementsEquals([10, 20, 30, 40], v2);
    vec4d.lerp(v0, v1, .5, v2);
    assertElementsEquals([5.5, 11, 16.5, 22], v2);
  },

  testMax() {
    const v0 = vec4d.setFromValues(vec4d.create(), 10, 20, 30, 40);
    const v1 = vec4d.setFromValues(vec4d.create(), 5, 25, 35, 30);
    const v2 = vec4d.create();

    vec4d.max(v0, v1, v2);
    assertElementsEquals([10, 25, 35, 40], v2);
    vec4d.max(v1, v0, v1);
    assertElementsEquals([10, 25, 35, 40], v1);
    vec4d.max(v2, 20, v2);
    assertElementsEquals([20, 25, 35, 40], v2);
  },

  testMin() {
    const v0 = vec4d.setFromValues(vec4d.create(), 10, 20, 30, 40);
    const v1 = vec4d.setFromValues(vec4d.create(), 5, 25, 35, 30);
    const v2 = vec4d.create();

    vec4d.min(v0, v1, v2);
    assertElementsEquals([5, 20, 30, 30], v2);
    vec4d.min(v1, v0, v1);
    assertElementsEquals([5, 20, 30, 30], v1);
    vec4d.min(v2, 20, v2);
    assertElementsEquals([5, 20, 20, 20], v2);
  },

  testEquals() {
    const v0 = vec4d.setFromValues(vec4d.create(), 1, 2, 3, 4);
    let v1 = vec4d.setFromVec4d(vec4d.create(), v0);
    assertElementsEquals(v0, v1);

    v1[0] = 5;
    assertFalse(vec4d.equals(v0, v1));

    v1 = vec4d.setFromVec4d(vec4d.create(), v0);
    v1[1] = 5;
    assertFalse(vec4d.equals(v0, v1));

    v1 = vec4d.setFromVec4d(vec4d.create(), v0);
    v1[2] = 5;
    assertFalse(vec4d.equals(v0, v1));

    v1 = vec4d.setFromVec4d(vec4d.create(), v0);
    v1[3] = 5;
    assertFalse(vec4d.equals(v0, v1));
  },
});
