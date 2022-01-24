/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.math.Coordinate3Test');
goog.setTestOnly();

const Coordinate3 = goog.require('goog.math.Coordinate3');
const testSuite = goog.require('goog.testing.testSuite');

function assertCoordinate3Equals(a, b) {
  assertTrue(`${b} should be equal to ${a}`, Coordinate3.equals(a, b));
}

testSuite({
  testCoordinate3MissingXYZ() {
    const noXYZ = new Coordinate3();
    assertEquals(0, noXYZ.x);
    assertEquals(0, noXYZ.y);
    assertEquals(0, noXYZ.z);
    assertCoordinate3Equals(noXYZ, new Coordinate3());
  },

  testCoordinate3MissingYZ() {
    const noYZ = new Coordinate3(10);
    assertEquals(10, noYZ.x);
    assertEquals(0, noYZ.y);
    assertEquals(0, noYZ.z);
    assertCoordinate3Equals(noYZ, new Coordinate3(10));
  },

  testCoordinate3MissingZ() {
    const noZ = new Coordinate3(10, 20);
    assertEquals(10, noZ.x);
    assertEquals(20, noZ.y);
    assertEquals(0, noZ.z);
    assertCoordinate3Equals(noZ, new Coordinate3(10, 20));
  },

  testCoordinate3IntegerValues() {
    const intCoord = new Coordinate3(10, 20, -19);
    assertEquals(10, intCoord.x);
    assertEquals(20, intCoord.y);
    assertEquals(-19, intCoord.z);
    assertCoordinate3Equals(intCoord, new Coordinate3(10, 20, -19));
  },

  testCoordinate3FloatValues() {
    const floatCoord = new Coordinate3(10.5, 20.897, -71.385);
    assertEquals(10.5, floatCoord.x);
    assertEquals(20.897, floatCoord.y);
    assertEquals(-71.385, floatCoord.z);
    assertCoordinate3Equals(floatCoord, new Coordinate3(10.5, 20.897, -71.385));
  },

  testCoordinate3OneNonNumericValue() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const dim5 = new Coordinate3('ten', 1000, 85);
    assertTrue(isNaN(dim5.x));
    assertEquals(1000, dim5.y);
    assertEquals(85, dim5.z);
  },

  testCoordinate3AllNonNumericValues() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const nonNumeric = new Coordinate3('ten', {woop: 'test'}, Math.sqrt(-1));
    assertTrue(isNaN(nonNumeric.x));
    assertTrue(isNaN(nonNumeric.y));
    assertTrue(isNaN(nonNumeric.z));
  },

  testCoordinate3Origin() {
    const origin = new Coordinate3(0, 0, 0);
    assertEquals(0, origin.x);
    assertEquals(0, origin.y);
    assertEquals(0, origin.z);
    assertCoordinate3Equals(origin, new Coordinate3(0, 0, 0));
  },

  testCoordinate3Clone() {
    const c = new Coordinate3();
    assertCoordinate3Equals(c, c.clone());
    c.x = -12;
    c.y = 13;
    c.z = 5;
    assertCoordinate3Equals(c, c.clone());
  },

  testToString() {
    assertEquals('(0, 0, 0)', new Coordinate3().toString());
    assertEquals('(1, 0, 0)', new Coordinate3(1).toString());
    assertEquals('(1, 2, 0)', new Coordinate3(1, 2).toString());
    assertEquals('(0, 0, 0)', new Coordinate3(0, 0, 0).toString());
    assertEquals('(1, 2, 3)', new Coordinate3(1, 2, 3).toString());
    assertEquals('(-4, 5, -3)', new Coordinate3(-4, 5, -3).toString());
    assertEquals(
        '(11.25, -71.935, 2.8)',
        new Coordinate3(11.25, -71.935, 2.8).toString());
  },

  testEquals() {
    const a = new Coordinate3(3, 4, 5);
    const b = new Coordinate3(3, 4, 5);
    const c = new Coordinate3(-3, 4, -5);

    assertTrue(Coordinate3.equals(null, null));
    assertFalse(Coordinate3.equals(a, null));
    assertTrue(Coordinate3.equals(a, a));
    assertTrue(Coordinate3.equals(a, b));
    assertFalse(Coordinate3.equals(a, c));
  },

  testCoordinate3Distance() {
    const a = new Coordinate3(-2, -3, 1);
    const b = new Coordinate3(2, 0, 1);
    assertEquals(5, Coordinate3.distance(a, b));
  },

  testCoordinate3SquaredDistance() {
    const a = new Coordinate3(7, 11, 1);
    const b = new Coordinate3(3, -1, 1);
    assertEquals(160, Coordinate3.squaredDistance(a, b));
  },

  testCoordinate3Difference() {
    const a = new Coordinate3(7, 11, 1);
    const b = new Coordinate3(3, -1, 1);
    assertCoordinate3Equals(
        Coordinate3.difference(a, b), new Coordinate3(4, 12, 0));
  },

  testToArray() {
    const a = new Coordinate3(7, 11, 1);
    const b = a.toArray();
    assertEquals(b.length, 3);
    assertEquals(b[0], 7);
    assertEquals(b[1], 11);
    assertEquals(b[2], 1);

    /** @suppress {checkTypes} suppression added to enable type checking */
    const c = new Coordinate3('abc', 'def', 'xyz');
    const result = c.toArray();
    assertTrue(isNaN(result[0]));
    assertTrue(isNaN(result[1]));
    assertTrue(isNaN(result[2]));
  },

  testFromArray() {
    const a = [1, 2, 3];
    const b = Coordinate3.fromArray(a);
    assertEquals('(1, 2, 3)', b.toString());

    const c = [1, 2];
    const d = Coordinate3.fromArray(c);
    assertEquals('(1, 2, 0)', d.toString());

    const e = [1];
    const f = Coordinate3.fromArray(e);
    assertEquals('(1, 0, 0)', f.toString());

    const g = [];
    const h = Coordinate3.fromArray(g);
    assertEquals('(0, 0, 0)', h.toString());

    const tooLong = [1, 2, 3, 4, 5, 6];
    assertThrows(
        'Error should be thrown attempting to convert an invalid type.',
        goog.partial(Coordinate3.fromArray, tooLong));
  },
});
