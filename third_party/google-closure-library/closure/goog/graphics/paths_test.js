/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.graphics.pathsTest');
goog.setTestOnly();

const Coordinate = goog.require('goog.math.Coordinate');
const dom = goog.require('goog.dom');
const googGraphics = goog.require('goog.graphics');
const paths = goog.require('goog.graphics.paths');
const testSuite = goog.require('goog.testing.testSuite');

// The purpose of this test is less about the actual unit test, and
// more for drawing demos of shapes.
const regularNGon = paths.createRegularNGon;
const arrow = paths.createArrow;

function assertArrayRoughlyEquals(expected, actual, delta) {
  const message = `Expected: ${expected}, Actual: ${actual}`;
  assertEquals(`Wrong length. ${message}`, expected.length, actual.length);
  for (let i = 0; i < expected.length; i++) {
    assertRoughlyEquals(
        `Wrong item at ${i}. ${message}`, expected[i], actual[i], delta);
  }
}

function $coord(x, y) {
  return new Coordinate(x, y);
}

testSuite({
  setUp() {
    dom.removeChildren(dom.getElement('root'));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSquare() {
    const square = regularNGon($coord(10, 10), $coord(0, 10), 4);
    assertArrayRoughlyEquals(
        [0, 10, 10, 0, 20, 10, 10, 20], square.arguments_, 0.05);
  },

  tearDownPage() {
    const root = dom.getElement('root');
    const graphics = googGraphics.createGraphics(800, 600);

    const blueFill = new googGraphics.SolidFill('blue');
    const blackStroke = new googGraphics.Stroke(1, 'black');
    graphics.drawPath(
        regularNGon($coord(20, 50), $coord(0, 20), 3), blackStroke, blueFill);
    graphics.drawPath(
        regularNGon($coord(120, 50), $coord(100, 20), 4), blackStroke,
        blueFill);
    graphics.drawPath(
        regularNGon($coord(220, 50), $coord(200, 20), 5), blackStroke,
        blueFill);
    graphics.drawPath(
        regularNGon($coord(320, 50), $coord(300, 20), 6), blackStroke,
        blueFill);

    graphics.drawPath(
        arrow($coord(0, 300), $coord(100, 400), 0, 0), blackStroke, blueFill);
    graphics.drawPath(
        arrow($coord(120, 400), $coord(200, 300), 0, 10), blackStroke,
        blueFill);
    graphics.drawPath(
        arrow($coord(220, 300), $coord(300, 400), 10, 0), blackStroke,
        blueFill);
    graphics.drawPath(
        arrow($coord(320, 400), $coord(400, 300), 10, 10), blackStroke,
        blueFill);

    root.appendChild(graphics.getElement());
  },
});
