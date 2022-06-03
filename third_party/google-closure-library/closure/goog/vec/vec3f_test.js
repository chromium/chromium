/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

////////////////////////// NOTE ABOUT EDITING THIS FILE ///////////////////////
//                                                                           //
// Any edits to this file must be applied to vec3d_test.js by running:       //
//   swap_type.sh vec3f_test.js > vec3d_test.js                              //
//                                                                           //
////////////////////////// NOTE ABOUT EDITING THIS FILE ///////////////////////

goog.module('goog.vec.vec3fTest');
goog.setTestOnly();

const testSuite = goog.require('goog.testing.testSuite');
const vec = goog.require('goog.vec');
const vec3f = goog.require('goog.vec.vec3f');

testSuite({
  testCreate() {
    const v = vec3f.create();
    assertElementsEquals([0, 0, 0], v);
  },

  testCreateFromArray() {
    const v = vec3f.createFromArray([1, 2, 3]);
    assertElementsEquals([1, 2, 3], v);
  },

  testCreateFromValues() {
    const v = vec3f.createFromValues(1, 2, 3);
    assertElementsEquals([1, 2, 3], v);
  },

  testClone() {
    const v0 = vec3f.createFromValues(1, 2, 3);
    const v1 = vec3f.clone(v0);
    assertElementsEquals([1, 2, 3], v1);
  },

  testSet() {
    const v = vec3f.create();
    vec3f.setFromValues(v, 1, 2, 3);
    assertElementsEquals([1, 2, 3], v);

    vec3f.setFromArray(v, [4, 5, 6]);
    assertElementsEquals([4, 5, 6], v);

    const w = vec3f.create();
    vec3f.setFromValues(w, 1, 2, 3);
    assertElementsEquals([1, 2, 3], w);

    vec3f.setFromArray(w, [4, 5, 6]);
    assertElementsEquals([4, 5, 6], w);
  },

  testAdd() {
    const v0 = vec3f.setFromArray(vec3f.create(), [1, 2, 3]);
    const v1 = vec3f.setFromArray(vec3f.create(), [4, 5, 6]);
    const v2 = vec3f.setFromVec3f(vec3f.create(), v0);

    vec3f.add(v2, v1, v2);
    assertElementsEquals([1, 2, 3], v0);
    assertElementsEquals([4, 5, 6], v1);
    assertElementsEquals([5, 7, 9], v2);

    vec3f.add(vec3f.add(v0, v1, v2), v0, v2);
    assertElementsEquals([6, 9, 12], v2);
  },

  testSubtract() {
    const v0 = vec3f.setFromArray(vec3f.create(), [1, 2, 3]);
    const v1 = vec3f.setFromArray(vec3f.create(), [4, 5, 6]);
    let v2 = vec3f.setFromVec3f(vec3f.create(), v0);

    vec3f.subtract(v2, v1, v2);
    assertElementsEquals([1, 2, 3], v0);
    assertElementsEquals([4, 5, 6], v1);
    assertElementsEquals([-3, -3, -3], v2);

    vec3f.setFromValues(v2, 0, 0, 0);
    vec3f.subtract(v1, v0, v2);
    assertElementsEquals([3, 3, 3], v2);

    v2 = vec3f.setFromVec3f(vec3f.create(), v0);
    vec3f.subtract(v2, v1, v2);
    assertElementsEquals([-3, -3, -3], v2);

    vec3f.subtract(vec3f.subtract(v1, v0, v2), v0, v2);
    assertElementsEquals([2, 1, 0], v2);
  },

  testNegate() {
    const v0 = vec3f.setFromArray(vec3f.create(), [1, 2, 3]);
    const v1 = vec3f.create();

    vec3f.negate(v0, v1);
    assertElementsEquals([-1, -2, -3], v1);
    assertElementsEquals([1, 2, 3], v0);

    vec3f.negate(v0, v0);
    assertElementsEquals([-1, -2, -3], v0);
  },

  testAbs() {
    const v0 = vec3f.setFromArray(vec3f.create(), [-1, -2, -3]);
    const v1 = vec3f.create();

    vec3f.abs(v0, v1);
    assertElementsEquals([1, 2, 3], v1);
    assertElementsEquals([-1, -2, -3], v0);

    vec3f.abs(v0, v0);
    assertElementsEquals([1, 2, 3], v0);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testScale() {
    const v0 = vec3f.setFromArray(vec3f.create(), [1, 2, 3]);
    const v1 = vec3f.create();

    vec3f.scale(v0, 4, v1);
    assertElementsEquals([4, 8, 12], v1);
    assertElementsEquals([1, 2, 3], v0);

    vec3f.setFromArray(v1, v0);
    vec3f.scale(v1, 5, v1);
    assertElementsEquals([5, 10, 15], v1);
  },

  testMagnitudeSquared() {
    const v0 = vec3f.setFromArray(vec3f.create(), [1, 2, 3]);
    assertEquals(14, vec3f.magnitudeSquared(v0));
  },

  testMagnitude() {
    const v0 = vec3f.setFromArray(vec3f.create(), [1, 2, 3]);
    assertEquals(Math.sqrt(14), vec3f.magnitude(v0));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testNormalize() {
    const v0 = vec3f.setFromArray(vec3f.create(), [2, 3, 4]);
    const v1 = vec3f.create();
    const v2 = vec3f.create();
    vec3f.scale(v0, 1 / vec3f.magnitude(v0), v2);

    vec3f.normalize(v0, v1);
    assertElementsEquals(v2, v1);
    assertElementsEquals([2, 3, 4], v0);

    vec3f.setFromArray(v1, v0);
    vec3f.normalize(v1, v1);
    assertElementsEquals(v2, v1);
  },

  testDot() {
    const v0 = vec3f.setFromArray(vec3f.create(), [1, 2, 3]);
    const v1 = vec3f.setFromArray(vec3f.create(), [4, 5, 6]);
    assertEquals(32, vec3f.dot(v0, v1));
    assertEquals(32, vec3f.dot(v1, v0));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testCross() {
    const v0 = vec3f.setFromArray(vec3f.create(), [1, 2, 3]);
    const v1 = vec3f.setFromArray(vec3f.create(), [4, 5, 6]);
    const crossVec = vec3f.create();

    vec3f.cross(v0, v1, crossVec);
    assertElementsEquals([1, 2, 3], v0);
    assertElementsEquals([4, 5, 6], v1);
    assertElementsEquals([-3, 6, -3], crossVec);

    vec3f.setFromArray(crossVec, v1);
    vec3f.cross(crossVec, v0, crossVec);
    assertElementsEquals([1, 2, 3], v0);
    assertElementsEquals([4, 5, 6], v1);
    assertElementsEquals([3, -6, 3], crossVec);

    vec3f.cross(v0, v0, v0);
    assertElementsEquals([0, 0, 0], v0);
  },

  testDistanceSquared() {
    const v0 = vec3f.setFromValues(vec3f.create(), 1, 2, 3);
    const v1 = vec3f.setFromValues(vec3f.create(), 1, 2, 3);
    assertEquals(0, vec3f.distanceSquared(v0, v1));
    vec3f.setFromValues(v0, 1, 2, 3);
    vec3f.setFromValues(v1, -1, -2, -1);
    assertEquals(36, vec3f.distanceSquared(v0, v1));
  },

  testDistance() {
    const v0 = vec3f.setFromValues(vec3f.create(), 1, 2, 3);
    const v1 = vec3f.setFromValues(vec3f.create(), 1, 2, 3);
    assertEquals(0, vec3f.distance(v0, v1));
    vec3f.setFromValues(v0, 1, 2, 3);
    vec3f.setFromValues(v1, -1, -2, -1);
    assertEquals(6, vec3f.distance(v0, v1));
  },

  testDirection() {
    const v0 = vec3f.setFromValues(vec3f.create(), 1, 2, 3);
    const v1 = vec3f.setFromValues(vec3f.create(), 1, 2, 3);
    const dirVec = vec3f.setFromValues(vec3f.create(), 4, 5, 6);
    vec3f.direction(v0, v1, dirVec);
    assertElementsEquals([0, 0, 0], dirVec);
    vec3f.setFromValues(v0, 0, 0, 0);
    vec3f.setFromValues(v1, 1, 0, 0);
    vec3f.direction(v0, v1, dirVec);
    assertElementsEquals([1, 0, 0], dirVec);
    vec3f.setFromValues(v0, 1, 1, 1);
    vec3f.setFromValues(v1, 0, 0, 0);
    vec3f.direction(v0, v1, dirVec);
    assertElementsRoughlyEqual(
        [-0.5773502588272095, -0.5773502588272095, -0.5773502588272095], dirVec,
        vec.EPSILON);
  },

  testLerp() {
    const v0 = vec3f.setFromValues(vec3f.create(), 1, 2, 3);
    const v1 = vec3f.setFromValues(vec3f.create(), 10, 20, 30);
    const v2 = vec3f.setFromVec3f(vec3f.create(), v0);

    vec3f.lerp(v2, v1, 0, v2);
    assertElementsEquals([1, 2, 3], v2);
    vec3f.lerp(v2, v1, 1, v2);
    assertElementsEquals([10, 20, 30], v2);
    vec3f.lerp(v0, v1, .5, v2);
    assertElementsEquals([5.5, 11, 16.5], v2);
  },

  testSlerp() {
    const v0 = vec3f.setFromValues(vec3f.create(), 0, 0, 1);
    const v1 = vec3f.setFromValues(vec3f.create(), 1, 0, 0);
    const v2 = vec3f.setFromValues(vec3f.create(), -1, 0, 0);
    const v3 = vec3f.setFromValues(vec3f.create(), -5, 0, 0);
    const v4 = vec3f.setFromValues(vec3f.create(), 0, 0, -1);
    let v5 = vec3f.setFromVec3f(vec3f.create(), v0);

    // Try f == 0 and f == 1.
    vec3f.slerp(v5, v1, 0, v5);
    assertElementsEquals([0, 0, 1], v5);
    vec3f.slerp(v5, v1, 1, v5);
    assertElementsEquals([1, 0, 0], v5);

    // Try slerp between perpendicular vectors.
    vec3f.slerp(v0, v1, .5, v5);
    assertElementsRoughlyEqual(
        [Math.sqrt(2) / 2, 0, Math.sqrt(2) / 2], v5, vec.EPSILON);

    // Try slerp between vectors of opposite directions (+Z and -Z).
    v5 = vec3f.slerp(v0, v4, .5, v5);
    // Axis of rotation is arbitrary, but result should be 90 degrees from both
    // v0 and v4 when f = 0.5.
    assertRoughlyEquals(Math.PI / 2, Math.acos(vec3f.dot(v5, v0)), vec.EPSILON);
    assertRoughlyEquals(Math.PI / 2, Math.acos(vec3f.dot(v5, v4)), vec.EPSILON);

    // f == 0.25, result should be 45-degrees to v0, and 135 to v4.
    v5 = vec3f.slerp(v0, v4, .25, v5);
    assertRoughlyEquals(Math.PI / 4, Math.acos(vec3f.dot(v5, v0)), vec.EPSILON);
    assertRoughlyEquals(
        Math.PI * 3 / 4, Math.acos(vec3f.dot(v5, v4)), vec.EPSILON);

    // f = 0.75, result should be 135-degrees to v0, and 45 to v4.
    v5 = vec3f.slerp(v0, v4, .75, v5);
    assertRoughlyEquals(
        Math.PI * 3 / 4, Math.acos(vec3f.dot(v5, v0)), vec.EPSILON);
    assertRoughlyEquals(Math.PI / 4, Math.acos(vec3f.dot(v5, v4)), vec.EPSILON);

    // Same as above, but on opposite directions of the X-axis.
    v5 = vec3f.slerp(v1, v2, .5, v5);
    // Axis of rotation is arbitrary, but result should be 90 degrees from both
    // v1 and v2 when f = 0.5.
    assertRoughlyEquals(Math.PI / 2, Math.acos(vec3f.dot(v5, v1)), vec.EPSILON);
    assertRoughlyEquals(Math.PI / 2, Math.acos(vec3f.dot(v5, v2)), vec.EPSILON);

    // f == 0.25, result should be 45-degrees to v1, and 135 to v2.
    v5 = vec3f.slerp(v1, v2, .25, v5);
    assertRoughlyEquals(Math.PI / 4, Math.acos(vec3f.dot(v5, v1)), vec.EPSILON);
    assertRoughlyEquals(
        Math.PI * 3 / 4, Math.acos(vec3f.dot(v5, v2)), vec.EPSILON);

    // f = 0.75, result should be 135-degrees to v1, and 45 to v2.
    v5 = vec3f.slerp(v1, v2, .75, v5);
    assertRoughlyEquals(
        Math.PI * 3 / 4, Math.acos(vec3f.dot(v5, v1)), vec.EPSILON);
    assertRoughlyEquals(Math.PI / 4, Math.acos(vec3f.dot(v5, v2)), vec.EPSILON);

    // Try vectors that aren't perpendicular or opposite/same direction.
    const v6 = vec3f.setFromValues(
        vec3f.create(), Math.sqrt(2) / 2, Math.sqrt(2) / 2, 0);
    vec3f.slerp(v1, v6, .9, v5);

    // The vectors are 45 degrees apart, for f == 0.9, results should be 1/10 of
    // that from v6 and 9/10 of that away from v1.
    assertRoughlyEquals(
        (Math.PI / 4) * 0.9, Math.acos(vec3f.dot(v1, v5)), vec.EPSILON);
    assertRoughlyEquals(
        (Math.PI / 4) * 0.1, Math.acos(vec3f.dot(v6, v5)), vec.EPSILON);

    // Between vectors of the same direction, where one is non-unit-length
    // (magnitudes should be lerp-ed).
    vec3f.slerp(v2, v3, .5, v5);
    assertElementsEquals([-3, 0, 0], v5);

    // Between perpendicular vectors, where one is non-unit length.
    vec3f.slerp(v0, v3, .5, v5);
    assertRoughlyEquals(3, vec3f.magnitude(v5), vec.EPSILON);
    assertElementsRoughlyEqual(
        [-3 * (Math.sqrt(2) / 2), 0, 3 * (Math.sqrt(2) / 2)], v5, vec.EPSILON);

    // And vectors of opposite directions, where one is non-unit length.
    vec3f.slerp(v1, v3, .5, v5);
    // Axis of rotation is arbitrary, but result should be 90 degrees from both
    // v1 and v3.
    assertRoughlyEquals(
        Math.PI / 2,
        Math.acos(
            vec3f.dot(v5, v1) / (vec3f.magnitude(v5) * vec3f.magnitude(v1))),
        vec.EPSILON);
    assertRoughlyEquals(
        Math.PI / 2,
        Math.acos(
            vec3f.dot(v5, v3) / (vec3f.magnitude(v3) * vec3f.magnitude(v5))),
        vec.EPSILON);
    // Magnitude should be linearly interpolated.
    assertRoughlyEquals(3, vec3f.magnitude(v5), vec.EPSILON);

    // Try a case where the vectors are the same direction (the same vector in
    // this case), but where numerical error results in a dot product
    // slightly greater than 1. Taking the acos of this would result in NaN.
    const v7 = vec3f.setFromValues(vec3f.create(), 0.009, 0.147, 0.989);
    vec3f.slerp(v7, v7, .25, v5);
    assertElementsRoughlyEqual([v7[0], v7[1], v7[2]], v5, vec.EPSILON);
  },

  testMax() {
    const v0 = vec3f.setFromValues(vec3f.create(), 10, 20, 30);
    const v1 = vec3f.setFromValues(vec3f.create(), 5, 25, 35);
    const v2 = vec3f.create();

    vec3f.max(v0, v1, v2);
    assertElementsEquals([10, 25, 35], v2);
    vec3f.max(v1, v0, v1);
    assertElementsEquals([10, 25, 35], v1);
    vec3f.max(v2, 20, v2);
    assertElementsEquals([20, 25, 35], v2);
  },

  testMin() {
    const v0 = vec3f.setFromValues(vec3f.create(), 10, 20, 30);
    const v1 = vec3f.setFromValues(vec3f.create(), 5, 25, 35);
    const v2 = vec3f.create();

    vec3f.min(v0, v1, v2);
    assertElementsEquals([5, 20, 30], v2);
    vec3f.min(v1, v0, v1);
    assertElementsEquals([5, 20, 30], v1);
    vec3f.min(v2, 20, v2);
    assertElementsEquals([5, 20, 20], v2);
  },

  testEquals() {
    const v0 = vec3f.setFromValues(vec3f.create(), 1, 2, 3);
    let v1 = vec3f.setFromVec3f(vec3f.create(), v0);
    assertElementsEquals(v0, v1);

    v1[0] = 4;
    assertFalse(vec3f.equals(v0, v1));

    v1 = vec3f.setFromVec3f(vec3f.create(), v0);
    v1[1] = 4;
    assertFalse(vec3f.equals(v0, v1));

    v1 = vec3f.setFromVec3f(vec3f.create(), v0);
    v1[2] = 4;
    assertFalse(vec3f.equals(v0, v1));
  },
});
