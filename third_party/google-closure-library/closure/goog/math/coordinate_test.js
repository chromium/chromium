/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.math.CoordinateTest');
goog.setTestOnly();

const Coordinate = goog.require('goog.math.Coordinate');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testCoordinate1() {
    const dim1 = new Coordinate();
    assertEquals(0, dim1.x);
    assertEquals(0, dim1.y);
    assertEquals('(0, 0)', dim1.toString());
  },

  testCoordinate2() {
    const dim2 = new Coordinate(10);
    assertEquals(10, dim2.x);
    assertEquals(0, dim2.y);
    assertEquals('(10, 0)', dim2.toString());
  },

  testCoordinate3() {
    const dim3 = new Coordinate(10, 20);
    assertEquals(10, dim3.x);
    assertEquals(20, dim3.y);
    assertEquals('(10, 20)', dim3.toString());
  },

  testCoordinate4() {
    const dim4 = new Coordinate(10.5, 20.897);
    assertEquals(10.5, dim4.x);
    assertEquals(20.897, dim4.y);
    assertEquals('(10.5, 20.897)', dim4.toString());
  },

  testCoordinate5() {
    const dim5 = new Coordinate(NaN, 1000);
    assertTrue(isNaN(dim5.x));
    assertEquals(1000, dim5.y);
    assertEquals('(NaN, 1000)', dim5.toString());
  },

  testCoordinateSquaredDistance() {
    const a = new Coordinate(7, 11);
    const b = new Coordinate(3, -1);
    assertEquals(160, Coordinate.squaredDistance(a, b));
  },

  testCoordinateDistance() {
    const a = new Coordinate(-2, -3);
    const b = new Coordinate(2, 0);
    assertEquals(5, Coordinate.distance(a, b));
  },

  testCoordinateMagnitude() {
    const a = new Coordinate(5, 5);
    assertEquals(Math.sqrt(50), Coordinate.magnitude(a));
  },

  testCoordinateAzimuth() {
    const a = new Coordinate(5, 5);
    assertEquals(45, Coordinate.azimuth(a));
  },

  testCoordinateClone() {
    const c = new Coordinate();
    assertEquals(c.toString(), c.clone().toString());
    c.x = -12;
    c.y = 13;
    assertEquals(c.toString(), c.clone().toString());
  },

  testCoordinateEquals() {
    const a = new Coordinate(1, 2);

    assertFalse(a.equals(null));
    assertFalse(a.equals({}));
    assertFalse(a.equals(new Coordinate(1, 3)));
    assertFalse(a.equals(new Coordinate(2, 2)));

    assertTrue(a.equals(a));
    assertTrue(a.equals(new Coordinate(1, 2)));
  },

  testCoordinateDifference() {
    assertObjectEquals(
        new Coordinate(3, -40),
        Coordinate.difference(new Coordinate(5, 10), new Coordinate(2, 50)));
  },

  testCoordinateSum() {
    assertObjectEquals(
        new Coordinate(7, 60),
        Coordinate.sum(new Coordinate(5, 10), new Coordinate(2, 50)));
  },

  testCoordinateCeil() {
    let c = new Coordinate(5.2, 7.6);
    assertObjectEquals(new Coordinate(6, 8), c.ceil());
    c = new Coordinate(-1.2, -3.9);
    assertObjectEquals(new Coordinate(-1, -3), c.ceil());
  },

  testCoordinateFloor() {
    let c = new Coordinate(5.2, 7.6);
    assertObjectEquals(new Coordinate(5, 7), c.floor());
    c = new Coordinate(-1.2, -3.9);
    assertObjectEquals(new Coordinate(-2, -4), c.floor());
  },

  testCoordinateRound() {
    let c = new Coordinate(5.2, 7.6);
    assertObjectEquals(new Coordinate(5, 8), c.round());
    c = new Coordinate(-1.2, -3.9);
    assertObjectEquals(new Coordinate(-1, -4), c.round());
  },

  testCoordinateTranslateCoordinate() {
    const c = new Coordinate(10, 20);
    const t = new Coordinate(5, 10);
    // The translate function modifies the coordinate instead of
    // returning a new one.
    assertEquals(c, c.translate(t));
    assertObjectEquals(new Coordinate(15, 30), c);
  },

  testCoordinateTranslateXY() {
    const c = new Coordinate(10, 20);
    // The translate function modifies the coordinate instead of
    // returning a new one.
    assertEquals(c, c.translate(25, 5));
    assertObjectEquals(new Coordinate(35, 25), c);
  },

  testCoordinateTranslateX() {
    const c = new Coordinate(10, 20);
    // The translate function modifies the coordinate instead of
    // returning a new one.
    assertEquals(c, c.translate(5));
    assertObjectEquals(new Coordinate(15, 20), c);
  },

  testCoordinateScaleXY() {
    const c = new Coordinate(10, 15);
    // The scale function modifies the coordinate instead of returning a new
    // one.
    assertEquals(c, c.scale(2, 3));
    assertObjectEquals(new Coordinate(20, 45), c);
  },

  testCoordinateScaleFactor() {
    const c = new Coordinate(10, 15);
    // The scale function modifies the coordinate instead of returning a new
    // one.
    assertEquals(c, c.scale(2));
    assertObjectEquals(new Coordinate(20, 30), c);
  },

  testCoordinateRotateRadians() {
    const c = new Coordinate(15, 75);
    c.rotateRadians(Math.PI / 2, new Coordinate(10, 70));
    assertObjectEquals(new Coordinate(5, 75), c);
  },

  testCoordinateRotateDegrees() {
    const c = new Coordinate(15, 75);
    c.rotateDegrees(90, new Coordinate(10, 70));
    assertObjectEquals(new Coordinate(5, 75), c);
  },
});
