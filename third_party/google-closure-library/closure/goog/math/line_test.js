/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.math.LineTest');
goog.setTestOnly();

const Coordinate = goog.require('goog.math.Coordinate');
const Line = goog.require('goog.math.Line');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testEquals() {
    const input = new Line(1, 2, 3, 4);

    assert(input.equals(input));
  },

  testClone() {
    const input = new Line(1, 2, 3, 4);

    assertNotEquals('Clone returns a new object', input, input.clone());
    assertTrue('Contents of clone match original', input.equals(input.clone()));
  },

  testGetLength() {
    const input = new Line(0, 0, Math.sqrt(2), Math.sqrt(2));
    assertRoughlyEquals(input.getSegmentLengthSquared(), 4, 1e-10);
    assertRoughlyEquals(input.getSegmentLength(), 2, 1e-10);
  },

  testGetClosestPoint() {
    const input = new Line(0, 1, 1, 2);

    const point = input.getClosestPoint(0, 3);
    assertRoughlyEquals(point.x, 1, 1e-10);
    assertRoughlyEquals(point.y, 2, 1e-10);
  },

  testGetClosestSegmentPoint() {
    const input = new Line(0, 1, 2, 3);

    let point = input.getClosestSegmentPoint(4, 4);
    assertRoughlyEquals(point.x, 2, 1e-10);
    assertRoughlyEquals(point.y, 3, 1e-10);

    point = input.getClosestSegmentPoint(new Coordinate(-1, -10));
    assertRoughlyEquals(point.x, 0, 1e-10);
    assertRoughlyEquals(point.y, 1, 1e-10);
  },
});
