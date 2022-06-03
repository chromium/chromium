/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.math.Vec2Test');
goog.setTestOnly();

const Vec2 = goog.require('goog.math.Vec2');
const testSuite = goog.require('goog.testing.testSuite');

function assertVectorEquals(a, b) {
  assertTrue(`${b} should be equal to ${a}`, Vec2.equals(a, b));
}

testSuite({
  testVec2() {
    const v = new Vec2(3.14, 2.78);
    assertEquals(3.14, v.x);
    assertEquals(2.78, v.y);
  },

  testRandomUnit() {
    const a = Vec2.randomUnit();
    assertRoughlyEquals(1.0, a.magnitude(), 1e-10);
  },

  testRandom() {
    const a = Vec2.random();
    assertTrue(a.magnitude() <= 1.0);
  },

  testClone() {
    const a = new Vec2(1, 2);
    const b = a.clone();

    assertEquals(a.x, b.x);
    assertEquals(a.y, b.y);
  },

  testMagnitude() {
    const a = new Vec2(0, 10);
    const b = new Vec2(3, 4);
    const c = new Vec2(0, 0);

    assertEquals(10, a.magnitude());
    assertEquals(5, b.magnitude());
    assertEquals(0, c.magnitude());
  },

  testMagnitudeOverflow() {
    const x = Number.MAX_VALUE / 2;
    assertEquals(
        (Number.MAX_VALUE / 2) * Math.sqrt(2), new Vec2(x, x).magnitude());
  },

  testSquaredMagnitude() {
    const a = new Vec2(-3, -4);
    assertEquals(25, a.squaredMagnitude());
  },

  testScaleFactor() {
    const a = new Vec2(1, 2);
    const scaled = a.scale(0.5);

    assertTrue(
        'The type of the return value should be goog.math.Vec2',
        scaled instanceof Vec2);
    assertVectorEquals(new Vec2(0.5, 1), a);
  },

  testScaleXY() {
    const a = new Vec2(10, 15);
    const scaled = a.scale(2, 3);
    assertEquals('The function should return the target instance', a, scaled);
    assertTrue(
        'The type of the return value should be goog.math.Vec2',
        scaled instanceof Vec2);
    assertVectorEquals(new Vec2(20, 45), a);
  },

  testInvert() {
    const a = new Vec2(3, 4);
    a.invert();

    assertEquals(-3, a.x);
    assertEquals(-4, a.y);
  },

  testNormalize() {
    const a = new Vec2(5, 5);
    a.normalize();
    assertRoughlyEquals(1.0, a.magnitude(), 1e-10);
  },

  testAdd() {
    const a = new Vec2(1, -1);
    a.add(new Vec2(3, 3));
    assertVectorEquals(new Vec2(4, 2), a);
  },

  testSubtract() {
    const a = new Vec2(1, -1);
    a.subtract(new Vec2(3, 3));
    assertVectorEquals(new Vec2(-2, -4), a);
  },

  testRotate() {
    const a = new Vec2(1, -1);
    a.rotate(Math.PI / 2);
    assertRoughlyEquals(1, a.x, 0.000001);
    assertRoughlyEquals(1, a.y, 0.000001);
    a.rotate(-Math.PI);
    assertRoughlyEquals(-1, a.x, 0.000001);
    assertRoughlyEquals(-1, a.y, 0.000001);
  },

  testRotateAroundPoint() {
    const a =
        Vec2.rotateAroundPoint(new Vec2(1, -1), new Vec2(1, 0), Math.PI / 2);
    assertRoughlyEquals(2, a.x, 0.000001);
    assertRoughlyEquals(0, a.y, 0.000001);
  },

  testEquals() {
    const a = new Vec2(1, 2);

    assertFalse(a.equals(null));
    assertFalse(a.equals({}));
    assertFalse(a.equals(new Vec2(1, 3)));
    assertFalse(a.equals(new Vec2(2, 2)));

    assertTrue(a.equals(a));
    assertTrue(a.equals(new Vec2(1, 2)));
  },

  testSum() {
    const a = new Vec2(0.5, 0.25);
    const b = new Vec2(0.5, 0.75);

    const c = Vec2.sum(a, b);
    assertVectorEquals(new Vec2(1, 1), c);
  },

  testDifference() {
    const a = new Vec2(0.5, 0.25);
    const b = new Vec2(0.5, 0.75);

    const c = Vec2.difference(a, b);
    assertVectorEquals(new Vec2(0, -0.5), c);
  },

  testDistance() {
    const a = new Vec2(3, 4);
    const b = new Vec2(-3, -4);

    assertEquals(10, Vec2.distance(a, b));
  },

  testSquaredDistance() {
    const a = new Vec2(3, 4);
    const b = new Vec2(-3, -4);

    assertEquals(100, Vec2.squaredDistance(a, b));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testVec2Equals() {
    assertTrue(Vec2.equals(null, null));
    assertFalse(Vec2.equals(null, new Vec2()));

    const a = new Vec2(1, 3);
    assertTrue(Vec2.equals(a, a));
    assertTrue(Vec2.equals(a, new Vec2(1, 3)));
    assertFalse(Vec2.equals(1, new Vec2(3, 1)));
  },

  testDot() {
    const a = new Vec2(0, 5);
    const b = new Vec2(3, 0);
    assertEquals(0, Vec2.dot(a, b));

    const c = new Vec2(-5, -5);
    const d = new Vec2(0, 7);
    assertEquals(-35, Vec2.dot(c, d));
  },

  testDeterminant() {
    const a = new Vec2(0, 5);
    const b = new Vec2(0, 10);
    assertEquals(0, Vec2.determinant(a, b));

    const c = new Vec2(0, 5);
    const d = new Vec2(10, 0);
    assertEquals(-50, Vec2.determinant(c, d));

    const e = new Vec2(-5, -5);
    const f = new Vec2(0, 7);
    assertEquals(-35, Vec2.determinant(e, f));
  },

  testLerp() {
    const a = new Vec2(0, 0);
    const b = new Vec2(10, 10);

    for (let i = 0; i <= 10; i++) {
      const c = Vec2.lerp(a, b, i / 10);
      assertEquals(i, c.x);
      assertEquals(i, c.y);
    }
  },

  testRescaled() {
    const a = new Vec2(1, 2);
    const b = Vec2.rescaled(a, 3);
    assertVectorEquals(new Vec2(3, 6), b);

    const c = Vec2.rescaled(a, 3, -2);
    assertVectorEquals(new Vec2(3, -4), c);
    assertVectorEquals(new Vec2(1, 2), a);
  },

  testToString() {
    assertEquals('(0, 0)', new Vec2(0, 0).toString());
  },
});
