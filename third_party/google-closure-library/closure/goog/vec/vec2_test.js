/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.vec.Vec2Test');
goog.setTestOnly();

const Vec2 = goog.require('goog.vec.Vec2');
const testSuite = goog.require('goog.testing.testSuite');
const vec = goog.require('goog.vec');

testSuite({
  /** @suppress {checkTypes} suppression added to enable type checking */
  testConstructor() {
    const v = Vec2.createFloat32();
    assertElementsEquals(0, v[0]);
    assertEquals(0, v[1]);

    assertElementsEquals([0, 0], Vec2.createFloat32());

    Vec2.setFromValues(v, 1, 2);
    assertElementsEquals([1, 2], v);

    const w = Vec2.createFloat64();
    assertElementsEquals(0, w[0]);
    assertEquals(0, w[1]);

    assertElementsEquals([0, 0], Vec2.createFloat64());

    Vec2.setFromValues(w, 1, 2);
    assertElementsEquals([1, 2], w);
  },

  testSet() {
    const v = Vec2.createFloat32();
    Vec2.setFromValues(v, 1, 2);
    assertElementsEquals([1, 2], v);

    Vec2.setFromArray(v, [4, 5]);
    assertElementsEquals([4, 5], v);

    const w = Vec2.createFloat32();
    Vec2.setFromValues(w, 1, 2);
    assertElementsEquals([1, 2], w);

    Vec2.setFromArray(w, [4, 5]);
    assertElementsEquals([4, 5], w);
  },

  testAdd() {
    const v0 = Vec2.createFloat32FromArray([1, 2]);
    const v1 = Vec2.createFloat32FromArray([4, 5]);
    const v2 = Vec2.cloneFloat32(v0);

    Vec2.add(v2, v1, v2);
    assertElementsEquals([1, 2], v0);
    assertElementsEquals([4, 5], v1);
    assertElementsEquals([5, 7], v2);

    Vec2.add(Vec2.add(v0, v1, v2), v0, v2);
    assertElementsEquals([6, 9], v2);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSubtract() {
    const v0 = Vec2.createFloat32FromArray([1, 2]);
    const v1 = Vec2.createFloat32FromArray([4, 5]);
    let v2 = Vec2.cloneFloat32(v0);

    Vec2.subtract(v2, v1, v2);
    assertElementsEquals([1, 2], v0);
    assertElementsEquals([4, 5], v1);
    assertElementsEquals([-3, -3], v2);

    Vec2.setFromValues(v2, 0, 0, 0);
    Vec2.subtract(v1, v0, v2);
    assertElementsEquals([3, 3], v2);

    v2 = Vec2.cloneFloat32(v0);
    Vec2.subtract(v2, v1, v2);
    assertElementsEquals([-3, -3], v2);

    Vec2.subtract(Vec2.subtract(v1, v0, v2), v0, v2);
    assertElementsEquals([2, 1], v2);
  },

  testNegate() {
    const v0 = Vec2.createFloat32FromArray([1, 2]);
    const v1 = Vec2.createFloat32();

    Vec2.negate(v0, v1);
    assertElementsEquals([-1, -2], v1);
    assertElementsEquals([1, 2], v0);

    Vec2.negate(v0, v0);
    assertElementsEquals([-1, -2], v0);
  },

  testAbs() {
    const v0 = Vec2.createFloat32FromValues(-1, -2);
    const v1 = Vec2.createFloat32();

    Vec2.abs(v0, v1);
    assertElementsEquals([1, 2], v1);
    assertElementsEquals([-1, -2], v0);

    Vec2.abs(v0, v0);
    assertElementsEquals([1, 2], v0);
  },

  testScale() {
    const v0 = Vec2.createFloat32FromArray([1, 2]);
    const v1 = Vec2.createFloat32();

    Vec2.scale(v0, 4, v1);
    assertElementsEquals([4, 8], v1);
    assertElementsEquals([1, 2], v0);

    Vec2.setFromArray(v1, v0);
    Vec2.scale(v1, 5, v1);
    assertElementsEquals([5, 10], v1);
  },

  testMagnitudeSquared() {
    const v0 = Vec2.createFloat32FromArray([1, 2]);
    assertEquals(5, Vec2.magnitudeSquared(v0));
  },

  testMagnitude() {
    const v0 = Vec2.createFloat32FromArray([1, 2]);
    assertEquals(Math.sqrt(5), Vec2.magnitude(v0));
    const v1 = Vec2.createFloat32FromArray([0, 0]);
    assertEquals(0, Vec2.magnitude(v1));
  },

  testMagnitudeOverflow() {
    const x = Number.MAX_VALUE / 2;
    assertEquals(
        (Number.MAX_VALUE / 2) * Math.sqrt(2),
        Vec2.magnitude(Vec2.createFloat64FromArray([x, x])));
  },


  testNormalize() {
    const v0 = Vec2.createFloat32FromArray([2, 3]);
    const v1 = Vec2.createFloat32();
    const v2 = Vec2.createFloat32();
    Vec2.scale(v0, 1 / Vec2.magnitude(v0), v2);

    Vec2.normalize(v0, v1);
    assertElementsEquals(v2, v1);
    assertElementsEquals([2, 3], v0);

    Vec2.setFromArray(v1, v0);
    Vec2.normalize(v1, v1);
    assertElementsEquals(v2, v1);
  },

  testDot() {
    const v0 = Vec2.createFloat32FromArray([1, 2]);
    const v1 = Vec2.createFloat32FromArray([4, 5]);
    assertEquals(14, Vec2.dot(v0, v1));
    assertEquals(14, Vec2.dot(v1, v0));
  },

  testDistanceSquared() {
    const v0 = Vec2.createFloat32FromValues(1, 2);
    const v1 = Vec2.createFloat32FromValues(1, 2);
    assertEquals(0, Vec2.distanceSquared(v0, v1));
    Vec2.setFromValues(v0, 1, 2);
    Vec2.setFromValues(v1, -1, -2);
    assertEquals(20, Vec2.distanceSquared(v0, v1));
  },

  testDistance() {
    const v0 = Vec2.createFloat32FromValues(1, 2);
    const v1 = Vec2.createFloat32FromValues(1, 2);
    assertEquals(0, Vec2.distance(v0, v1));
    Vec2.setFromValues(v0, 2, 3);
    Vec2.setFromValues(v1, -2, 0);
    assertEquals(5, Vec2.distance(v0, v1));
  },

  testDirection() {
    const v0 = Vec2.createFloat32FromValues(1, 2);
    const v1 = Vec2.createFloat32FromValues(1, 2);
    const dirVec = Vec2.createFloat32FromValues(4, 5);
    Vec2.direction(v0, v1, dirVec);
    assertElementsEquals([0, 0], dirVec);
    Vec2.setFromValues(v0, 0, 0);
    Vec2.setFromValues(v1, 1, 0);
    Vec2.direction(v0, v1, dirVec);
    assertElementsEquals([1, 0], dirVec);
    Vec2.setFromValues(v0, 1, 1);
    Vec2.setFromValues(v1, 0, 0);
    Vec2.direction(v0, v1, dirVec);
    assertElementsRoughlyEqual(
        [-0.707106781, -0.707106781], dirVec, vec.EPSILON);
  },

  testLerp() {
    const v0 = Vec2.createFloat32FromValues(1, 2);
    const v1 = Vec2.createFloat32FromValues(10, 20);
    const v2 = Vec2.cloneFloat32(v0);

    Vec2.lerp(v2, v1, 0, v2);
    assertElementsEquals([1, 2], v2);
    Vec2.lerp(v2, v1, 1, v2);
    assertElementsEquals([10, 20], v2);
    Vec2.lerp(v0, v1, .5, v2);
    assertElementsEquals([5.5, 11], v2);
  },

  testMax() {
    const v0 = Vec2.createFloat32FromValues(10, 20);
    const v1 = Vec2.createFloat32FromValues(5, 25);
    const v2 = Vec2.createFloat32();

    Vec2.max(v0, v1, v2);
    assertElementsEquals([10, 25], v2);
    Vec2.max(v1, v0, v1);
    assertElementsEquals([10, 25], v1);
    Vec2.max(v2, 20, v2);
    assertElementsEquals([20, 25], v2);
  },

  testMin() {
    const v0 = Vec2.createFloat32FromValues(10, 20);
    const v1 = Vec2.createFloat32FromValues(5, 25);
    const v2 = Vec2.createFloat32();

    Vec2.min(v0, v1, v2);
    assertElementsEquals([5, 20], v2);
    Vec2.min(v1, v0, v1);
    assertElementsEquals([5, 20], v1);
    Vec2.min(v2, 10, v2);
    assertElementsEquals([5, 10], v2);
  },

  testEquals() {
    const v0 = Vec2.createFloat32FromValues(1, 2);
    let v1 = Vec2.cloneFloat32(v0);
    assertElementsEquals(v0, v1);

    v1[0] = 4;
    assertFalse(Vec2.equals(v0, v1));

    v1 = Vec2.cloneFloat32(v0);
    v1[1] = 4;
    assertFalse(Vec2.equals(v0, v1));
  },
});
