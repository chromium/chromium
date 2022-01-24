/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.math.tdmaTest');
goog.setTestOnly();

const tdma = goog.require('goog.math.tdma');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testTdmaSolver() {
    const supDiag = [1, 1, 1, 1, 1];
    const mainDiag = [-1, -2, -2, -2, -2, -2];
    const subDiag = [1, 1, 1, 1, 1];
    const vecRight = [1, 2, 3, 4, 5, 6];
    const expected = [-56, -55, -52, -46, -36, -21];
    const result = [];
    tdma.solve(subDiag, mainDiag, supDiag, vecRight, result);
    assertElementsEquals(expected, result);
  },
});
