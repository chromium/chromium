/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.vec.Vec3Test');
goog.setTestOnly();

const Vec3 = goog.require('goog.vec.Vec3');
const testSuite = goog.require('goog.testing.testSuite');
const vec = goog.require('goog.vec');

testSuite({
  /** @suppress {checkTypes} suppression added to enable type checking */
  testDeprecatedConstructor() {
    let v = Vec3.create();
    assertElementsEquals(0, v[0]);
    assertEquals(0, v[1]);
    assertEquals(0, v[2]);

    assertElementsEquals([0, 0, 0], Vec3.create());

    assertElementsEquals([1, 2, 3], Vec3.createFromValues(1, 2, 3));

    assertElementsEquals([1, 2, 3], Vec3.createFromArray([1, 2, 3]));

    v = Vec3.createFromValues(1, 2, 3);
    assertElementsEquals([1, 2, 3], Vec3.clone(v));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testConstructor() {
    const v = Vec3.createFloat32();
    assertElementsEquals(0, v[0]);
    assertEquals(0, v[1]);
    assertEquals(0, v[2]);

    assertElementsEquals([0, 0, 0], Vec3.createFloat32());

    Vec3.setFromValues(v, 1, 2, 3);
    assertElementsEquals([1, 2, 3], v);

    const w = Vec3.createFloat64();
    assertElementsEquals(0, w[0]);
    assertEquals(0, w[1]);
    assertEquals(0, w[2]);

    assertElementsEquals([0, 0, 0], Vec3.createFloat64());

    Vec3.setFromValues(w, 1, 2, 3);
    assertElementsEquals([1, 2, 3], w);
  },

  testSet() {
    const v = Vec3.createFloat32();
    Vec3.setFromValues(v, 1, 2, 3);
    assertElementsEquals([1, 2, 3], v);

    Vec3.setFromArray(v, [4, 5, 6]);
    assertElementsEquals([4, 5, 6], v);

    const w = Vec3.createFloat32();
    Vec3.setFromValues(w, 1, 2, 3);
    assertElementsEquals([1, 2, 3], w);

    Vec3.setFromArray(w, [4, 5, 6]);
    assertElementsEquals([4, 5, 6], w);
  },

  testAdd() {
    const v0 = Vec3.createFloat32FromArray([1, 2, 3]);
    const v1 = Vec3.createFloat32FromArray([4, 5, 6]);
    const v2 = Vec3.cloneFloat32(v0);

    Vec3.add(v2, v1, v2);
    assertElementsEquals([1, 2, 3], v0);
    assertElementsEquals([4, 5, 6], v1);
    assertElementsEquals([5, 7, 9], v2);

    Vec3.add(Vec3.add(v0, v1, v2), v0, v2);
    assertElementsEquals([6, 9, 12], v2);
  },

  testSubtract() {
    const v0 = Vec3.createFloat32FromArray([1, 2, 3]);
    const v1 = Vec3.createFloat32FromArray([4, 5, 6]);
    let v2 = Vec3.cloneFloat32(v0);

    Vec3.subtract(v2, v1, v2);
    assertElementsEquals([1, 2, 3], v0);
    assertElementsEquals([4, 5, 6], v1);
    assertElementsEquals([-3, -3, -3], v2);

    Vec3.setFromValues(v2, 0, 0, 0);
    Vec3.subtract(v1, v0, v2);
    assertElementsEquals([3, 3, 3], v2);

    v2 = Vec3.cloneFloat32(v0);
    Vec3.subtract(v2, v1, v2);
    assertElementsEquals([-3, -3, -3], v2);

    Vec3.subtract(Vec3.subtract(v1, v0, v2), v0, v2);
    assertElementsEquals([2, 1, 0], v2);
  },

  testNegate() {
    const v0 = Vec3.createFloat32FromArray([1, 2, 3]);
    const v1 = Vec3.createFloat32();

    Vec3.negate(v0, v1);
    assertElementsEquals([-1, -2, -3], v1);
    assertElementsEquals([1, 2, 3], v0);

    Vec3.negate(v0, v0);
    assertElementsEquals([-1, -2, -3], v0);
  },

  testAbs() {
    const v0 = Vec3.createFloat32FromValues(-1, -2, -3);
    const v1 = Vec3.createFloat32();

    Vec3.abs(v0, v1);
    assertElementsEquals([1, 2, 3], v1);
    assertElementsEquals([-1, -2, -3], v0);

    Vec3.abs(v0, v0);
    assertElementsEquals([1, 2, 3], v0);
  },

  testScale() {
    const v0 = Vec3.createFloat32FromArray([1, 2, 3]);
    const v1 = Vec3.createFloat32();

    Vec3.scale(v0, 4, v1);
    assertElementsEquals([4, 8, 12], v1);
    assertElementsEquals([1, 2, 3], v0);

    Vec3.setFromArray(v1, v0);
    Vec3.scale(v1, 5, v1);
    assertElementsEquals([5, 10, 15], v1);
  },

  testMagnitudeSquared() {
    const v0 = Vec3.createFloat32FromArray([1, 2, 3]);
    assertEquals(14, Vec3.magnitudeSquared(v0));
  },

  testMagnitude() {
    const v0 = Vec3.createFloat32FromArray([1, 2, 3]);
    assertEquals(Math.sqrt(14), Vec3.magnitude(v0));
  },

  testNormalize() {
    const v0 = Vec3.createFloat32FromArray([2, 3, 4]);
    const v1 = Vec3.create();
    const v2 = Vec3.create();
    Vec3.scale(v0, 1 / Vec3.magnitude(v0), v2);

    Vec3.normalize(v0, v1);
    assertElementsEquals(v2, v1);
    assertElementsEquals([2, 3, 4], v0);

    Vec3.setFromArray(v1, v0);
    Vec3.normalize(v1, v1);
    assertElementsEquals(v2, v1);
  },

  testDot() {
    const v0 = Vec3.createFloat32FromArray([1, 2, 3]);
    const v1 = Vec3.createFloat32FromArray([4, 5, 6]);
    assertEquals(32, Vec3.dot(v0, v1));
    assertEquals(32, Vec3.dot(v1, v0));
  },

  testCross() {
    const v0 = Vec3.createFloat32FromArray([1, 2, 3]);
    const v1 = Vec3.createFloat32FromArray([4, 5, 6]);
    const crossVec = Vec3.create();

    Vec3.cross(v0, v1, crossVec);
    assertElementsEquals([1, 2, 3], v0);
    assertElementsEquals([4, 5, 6], v1);
    assertElementsEquals([-3, 6, -3], crossVec);

    Vec3.setFromArray(crossVec, v1);
    Vec3.cross(crossVec, v0, crossVec);
    assertElementsEquals([1, 2, 3], v0);
    assertElementsEquals([4, 5, 6], v1);
    assertElementsEquals([3, -6, 3], crossVec);

    Vec3.cross(v0, v0, v0);
    assertElementsEquals([0, 0, 0], v0);
  },

  testDistanceSquared() {
    const v0 = Vec3.createFloat32FromValues(1, 2, 3);
    const v1 = Vec3.createFloat32FromValues(1, 2, 3);
    assertEquals(0, Vec3.distanceSquared(v0, v1));
    Vec3.setFromValues(v0, 1, 2, 3);
    Vec3.setFromValues(v1, -1, -2, -1);
    assertEquals(36, Vec3.distanceSquared(v0, v1));
  },

  testDistance() {
    const v0 = Vec3.createFloat32FromValues(1, 2, 3);
    const v1 = Vec3.createFloat32FromValues(1, 2, 3);
    assertEquals(0, Vec3.distance(v0, v1));
    Vec3.setFromValues(v0, 1, 2, 3);
    Vec3.setFromValues(v1, -1, -2, -1);
    assertEquals(6, Vec3.distance(v0, v1));
  },

  testDirection() {
    const v0 = Vec3.createFloat32FromValues(1, 2, 3);
    const v1 = Vec3.createFloat32FromValues(1, 2, 3);
    const dirVec = Vec3.createFloat32FromValues(4, 5, 6);
    Vec3.direction(v0, v1, dirVec);
    assertElementsEquals([0, 0, 0], dirVec);
    Vec3.setFromValues(v0, 0, 0, 0);
    Vec3.setFromValues(v1, 1, 0, 0);
    Vec3.direction(v0, v1, dirVec);
    assertElementsEquals([1, 0, 0], dirVec);
    Vec3.setFromValues(v0, 1, 1, 1);
    Vec3.setFromValues(v1, 0, 0, 0);
    Vec3.direction(v0, v1, dirVec);
    assertElementsRoughlyEqual(
        [-0.5773502588272095, -0.5773502588272095, -0.5773502588272095], dirVec,
        vec.EPSILON);
  },

  testLerp() {
    const v0 = Vec3.createFloat32FromValues(1, 2, 3);
    const v1 = Vec3.createFloat32FromValues(10, 20, 30);
    const v2 = Vec3.cloneFloat32(v0);

    Vec3.lerp(v2, v1, 0, v2);
    assertElementsEquals([1, 2, 3], v2);
    Vec3.lerp(v2, v1, 1, v2);
    assertElementsEquals([10, 20, 30], v2);
    Vec3.lerp(v0, v1, .5, v2);
    assertElementsEquals([5.5, 11, 16.5], v2);
  },

  testMax() {
    const v0 = Vec3.createFloat32FromValues(10, 20, 30);
    const v1 = Vec3.createFloat32FromValues(5, 25, 35);
    const v2 = Vec3.createFloat32();

    Vec3.max(v0, v1, v2);
    assertElementsEquals([10, 25, 35], v2);
    Vec3.max(v1, v0, v1);
    assertElementsEquals([10, 25, 35], v1);
    Vec3.max(v2, 20, v2);
    assertElementsEquals([20, 25, 35], v2);
  },

  testMin() {
    const v0 = Vec3.createFloat32FromValues(10, 20, 30);
    const v1 = Vec3.createFloat32FromValues(5, 25, 35);
    const v2 = Vec3.createFloat32();

    Vec3.min(v0, v1, v2);
    assertElementsEquals([5, 20, 30], v2);
    Vec3.min(v1, v0, v1);
    assertElementsEquals([5, 20, 30], v1);
    Vec3.min(v2, 20, v2);
    assertElementsEquals([5, 20, 20], v2);
  },

  testEquals() {
    const v0 = Vec3.createFloat32FromValues(1, 2, 3);
    let v1 = Vec3.cloneFloat32(v0);
    assertElementsEquals(v0, v1);

    v1[0] = 4;
    assertFalse(Vec3.equals(v0, v1));

    v1 = Vec3.cloneFloat32(v0);
    v1[1] = 4;
    assertFalse(Vec3.equals(v0, v1));

    v1 = Vec3.cloneFloat32(v0);
    v1[2] = 4;
    assertFalse(Vec3.equals(v0, v1));
  },
});
