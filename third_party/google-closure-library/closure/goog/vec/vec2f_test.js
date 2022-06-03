/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

////////////////////////// NOTE ABOUT EDITING THIS FILE ///////////////////////
//                                                                           //
// Any edits to this file must be applied to vec2d_test.js by running:       //
//   swap_type.sh vec2f_test.js > vec2d_test.js                              //
//                                                                           //
////////////////////////// NOTE ABOUT EDITING THIS FILE ///////////////////////

goog.module('goog.vec.vec2fTest');
goog.setTestOnly();

const testSuite = goog.require('goog.testing.testSuite');
const vec = goog.require('goog.vec');
const vec2f = goog.require('goog.vec.vec2f');

testSuite({
  testCreate() {
    const v = vec2f.create();
    assertElementsEquals([0, 0], v);
  },

  testCreateFromArray() {
    const v = vec2f.createFromArray([1, 2]);
    assertElementsEquals([1, 2], v);
  },

  testCreateFromValues() {
    const v = vec2f.createFromValues(1, 2);
    assertElementsEquals([1, 2], v);
  },

  testClone() {
    const v0 = vec2f.createFromValues(1, 2);
    const v1 = vec2f.clone(v0);
    assertElementsEquals([1, 2], v1);
  },

  testSet() {
    const v = vec2f.create();
    vec2f.setFromValues(v, 1, 2);
    assertElementsEquals([1, 2], v);

    vec2f.setFromArray(v, [4, 5]);
    assertElementsEquals([4, 5], v);

    const w = vec2f.create();
    vec2f.setFromValues(w, 1, 2);
    assertElementsEquals([1, 2], w);

    vec2f.setFromArray(w, [4, 5]);
    assertElementsEquals([4, 5], w);
  },

  testAdd() {
    const v0 = vec2f.setFromArray(vec2f.create(), [1, 2]);
    const v1 = vec2f.setFromArray(vec2f.create(), [4, 5]);
    const v2 = vec2f.setFromVec2f(vec2f.create(), v0);

    vec2f.add(v2, v1, v2);
    assertElementsEquals([1, 2], v0);
    assertElementsEquals([4, 5], v1);
    assertElementsEquals([5, 7], v2);

    vec2f.add(vec2f.add(v0, v1, v2), v0, v2);
    assertElementsEquals([6, 9], v2);
  },

  testSubtract() {
    const v0 = vec2f.setFromArray(vec2f.create(), [1, 2]);
    const v1 = vec2f.setFromArray(vec2f.create(), [4, 5]);
    let v2 = vec2f.setFromVec2f(vec2f.create(), v0);

    vec2f.subtract(v2, v1, v2);
    assertElementsEquals([1, 2], v0);
    assertElementsEquals([4, 5], v1);
    assertElementsEquals([-3, -3], v2);

    vec2f.setFromValues(v2, 0, 0);
    vec2f.subtract(v1, v0, v2);
    assertElementsEquals([3, 3], v2);

    v2 = vec2f.setFromVec2f(vec2f.create(), v0);
    vec2f.subtract(v2, v1, v2);
    assertElementsEquals([-3, -3], v2);

    vec2f.subtract(vec2f.subtract(v1, v0, v2), v0, v2);
    assertElementsEquals([2, 1], v2);
  },

  testMultiply() {
    const v0 = vec2f.setFromArray(vec2f.create(), [1, 2]);
    const v1 = vec2f.setFromArray(vec2f.create(), [4, 5]);
    const v2 = vec2f.setFromVec2f(vec2f.create(), v0);

    vec2f.componentMultiply(v2, v1, v2);
    assertElementsEquals([1, 2], v0);
    assertElementsEquals([4, 5], v1);
    assertElementsEquals([4, 10], v2);

    vec2f.componentMultiply(vec2f.componentMultiply(v0, v1, v2), v0, v2);
    assertElementsEquals([4, 20], v2);
  },

  testDivide() {
    const v0 = vec2f.setFromArray(vec2f.create(), [1, 2]);
    const v1 = vec2f.setFromArray(vec2f.create(), [4, 5]);
    let v2 = vec2f.setFromVec2f(vec2f.create(), v0);

    vec2f.componentDivide(v2, v1, v2);
    assertElementsRoughlyEqual([1, 2], v0, 10e-5);
    assertElementsRoughlyEqual([4, 5], v1, 10e-5);
    assertElementsRoughlyEqual([.25, .4], v2, 10e-5);

    vec2f.setFromValues(v2, 0, 0);
    vec2f.componentDivide(v1, v0, v2);
    assertElementsRoughlyEqual([4, 2.5], v2, 10e-5);

    v2 = vec2f.setFromVec2f(vec2f.create(), v0);
    vec2f.componentDivide(v2, v1, v2);
    assertElementsRoughlyEqual([.25, .4], v2, 10e-5);

    vec2f.componentDivide(vec2f.componentDivide(v1, v0, v2), v0, v2);
    assertElementsRoughlyEqual([4, 1.25], v2, 10e-5);
  },

  testNegate() {
    const v0 = vec2f.setFromArray(vec2f.create(), [1, 2]);
    const v1 = vec2f.create();

    vec2f.negate(v0, v1);
    assertElementsEquals([-1, -2], v1);
    assertElementsEquals([1, 2], v0);

    vec2f.negate(v0, v0);
    assertElementsEquals([-1, -2], v0);
  },

  testAbs() {
    const v0 = vec2f.setFromArray(vec2f.create(), [-1, -2]);
    const v1 = vec2f.create();

    vec2f.abs(v0, v1);
    assertElementsEquals([1, 2], v1);
    assertElementsEquals([-1, -2], v0);

    vec2f.abs(v0, v0);
    assertElementsEquals([1, 2], v0);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testScale() {
    const v0 = vec2f.setFromArray(vec2f.create(), [1, 2]);
    const v1 = vec2f.create();

    vec2f.scale(v0, 4, v1);
    assertElementsEquals([4, 8], v1);
    assertElementsEquals([1, 2], v0);

    vec2f.setFromArray(v1, v0);
    vec2f.scale(v1, 5, v1);
    assertElementsEquals([5, 10], v1);
  },

  testMagnitudeSquared() {
    const v0 = vec2f.setFromArray(vec2f.create(), [1, 2]);
    assertEquals(5, vec2f.magnitudeSquared(v0));
  },

  testMagnitude() {
    const v0 = vec2f.setFromArray(vec2f.create(), [1, 2]);
    assertEquals(Math.sqrt(5), vec2f.magnitude(v0));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testNormalize() {
    const v0 = vec2f.setFromArray(vec2f.create(), [2, 3]);
    const v1 = vec2f.create();
    const v2 = vec2f.create();
    vec2f.scale(v0, 1 / vec2f.magnitude(v0), v2);

    vec2f.normalize(v0, v1);
    assertElementsEquals(v2, v1);
    assertElementsEquals([2, 3], v0);

    vec2f.setFromArray(v1, v0);
    vec2f.normalize(v1, v1);
    assertElementsEquals(v2, v1);
  },

  testDot() {
    const v0 = vec2f.setFromArray(vec2f.create(), [1, 2]);
    const v1 = vec2f.setFromArray(vec2f.create(), [4, 5]);
    assertEquals(14, vec2f.dot(v0, v1));
    assertEquals(14, vec2f.dot(v1, v0));
  },

  testDistanceSquared() {
    const v0 = vec2f.setFromValues(vec2f.create(), 1, 2);
    const v1 = vec2f.setFromValues(vec2f.create(), 1, 2);
    assertEquals(0, vec2f.distanceSquared(v0, v1));
    vec2f.setFromValues(v0, 1, 2);
    vec2f.setFromValues(v1, -1, -2);
    assertEquals(20, vec2f.distanceSquared(v0, v1));
  },

  testDistance() {
    const v0 = vec2f.setFromValues(vec2f.create(), 1, 2);
    const v1 = vec2f.setFromValues(vec2f.create(), 1, 2);
    assertEquals(0, vec2f.distance(v0, v1));
    vec2f.setFromValues(v0, 2, 3);
    vec2f.setFromValues(v1, -2, 0);
    assertEquals(5, vec2f.distance(v0, v1));
  },

  testDirection() {
    const v0 = vec2f.setFromValues(vec2f.create(), 1, 2);
    const v1 = vec2f.setFromValues(vec2f.create(), 1, 2);
    const dirVec = vec2f.setFromValues(vec2f.create(), 4, 5);
    vec2f.direction(v0, v1, dirVec);
    assertElementsEquals([0, 0], dirVec);
    vec2f.setFromValues(v0, 0, 0);
    vec2f.setFromValues(v1, 1, 0);
    vec2f.direction(v0, v1, dirVec);
    assertElementsEquals([1, 0], dirVec);
    vec2f.setFromValues(v0, 1, 1);
    vec2f.setFromValues(v1, 0, 0);
    vec2f.direction(v0, v1, dirVec);
    assertElementsRoughlyEqual(
        [-0.707106781, -0.707106781], dirVec, vec.EPSILON);
  },

  testLerp() {
    const v0 = vec2f.setFromValues(vec2f.create(), 1, 2);
    const v1 = vec2f.setFromValues(vec2f.create(), 10, 20);
    const v2 = vec2f.setFromVec2f(vec2f.create(), v0);

    vec2f.lerp(v2, v1, 0, v2);
    assertElementsEquals([1, 2], v2);
    vec2f.lerp(v2, v1, 1, v2);
    assertElementsEquals([10, 20], v2);
    vec2f.lerp(v0, v1, .5, v2);
    assertElementsEquals([5.5, 11], v2);
  },

  testMax() {
    const v0 = vec2f.setFromValues(vec2f.create(), 10, 20);
    const v1 = vec2f.setFromValues(vec2f.create(), 5, 25);
    const v2 = vec2f.create();

    vec2f.max(v0, v1, v2);
    assertElementsEquals([10, 25], v2);
    vec2f.max(v1, v0, v1);
    assertElementsEquals([10, 25], v1);
    vec2f.max(v2, 20, v2);
    assertElementsEquals([20, 25], v2);
  },

  testMin() {
    const v0 = vec2f.setFromValues(vec2f.create(), 10, 20);
    const v1 = vec2f.setFromValues(vec2f.create(), 5, 25);
    const v2 = vec2f.create();

    vec2f.min(v0, v1, v2);
    assertElementsEquals([5, 20], v2);
    vec2f.min(v1, v0, v1);
    assertElementsEquals([5, 20], v1);
    vec2f.min(v2, 10, v2);
    assertElementsEquals([5, 10], v2);
  },

  testEquals() {
    const v0 = vec2f.setFromValues(vec2f.create(), 1, 2);
    let v1 = vec2f.setFromVec2f(vec2f.create(), v0);
    assertElementsEquals(v0, v1);

    v1[0] = 4;
    assertFalse(vec2f.equals(v0, v1));

    v1 = vec2f.setFromVec2f(vec2f.create(), v0);
    v1[1] = 4;
    assertFalse(vec2f.equals(v0, v1));
  },
});
