/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.math.Vec3Test');
goog.setTestOnly();

const Coordinate3 = goog.require('goog.math.Coordinate3');
const Vec3 = goog.require('goog.math.Vec3');
const testSuite = goog.require('goog.testing.testSuite');

function assertVec3Equals(a, b) {
  assertTrue(`${b} should be equal to ${a}`, Vec3.equals(a, b));
}

testSuite({
  testVec3() {
    const v = new Vec3(3.14, 2.78, -7.21);
    assertEquals(3.14, v.x);
    assertEquals(2.78, v.y);
    assertEquals(-7.21, v.z);
  },

  testRandomUnit() {
    const a = Vec3.randomUnit();
    assertRoughlyEquals(1.0, a.magnitude(), 1e-10);
  },

  testRandom() {
    const a = Vec3.random();
    assertTrue(a.magnitude() <= 1.0);
  },

  testFromCoordinate3() {
    const a = new Coordinate3(-2, 10, 4);
    const b = Vec3.fromCoordinate3(a);
    assertEquals(-2, b.x);
    assertEquals(10, b.y);
    assertEquals(4, b.z);
  },

  testClone() {
    const a = new Vec3(1, 2, 5);
    const b = a.clone();

    assertEquals(a.x, b.x);
    assertEquals(a.y, b.y);
    assertEquals(a.z, b.z);
  },

  testMagnitude() {
    const a = new Vec3(0, 10, 0);
    const b = new Vec3(3, 4, 5);
    const c = new Vec3(-4, 3, 8);

    assertEquals(10, a.magnitude());
    assertEquals(Math.sqrt(50), b.magnitude());
    assertEquals(Math.sqrt(89), c.magnitude());
  },

  testSquaredMagnitude() {
    const a = new Vec3(-3, -4, -5);
    assertEquals(50, a.squaredMagnitude());
  },

  testScale() {
    const a = new Vec3(1, 2, 3);
    a.scale(0.5);

    assertEquals(0.5, a.x);
    assertEquals(1, a.y);
    assertEquals(1.5, a.z);
  },

  testInvert() {
    const a = new Vec3(3, 4, 5);
    a.invert();

    assertEquals(-3, a.x);
    assertEquals(-4, a.y);
    assertEquals(-5, a.z);
  },

  testNormalize() {
    const a = new Vec3(5, 5, 5);
    a.normalize();
    assertRoughlyEquals(1.0, a.magnitude(), 1e-10);
  },

  testAdd() {
    const a = new Vec3(1, -1, 7);
    a.add(new Vec3(3, 3, 3));
    assertVec3Equals(new Vec3(4, 2, 10), a);
  },

  testSubtract() {
    const a = new Vec3(1, -1, 4);
    a.subtract(new Vec3(3, 3, 3));
    assertVec3Equals(new Vec3(-2, -4, 1), a);
  },

  testEquals() {
    const a = new Vec3(1, 2, 5);

    assertFalse(a.equals(null));
    assertFalse(a.equals(new Vec3(1, 3, 5)));
    assertFalse(a.equals(new Vec3(2, 2, 2)));

    assertTrue(a.equals(a));
    assertTrue(a.equals(new Vec3(1, 2, 5)));
  },

  testSum() {
    const a = new Vec3(0.5, 0.25, 1.2);
    const b = new Vec3(0.5, 0.75, -0.6);

    const c = Vec3.sum(a, b);
    assertVec3Equals(new Vec3(1, 1, 0.6), c);
  },

  testDifference() {
    const a = new Vec3(0.5, 0.25, 3);
    const b = new Vec3(0.5, 0.75, 5);

    const c = Vec3.difference(a, b);
    assertVec3Equals(new Vec3(0, -0.5, -2), c);
  },

  testDistance() {
    const a = new Vec3(3, 4, 5);
    const b = new Vec3(-3, -4, 5);

    assertEquals(10, Vec3.distance(a, b));
  },

  testSquaredDistance() {
    const a = new Vec3(3, 4, 5);
    const b = new Vec3(-3, -4, 5);

    assertEquals(100, Vec3.squaredDistance(a, b));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testVec3Equals() {
    assertTrue(Vec3.equals(null, null, null));
    assertFalse(Vec3.equals(null, new Vec3()));

    const a = new Vec3(1, 3, 5);
    assertTrue(Vec3.equals(a, a));
    assertTrue(Vec3.equals(a, new Vec3(1, 3, 5)));
    assertFalse(Vec3.equals(1, new Vec3(3, 1, 5)));
  },

  testDot() {
    const a = new Vec3(0, 5, 2);
    const b = new Vec3(3, 0, 5);
    assertEquals(10, Vec3.dot(a, b));

    const c = new Vec3(-5, -5, 5);
    const d = new Vec3(0, 7, -2);
    assertEquals(-45, Vec3.dot(c, d));
  },

  testCross() {
    const a = new Vec3(3, 0, 0);
    const b = new Vec3(0, 2, 0);
    assertVec3Equals(new Vec3(0, 0, 6), Vec3.cross(a, b));

    const c = new Vec3(1, 2, 3);
    const d = new Vec3(4, 5, 6);
    assertVec3Equals(new Vec3(-3, 6, -3), Vec3.cross(c, d));
  },

  testLerp() {
    const a = new Vec3(0, 0, 0);
    const b = new Vec3(10, 10, 10);

    for (let i = 0; i <= 10; i++) {
      const c = Vec3.lerp(a, b, i / 10);
      assertEquals(i, c.x);
      assertEquals(i, c.y);
      assertEquals(i, c.z);
    }
  },

  testRescaled() {
    const a = new Vec3(2, 3, 4);
    const b = Vec3.rescaled(a, -2);
    assertVec3Equals(new Vec3(-4, -6, -8), b);
    assertVec3Equals(new Vec3(2, 3, 4), a);
  },
});
