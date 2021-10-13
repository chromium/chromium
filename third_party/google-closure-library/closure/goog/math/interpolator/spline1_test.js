/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.math.interpolator.Spline1Test');
goog.setTestOnly();

const Spline1 = goog.require('goog.math.interpolator.Spline1');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testSpline() {
    // Test special case with no data to interpolate.
    let x = [];
    let y = [];
    let interp = new Spline1();
    interp.setData(x, y);
    assertTrue(isNaN(interp.interpolate(1)));

    // Test special case with 1 data point.
    x = [0];
    y = [2];
    interp = new Spline1();
    interp.setData(x, y);
    assertRoughlyEquals(2, interp.interpolate(1), 1e-4);

    // Test special case with 2 data points.
    x = [0, 1];
    y = [2, 5];
    interp = new Spline1();
    interp.setData(x, y);
    assertRoughlyEquals(3.5, interp.interpolate(.5), 1e-4);

    // Test special case with 3 data points.
    x = [0, 1, 2];
    y = [2, 5, 4];
    interp = new Spline1();
    interp.setData(x, y);
    assertRoughlyEquals(4, interp.interpolate(.5), 1e-4);
    assertRoughlyEquals(-1, interp.interpolate(3), 1e-4);

    // Test general case.
    x = [0, 1, 3, 6, 7];
    y = [0, 0, 0, 0, 0];
    for (let i = 0; i < x.length; ++i) {
      y[i] = Math.sin(x[i]);
    }
    interp = new Spline1();
    interp.setData(x, y);

    const xi = [0, 0.5, 1, 2, 3, 4, 5, 6, 7];
    const expected =
        [0, 0.5775, 0.8415, 0.7047, 0.1411, -0.3601, -0.55940, -0.2794, 0.6570];
    const result = [0, 0, 0, 0, 0, 0, 0, 0, 0];
    for (let i = 0; i < xi.length; ++i) {
      result[i] = interp.interpolate(xi[i]);
    }
    assertElementsRoughlyEqual(expected, result, 1e-4);
  },

  testOutOfBounds() {
    const x = [0, 1, 2, 4];
    const y = [2, 5, 4, 1];
    const interp = new Spline1();
    interp.setData(x, y);
    assertRoughlyEquals(-7.75, interp.interpolate(-1), 1e-4);
    assertRoughlyEquals(4.5, interp.interpolate(5), 1e-4);
  },

  testInverse() {
    const x = [0, 1, 3, 6, 7];
    const y = [0, 2, 7, 8, 10];

    const interp = new Spline1();
    interp.setData(x, y);
    const invInterp = interp.getInverse();

    const xi = [0, 0.5, 1, 2, 3, 4, 5, 6, 7];
    const yi = [0, 0.8159, 2, 4.7892, 7, 7.6912, 7.6275, 8, 10];
    const expectedX = [0, 0.8142, 1, 0.2638, 3, 5.0534, 4.8544, 6, 7];
    const resultX = [0, 0, 0, 0, 0, 0, 0, 0, 0];
    const resultY = [0, 0, 0, 0, 0, 0, 0, 0, 0];
    for (let i = 0; i < xi.length; ++i) {
      resultY[i] = interp.interpolate(xi[i]);
      resultX[i] = invInterp.interpolate(yi[i]);
    }
    assertElementsRoughlyEqual(expectedX, resultX, 1e-4);
    assertElementsRoughlyEqual(yi, resultY, 1e-4);
  },
});
