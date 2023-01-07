/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.math.interpolator.Pchip1Test');
goog.setTestOnly();

const Pchip1 = goog.require('goog.math.interpolator.Pchip1');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testSpline() {
    const x = [0, 1, 3, 6, 7];
    const y = [0, 0, 0, 0, 0];

    for (let i = 0; i < x.length; ++i) {
      y[i] = Math.sin(x[i]);
    }
    const interp = new Pchip1();
    interp.setData(x, y);

    const xi = [0, 0.5, 1, 2, 3, 4, 5, 6, 7];
    const expected =
        [0, 0.5756, 0.8415, 0.5428, 0.1411, -0.0595, -0.2162, -0.2794, 0.657];
    const result = [0, 0, 0, 0, 0, 0, 0, 0, 0];
    for (let i = 0; i < xi.length; ++i) {
      result[i] = interp.interpolate(xi[i]);
    }
    assertElementsRoughlyEqual(expected, result, 1e-4);
  },

  testOutOfBounds() {
    const x = [0, 1, 2, 4];
    const y = [2, 5, 4, 2];
    const interp = new Pchip1();
    interp.setData(x, y);
    assertRoughlyEquals(-3, interp.interpolate(-1), 1e-4);
    assertRoughlyEquals(1, interp.interpolate(5), 1e-4);
  },

  testInverse() {
    const x = [0, 1, 3, 6, 7];
    const y = [0, 2, 7, 8, 10];

    const interp = new Pchip1();
    interp.setData(x, y);
    const invInterp = interp.getInverse();

    const xi = [0, 0.5, 1, 2, 3, 4, 5, 6, 7];
    const yi = [0, 0.9548, 2, 4.8938, 7, 7.3906, 7.5902, 8, 10];
    const expectedX = [0, 0.888, 1, 0.2852, 3, 4.1206, 4.7379, 6, 7];
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
