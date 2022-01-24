/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Unit tests for paths. */

goog.module('goog.math.pathsTest');
goog.setTestOnly();

const Coordinate = goog.require('goog.math.Coordinate');
const paths = goog.require('goog.math.paths');
const testSuite = goog.require('goog.testing.testSuite');

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
  /** @suppress {visibility} suppression added to enable type checking */
  testSquare() {
    const square = regularNGon($coord(10, 10), $coord(0, 10), 4);
    assertArrayRoughlyEquals(
        [0, 10, 10, 0, 20, 10, 10, 20], square.arguments_, 0.05);
  },
});
