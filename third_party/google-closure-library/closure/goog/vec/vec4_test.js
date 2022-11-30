/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.vec.Vec4Test');
goog.setTestOnly();

const Vec4 = goog.require('goog.vec.Vec4');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testDeprecatedConstructor() {
    assertElementsEquals([0, 0, 0, 0], Vec4.create());

    assertElementsEquals([1, 2, 3, 4], Vec4.createFromValues(1, 2, 3, 4));

    assertElementsEquals([1, 2, 3, 4], Vec4.createFromArray([1, 2, 3, 4]));

    const v = Vec4.createFromValues(1, 2, 3, 4);
    assertElementsEquals([1, 2, 3, 4], Vec4.clone(v));
  },

  testConstructor() {
    assertElementsEquals([0, 0, 0, 0], Vec4.createFloat32());

    assertElementsEquals(
        [1, 2, 3, 4], Vec4.createFloat32FromValues(1, 2, 3, 4));

    assertElementsEquals(
        [1, 2, 3, 4], Vec4.createFloat32FromArray([1, 2, 3, 4]));

    const v = Vec4.createFloat32FromValues(1, 2, 3, 4);
    assertElementsEquals([1, 2, 3, 4], Vec4.cloneFloat32(v));

    assertElementsEquals([0, 0, 0, 0], Vec4.createFloat64());

    assertElementsEquals(
        [1, 2, 3, 4], Vec4.createFloat64FromValues(1, 2, 3, 4));

    assertElementsEquals(
        [1, 2, 3, 4], Vec4.createFloat64FromArray([1, 2, 3, 4]));

    const w = Vec4.createFloat64FromValues(1, 2, 3, 4);
    assertElementsEquals([1, 2, 3, 4], Vec4.cloneFloat64(w));
  },

  testSet() {
    const v = Vec4.createFloat32();
    Vec4.setFromValues(v, 1, 2, 3, 4);
    assertElementsEquals([1, 2, 3, 4], v);

    Vec4.setFromArray(v, [4, 5, 6, 7]);
    assertElementsEquals([4, 5, 6, 7], v);
  },

  testAdd() {
    const v0 = Vec4.createFloat32FromArray([1, 2, 3, 4]);
    const v1 = Vec4.createFloat32FromArray([5, 6, 7, 8]);
    const v2 = Vec4.cloneFloat32(v0);

    Vec4.add(v2, v1, v2);
    assertElementsEquals([1, 2, 3, 4], v0);
    assertElementsEquals([5, 6, 7, 8], v1);
    assertElementsEquals([6, 8, 10, 12], v2);

    Vec4.add(Vec4.add(v0, v1, v2), v0, v2);
    assertElementsEquals([7, 10, 13, 16], v2);
  },

  testSubtract() {
    const v0 = Vec4.createFloat32FromArray([4, 3, 2, 1]);
    const v1 = Vec4.createFloat32FromArray([5, 6, 7, 8]);
    const v2 = Vec4.cloneFloat32(v0);

    Vec4.subtract(v2, v1, v2);
    assertElementsEquals([4, 3, 2, 1], v0);
    assertElementsEquals([5, 6, 7, 8], v1);
    assertElementsEquals([-1, -3, -5, -7], v2);

    Vec4.setFromValues(v2, 0, 0, 0, 0);
    Vec4.subtract(v1, v0, v2);
    assertElementsEquals([1, 3, 5, 7], v2);

    Vec4.subtract(Vec4.subtract(v1, v0, v2), v0, v2);
    assertElementsEquals([-3, 0, 3, 6], v2);
  },

  testNegate() {
    const v0 = Vec4.createFloat32FromArray([1, 2, 3, 4]);
    const v1 = Vec4.createFloat32();

    Vec4.negate(v0, v1);
    assertElementsEquals([-1, -2, -3, -4], v1);
    assertElementsEquals([1, 2, 3, 4], v0);

    Vec4.negate(v0, v0);
    assertElementsEquals([-1, -2, -3, -4], v0);
  },

  testAbs() {
    const v0 = Vec4.createFloat32FromValues(-1, -2, -3, -4);
    const v1 = Vec4.createFloat32();

    Vec4.abs(v0, v1);
    assertElementsEquals([1, 2, 3, 4], v1);
    assertElementsEquals([-1, -2, -3, -4], v0);

    Vec4.abs(v0, v0);
    assertElementsEquals([1, 2, 3, 4], v0);
  },

  testScale() {
    const v0 = Vec4.createFloat32FromArray([1, 2, 3, 4]);
    const v1 = Vec4.createFloat32();

    Vec4.scale(v0, 4, v1);
    assertElementsEquals([4, 8, 12, 16], v1);
    assertElementsEquals([1, 2, 3, 4], v0);

    Vec4.setFromArray(v1, v0);
    Vec4.scale(v1, 5, v1);
    assertElementsEquals([5, 10, 15, 20], v1);
  },

  testMagnitudeSquared() {
    const v0 = Vec4.createFloat32FromArray([1, 2, 3, 4]);
    assertEquals(30, Vec4.magnitudeSquared(v0));
  },

  testMagnitude() {
    const v0 = Vec4.createFloat32FromArray([1, 2, 3, 4]);
    assertEquals(Math.sqrt(30), Vec4.magnitude(v0));
  },

  testNormalize() {
    const v0 = Vec4.createFloat32FromArray([2, 3, 4, 5]);
    const v1 = Vec4.createFloat32();
    const v2 = Vec4.createFloat32();
    Vec4.scale(v0, 1 / Vec4.magnitude(v0), v2);

    Vec4.normalize(v0, v1);
    assertElementsEquals(v2, v1);
    assertElementsEquals([2, 3, 4, 5], v0);

    Vec4.setFromArray(v1, v0);
    Vec4.normalize(v1, v1);
    assertElementsEquals(v2, v1);
  },

  testDot() {
    const v0 = Vec4.createFloat32FromArray([1, 2, 3, 4]);
    const v1 = Vec4.createFloat32FromArray([5, 6, 7, 8]);
    assertEquals(70, Vec4.dot(v0, v1));
    assertEquals(70, Vec4.dot(v1, v0));
  },

  testLerp() {
    const v0 = Vec4.createFloat32FromValues(1, 2, 3, 4);
    const v1 = Vec4.createFloat32FromValues(10, 20, 30, 40);
    const v2 = Vec4.cloneFloat32(v0);

    Vec4.lerp(v2, v1, 0, v2);
    assertElementsEquals([1, 2, 3, 4], v2);
    Vec4.lerp(v2, v1, 1, v2);
    assertElementsEquals([10, 20, 30, 40], v2);
    Vec4.lerp(v0, v1, .5, v2);
    assertElementsEquals([5.5, 11, 16.5, 22], v2);
  },

  testMax() {
    const v0 = Vec4.createFloat32FromValues(10, 20, 30, 40);
    const v1 = Vec4.createFloat32FromValues(5, 25, 35, 30);
    const v2 = Vec4.createFloat32();

    Vec4.max(v0, v1, v2);
    assertElementsEquals([10, 25, 35, 40], v2);
    Vec4.max(v1, v0, v1);
    assertElementsEquals([10, 25, 35, 40], v1);
    Vec4.max(v2, 20, v2);
    assertElementsEquals([20, 25, 35, 40], v2);
  },

  testMin() {
    const v0 = Vec4.createFloat32FromValues(10, 20, 30, 40);
    const v1 = Vec4.createFloat32FromValues(5, 25, 35, 30);
    const v2 = Vec4.createFloat32();

    Vec4.min(v0, v1, v2);
    assertElementsEquals([5, 20, 30, 30], v2);
    Vec4.min(v1, v0, v1);
    assertElementsEquals([5, 20, 30, 30], v1);
    Vec4.min(v2, 20, v2);
    assertElementsEquals([5, 20, 20, 20], v2);
  },

  testEquals() {
    const v0 = Vec4.createFloat32FromValues(1, 2, 3, 4);
    let v1 = Vec4.cloneFloat32(v0);
    assertElementsEquals(v0, v1);

    v1[0] = 5;
    assertFalse(Vec4.equals(v0, v1));

    v1 = Vec4.cloneFloat32(v0);
    v1[1] = 5;
    assertFalse(Vec4.equals(v0, v1));

    v1 = Vec4.cloneFloat32(v0);
    v1[2] = 5;
    assertFalse(Vec4.equals(v0, v1));

    v1 = Vec4.cloneFloat32(v0);
    v1[3] = 5;
    assertFalse(Vec4.equals(v0, v1));
  },
});
