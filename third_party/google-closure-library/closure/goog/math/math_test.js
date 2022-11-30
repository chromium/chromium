/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.mathTest');
goog.setTestOnly();

const googMath = goog.require('goog.math');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testRandomInt() {
    assertEquals(0, googMath.randomInt(0));
    assertEquals(0, googMath.randomInt(1));

    const r = googMath.randomInt(3);
    assertTrue(0 <= r && r < 3);
  },

  testUniformRandom() {
    assertEquals(5.2, googMath.uniformRandom(5.2, 5.2));
    assertEquals(-6, googMath.uniformRandom(-6, -6));

    const r = googMath.uniformRandom(-0.5, 0.5);
    assertTrue(-0.5 <= r && r < 0.5);
  },

  testClamp() {
    assertEquals(3, googMath.clamp(3, -5, 5));
    assertEquals(5, googMath.clamp(5, -5, 5));
    assertEquals(-5, googMath.clamp(-5, -5, 5));

    assertEquals(-5, googMath.clamp(-22, -5, 5));
    assertEquals(5, googMath.clamp(6, -5, 5));
  },

  testModulo() {
    assertEquals(0, googMath.modulo(256, 8));

    assertEquals(7, googMath.modulo(7, 8));
    assertEquals(7, googMath.modulo(23, 8));
    assertEquals(7, googMath.modulo(-1, 8));

    // Safari 5.1.7 has a bug in its JS engine where modulo is computed
    // incorrectly when using variables. We avoid using
    // goog.testing.ExpectedFailure here since it pulls in a bunch of
    // extra dependencies for maintaining a DOM console.
    const a = 1;
    const b = -5;
    if (a % b === 1 % -5) {
      assertEquals(-4, googMath.modulo(1, -5));
      assertEquals(-4, googMath.modulo(6, -5));
    }
    assertEquals(-4, googMath.modulo(-4, -5));
  },

  testLerp() {
    assertEquals(0, googMath.lerp(0, 0, 0));
    assertEquals(3, googMath.lerp(0, 6, 0.5));
    assertEquals(3, googMath.lerp(-1, 1, 2));
  },

  testNearlyEquals() {
    assertTrue(
        'Numbers inside default tolerance should be equal',
        googMath.nearlyEquals(0.000001, 0.000001001));
    assertFalse(
        'Numbers outside default tolerance should be unequal',
        googMath.nearlyEquals(0.000001, 0.000003));
    assertTrue(
        'Numbers inside custom tolerance should be equal',
        googMath.nearlyEquals(0.001, 0.002, 0.1));
    assertFalse(
        'Numbers outside custom tolerance should be unequal',
        googMath.nearlyEquals(0.001, -0.1, 0.1));
    assertTrue(
        'Integer tolerance greater than one should succeed',
        googMath.nearlyEquals(87, 85, 3));
  },

  testStandardAngleInRadians() {
    assertRoughlyEquals(0, googMath.standardAngleInRadians(2 * Math.PI), 1e-10);
    assertRoughlyEquals(
        Math.PI, googMath.standardAngleInRadians(Math.PI), 1e-10);
    assertRoughlyEquals(
        Math.PI, googMath.standardAngleInRadians(-1 * Math.PI), 1e-10);
    assertRoughlyEquals(
        Math.PI / 2, googMath.standardAngleInRadians(-1.5 * Math.PI), 1e-10);
    assertRoughlyEquals(
        Math.PI, googMath.standardAngleInRadians(5 * Math.PI), 1e-10);
    assertEquals(0.01, googMath.standardAngleInRadians(0.01));
    assertEquals(0, googMath.standardAngleInRadians(0));
  },

  testStandardAngle() {
    assertEquals(359.5, googMath.standardAngle(-360.5));
    assertEquals(0, googMath.standardAngle(-360));
    assertEquals(359.5, googMath.standardAngle(-0.5));
    assertEquals(0, googMath.standardAngle(0));
    assertEquals(0.5, googMath.standardAngle(0.5));
    assertEquals(0, googMath.standardAngle(360));
    assertEquals(1, googMath.standardAngle(721));
  },

  testToRadians() {
    assertEquals(-Math.PI, googMath.toRadians(-180));
    assertEquals(0, googMath.toRadians(0));
    assertEquals(Math.PI, googMath.toRadians(180));
  },

  testToDegrees() {
    assertEquals(-180, googMath.toDegrees(-Math.PI));
    assertEquals(0, googMath.toDegrees(0));
    assertEquals(180, googMath.toDegrees(Math.PI));
  },

  testAngleDx() {
    assertRoughlyEquals(0, googMath.angleDx(0, 0), 1e-10);
    assertRoughlyEquals(0, googMath.angleDx(90, 0), 1e-10);
    assertRoughlyEquals(100, googMath.angleDx(0, 100), 1e-10);
    assertRoughlyEquals(0, googMath.angleDx(90, 100), 1e-10);
    assertRoughlyEquals(-100, googMath.angleDx(180, 100), 1e-10);
    assertRoughlyEquals(0, googMath.angleDx(270, 100), 1e-10);
  },

  testAngleDy() {
    assertRoughlyEquals(0, googMath.angleDy(0, 0), 1e-10);
    assertRoughlyEquals(0, googMath.angleDy(90, 0), 1e-10);
    assertRoughlyEquals(0, googMath.angleDy(0, 100), 1e-10);
    assertRoughlyEquals(100, googMath.angleDy(90, 100), 1e-10);
    assertRoughlyEquals(0, googMath.angleDy(180, 100), 1e-10);
    assertRoughlyEquals(-100, googMath.angleDy(270, 100), 1e-10);
  },

  testAngle() {
    assertRoughlyEquals(0, googMath.angle(10, 10, 20, 10), 1e-10);
    assertRoughlyEquals(90, googMath.angle(10, 10, 10, 20), 1e-10);
    assertRoughlyEquals(225, googMath.angle(10, 10, 0, 0), 1e-10);
    assertRoughlyEquals(270, googMath.angle(10, 10, 10, 0), 1e-10);

    // 0 is the conventional result, but mathematically this is undefined.
    assertEquals(0, googMath.angle(10, 10, 10, 10));
  },

  testAngleDifference() {
    assertEquals(10, googMath.angleDifference(30, 40));
    assertEquals(-10, googMath.angleDifference(40, 30));
    assertEquals(180, googMath.angleDifference(10, 190));
    assertEquals(180, googMath.angleDifference(190, 10));
    assertEquals(20, googMath.angleDifference(350, 10));
    assertEquals(-20, googMath.angleDifference(10, 350));
    assertEquals(100, googMath.angleDifference(350, 90));
    assertEquals(-80, googMath.angleDifference(350, 270));
    assertEquals(0, googMath.angleDifference(15, 15));
  },

  testSign() {
    assertEquals(0, googMath.sign(0));
    assertEquals(-1, googMath.sign(-3));
    assertEquals(1, googMath.sign(3));
    assertEquals(1, googMath.sign(0.0001));
    assertEquals(-1, googMath.sign(-0.0001));
    assertEquals(1, googMath.sign(3141592653589793));
  },

  testSignOfSpecialFloatValues() {
    assertEquals(-1, googMath.sign(-Infinity));
    assertEquals(1, googMath.sign(Infinity));
    assertNaN(googMath.sign(NaN));
    assertEquals(0, googMath.sign(0));
    assertFalse(googMath.isNegativeZero(googMath.sign(0)));
    assertEquals(0, googMath.sign(-0));
    assertTrue(googMath.isNegativeZero(googMath.sign(-0)));
  },

  testLongestCommonSubsequence() {
    const func = googMath.longestCommonSubsequence;

    assertArrayEquals([2], func([1, 2], [2, 1]));
    assertArrayEquals([1, 2], func([1, 2, 5], [2, 1, 2]));
    assertArrayEquals(
        [1, 2, 3, 4, 5],
        func([1, 0, 2, 3, 8, 4, 9, 5], [8, 1, 2, 4, 3, 6, 4, 5]));
    assertArrayEquals([1, 1, 1, 1, 1], func([1, 1, 1, 1, 1], [1, 1, 1, 1, 1]));
    assertArrayEquals([5], func([1, 2, 3, 4, 5], [5, 4, 3, 2, 1]));
    assertArrayEquals(
        [1, 8, 11], func([1, 6, 8, 11, 13], [1, 3, 5, 8, 9, 11, 12]));
  },

  testLongestCommonSubsequenceWithCustomComparator() {
    const func = googMath.longestCommonSubsequence;

    const compareFn = (a, b) => a.field == b.field;

    const a1 = {field: 'a1', field2: 'hello'};
    const a2 = {field: 'a2', field2: 33};
    const a3 = {field: 'a3'};
    const a4 = {field: 'a3'};

    assertArrayEquals([a1, a2], func([a1, a2, a3], [a3, a1, a2], compareFn));
    assertArrayEquals([a1, a3], func([a1, a3], [a1, a4], compareFn));
    // testing the same arrays without compare function
    assertArrayEquals([a1], func([a1, a3], [a1, a4]));
  },

  testLongestCommonSubsequenceWithCustomCollector() {
    const func = googMath.longestCommonSubsequence;

    const collectorFn = (a, b) => b;

    assertArrayEquals(
        [1, 2, 4, 6, 7],
        func(
            [1, 0, 2, 3, 8, 4, 9, 5], [8, 1, 2, 4, 3, 6, 4, 5], null,
            collectorFn));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSum() {
    assertEquals(
        'sum() must return 0 if there are no arguments', 0, googMath.sum());
    assertEquals(
        'sum() must return its argument if there is only one', 17,
        googMath.sum(17));
    assertEquals(
        'sum() must handle positive integers', 10, googMath.sum(1, 2, 3, 4));
    assertEquals(
        'sum() must handle real numbers', -2.5, googMath.sum(1, -2, 3, -4.5));
    assertTrue(
        'sum() must return NaN if one of the arguments isn\'t numeric',
        isNaN(googMath.sum(1, 2, 'foo', 3)));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testAverage() {
    assertTrue(
        'average() must return NaN if there are no arguments',
        isNaN(googMath.average()));
    assertEquals(
        'average() must return its argument if there is only one', 17,
        googMath.average(17));
    assertEquals(
        'average() must handle positive integers', 3,
        googMath.average(1, 2, 3, 4, 5));
    assertEquals(
        'average() must handle real numbers', -0.625,
        googMath.average(1, -2, 3, -4.5));
    assertTrue(
        'average() must return NaN if one of the arguments isn\'t ' +
            'numeric',
        isNaN(googMath.average(1, 2, 'foo', 3)));
  },

  testSampleVariance() {
    assertEquals(
        'sampleVariance() must return 0 if there are no samples', 0,
        googMath.sampleVariance());
    assertEquals(
        'sampleVariance() must return 0 if there is only one ' +
            'sample',
        0, googMath.sampleVariance(17));
    assertRoughlyEquals(
        'sampleVariance() must handle positive integers', 48,
        googMath.sampleVariance(3, 7, 7, 19), 0.0001);
    assertRoughlyEquals(
        'sampleVariance() must handle real numbers', 12.0138,
        googMath.sampleVariance(1.23, -2.34, 3.14, -4.56), 0.0001);
  },

  testStandardDeviation() {
    assertEquals(
        'standardDeviation() must return 0 if there are no samples', 0,
        googMath.standardDeviation());
    assertEquals(
        'standardDeviation() must return 0 if there is only one ' +
            'sample',
        0, googMath.standardDeviation(17));
    assertRoughlyEquals(
        'standardDeviation() must handle positive integers', 6.9282,
        googMath.standardDeviation(3, 7, 7, 19), 0.0001);
    assertRoughlyEquals(
        'standardDeviation() must handle real numbers', 3.4660,
        googMath.standardDeviation(1.23, -2.34, 3.14, -4.56), 0.0001);
  },

  testIsInt() {
    assertFalse(googMath.isInt(12345.67));
    assertFalse(googMath.isInt(0.123));
    assertFalse(googMath.isInt(.1));
    assertFalse(googMath.isInt(-23.43));
    assertFalse(googMath.isInt(-.1));
    assertFalse(googMath.isInt(1e-1));
    assertTrue(googMath.isInt(1));
    assertTrue(googMath.isInt(0));
    assertTrue(googMath.isInt(-2));
    assertTrue(googMath.isInt(-2.0));
    assertTrue(googMath.isInt(10324231));
    assertTrue(googMath.isInt(1.));
    assertTrue(googMath.isInt(1e3));
  },

  testIsFiniteNumber() {
    assertFalse(googMath.isFiniteNumber(NaN));
    assertFalse(googMath.isFiniteNumber(-Infinity));
    assertFalse(googMath.isFiniteNumber(+Infinity));
    assertTrue(googMath.isFiniteNumber(0));
    assertTrue(googMath.isFiniteNumber(1));
    assertTrue(googMath.isFiniteNumber(Math.PI));
  },

  testIsNegativeZero() {
    assertFalse(googMath.isNegativeZero(0));
    assertTrue(googMath.isNegativeZero(-0));
    assertFalse(googMath.isNegativeZero(1));
    assertFalse(googMath.isNegativeZero(-1));
    assertFalse(googMath.isNegativeZero(-Number.MIN_VALUE));
  },

  testLog10Floor() {
    // The greatest floating point number that is less than 1.
    const oneMinusEpsilon = 1 - Math.pow(2, -53);
    for (let i = -30; i <= 30; i++) {
      assertEquals(i, googMath.log10Floor(parseFloat(`1e${i}`)));
      assertEquals(
          i - 1, googMath.log10Floor(parseFloat(`1e${i}`) * oneMinusEpsilon));
    }
    assertEquals(-Infinity, googMath.log10Floor(0));
    assertTrue(isNaN(googMath.log10Floor(-1)));
  },

  testSafeFloor() {
    assertEquals(0, googMath.safeFloor(0));
    assertEquals(0, googMath.safeFloor(1e-15));
    assertEquals(0, googMath.safeFloor(-1e-15));
    assertEquals(-1, googMath.safeFloor(-3e-15));
    assertEquals(4, googMath.safeFloor(5 - 3e-15));
    assertEquals(5, googMath.safeFloor(5 - 1e-15));
    assertEquals(-5, googMath.safeFloor(-5 - 1e-15));
    assertEquals(-6, googMath.safeFloor(-5 - 3e-15));
    assertEquals(3, googMath.safeFloor(2.91, 0.1));
    assertEquals(2, googMath.safeFloor(2.89, 0.1));
    // Tests some real life examples with the default epsilon value.
    assertEquals(0, googMath.safeFloor(Math.log(1000) / Math.LN10 - 3));
    assertEquals(21, googMath.safeFloor(Math.log(1e+21) / Math.LN10));
  },

  testSafeCeil() {
    assertEquals(0, googMath.safeCeil(0));
    assertEquals(0, googMath.safeCeil(1e-15));
    assertEquals(0, googMath.safeCeil(-1e-15));
    assertEquals(1, googMath.safeCeil(3e-15));
    assertEquals(6, googMath.safeCeil(5 + 3e-15));
    assertEquals(5, googMath.safeCeil(5 + 1e-15));
    assertEquals(-4, googMath.safeCeil(-5 + 3e-15));
    assertEquals(-5, googMath.safeCeil(-5 + 1e-15));
    assertEquals(3, googMath.safeCeil(3.09, 0.1));
    assertEquals(4, googMath.safeCeil(3.11, 0.1));
  },
});
