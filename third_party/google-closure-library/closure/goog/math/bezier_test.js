/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.math.BezierTest');
goog.setTestOnly();

const Bezier = goog.require('goog.math.Bezier');
const Coordinate = goog.require('goog.math.Coordinate');
const googMath = goog.require('goog.math');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testEquals() {
    const input = new Bezier(1, 2, 3, 4, 5, 6, 7, 8);

    assert(input.equals(input));
  },

  testClone() {
    const input = new Bezier(1, 2, 3, 4, 5, 6, 7, 8);

    assertNotEquals('Clone returns a new object', input, input.clone());
    assert('Contents of clone match original', input.equals(input.clone()));
  },

  testFlip() {
    const input = new Bezier(1, 1, 2, 2, 3, 3, 4, 4);
    const compare = new Bezier(4, 4, 3, 3, 2, 2, 1, 1);

    const flipped = input.clone();
    flipped.flip();
    assert('Flipped behaves as expected', compare.equals(flipped));

    flipped.flip();
    assert('Flipping twice gives original', input.equals(flipped));
  },

  testGetPoint() {
    const input = new Bezier(0, 1, 1, 2, 2, 3, 3, 4);

    assert(Coordinate.equals(input.getPoint(0), new Coordinate(0, 1)));
    assert(Coordinate.equals(input.getPoint(1), new Coordinate(3, 4)));
    assert(Coordinate.equals(input.getPoint(0.5), new Coordinate(1.5, 2.5)));
  },

  testGetPointX() {
    const input = new Bezier(0, 1, 1, 2, 2, 3, 3, 4);

    assert(googMath.nearlyEquals(input.getPointX(0), 0));
    assert(googMath.nearlyEquals(input.getPointX(1), 3));
    assert(googMath.nearlyEquals(input.getPointX(0.5), 1.5));
  },

  testGetPointY() {
    const input = new Bezier(0, 1, 1, 2, 2, 3, 3, 4);

    assert(googMath.nearlyEquals(input.getPointY(0), 1));
    assert(googMath.nearlyEquals(input.getPointY(1), 4));
    assert(googMath.nearlyEquals(input.getPointY(0.5), 2.5));
  },

  testSubdivide() {
    const input = new Bezier(0, 1, 1, 2, 2, 3, 3, 4);

    input.subdivide(1 / 3, 2 / 3);

    assert(googMath.nearlyEquals(1, input.x0));
    assert(googMath.nearlyEquals(2, input.y0));
    assert(googMath.nearlyEquals(2, input.x3));
    assert(googMath.nearlyEquals(3, input.y3));
  },

  testSolvePositionFromXValue() {
    const eps = 1e-6;
    const bezier = new Bezier(0, 0, 0.25, 0.1, 0.25, 1, 1, 1);
    const pt = bezier.getPoint(0.5);
    assertRoughlyEquals(0.3125, pt.x, eps);
    assertRoughlyEquals(0.5375, pt.y, eps);
    assertRoughlyEquals(
        0.321, bezier.solvePositionFromXValue(bezier.getPoint(0.321).x), eps);
  },

  testSolveYValueFromXValue() {
    const eps = 1e-6;
    // The following example is taken from
    // http://www.netzgesta.de/dev/cubic-bezier-timing-function.html.
    // The timing values shown in that page are 1 - <value> so the
    // bezier curves in this test are constructed with 1 - ctrl points.
    // E.g. ctrl points (0, 0, 0.25, 0.1, 0.25, 1, 1, 1) become
    // (1, 1, 0.75, 0, 0.75, 0.9, 0, 0) here. Since chanding the order of
    // the ctrl points does not affect the shape of the curve, once can also
    // have (0, 0, 0.75, 0.9, 0.75, 0, 1, 1).

    // netzgesta example.
    let bezier = new Bezier(1, 1, 0.75, 0.9, 0.75, 0, 0, 0);
    assertRoughlyEquals(0.024374631, bezier.solveYValueFromXValue(0.2), eps);
    assertRoughlyEquals(0.317459494, bezier.solveYValueFromXValue(0.6), eps);
    assertRoughlyEquals(0.905205002, bezier.solveYValueFromXValue(0.9), eps);

    // netzgesta example with ctrl points in the reverse order so that 1st and
    // last ctrl points are (0, 0) and (1, 1). Note the result is exactly the
    // same.
    bezier = new Bezier(0, 0, 0.75, 0, 0.75, 0.9, 1, 1);
    assertRoughlyEquals(0.024374631, bezier.solveYValueFromXValue(0.2), eps);
    assertRoughlyEquals(0.317459494, bezier.solveYValueFromXValue(0.6), eps);
    assertRoughlyEquals(0.905205002, bezier.solveYValueFromXValue(0.9), eps);

    // Ease-out css animation timing in webkit.
    bezier = new Bezier(0, 0, 0, 0, 0.58, 1, 1, 1);
    assertRoughlyEquals(0.308366667, bezier.solveYValueFromXValue(0.2), eps);
    assertRoughlyEquals(0.785139061, bezier.solveYValueFromXValue(0.6), eps);
    assertRoughlyEquals(0.982973389, bezier.solveYValueFromXValue(0.9), eps);
  },
});
