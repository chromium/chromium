/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.math.interpolator.Linear1Test');
goog.setTestOnly();

const Linear1 = goog.require('goog.math.interpolator.Linear1');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testLinear() {
    // Test special case with no data to interpolate.
    let x = [];
    let y = [];
    let interp = new Linear1();
    interp.setData(x, y);
    assertTrue(isNaN(interp.interpolate(1)));

    // Test special case with 1 data point.
    x = [0];
    y = [3];
    interp = new Linear1();
    interp.setData(x, y);
    assertRoughlyEquals(3, interp.interpolate(1), 1e-4);

    // Test general case.
    x = [0, 1, 3, 6, 7];
    y = [0, 0, 0, 0, 0];
    for (let i = 0; i < x.length; ++i) {
      y[i] = Math.sin(x[i]);
    }
    interp = new Linear1();
    interp.setData(x, y);

    const xi = [0, 0.5, 1, 2, 3, 4, 5, 6, 7];
    const expected =
        [0, 0.4207, 0.8415, 0.4913, 0.1411, 0.0009, -0.1392, -0.2794, 0.657];
    const result = [0, 0, 0, 0, 0, 0, 0, 0, 0];
    for (let i = 0; i < xi.length; ++i) {
      result[i] = interp.interpolate(xi[i]);
    }
    assertElementsRoughlyEqual(expected, result, 1e-4);
  },

  testOutOfBounds() {
    const x = [0, 1, 2];
    const y = [2, 5, 4];
    const interp = new Linear1();
    interp.setData(x, y);
    assertRoughlyEquals(interp.interpolate(-1), -1, 1e-4);
    assertRoughlyEquals(interp.interpolate(4), 2, 1e-4);
  },

  testInverse() {
    const x = [0, 1, 3, 6, 7];
    const y = [0, 2, 7, 8, 10];

    const interp = new Linear1();
    interp.setData(x, y);
    const invInterp = interp.getInverse();

    const xi = [0, 0.5, 1, 2, 3, 4, 5, 6, 7];
    const yi = [0, 1, 2, 4.5, 7, 7.3333, 7.6667, 8, 10];
    const resultX = [0, 0, 0, 0, 0, 0, 0, 0, 0];
    const resultY = [0, 0, 0, 0, 0, 0, 0, 0, 0];
    for (let i = 0; i < xi.length; ++i) {
      resultY[i] = interp.interpolate(xi[i]);
      resultX[i] = invInterp.interpolate(yi[i]);
    }
    assertElementsRoughlyEqual(xi, resultX, 1e-4);
    assertElementsRoughlyEqual(yi, resultY, 1e-4);
  },
});
