/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.fx.easingTest');
goog.setTestOnly();

const easing = goog.require('goog.fx.easing');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testEaseIn() {
    assertEquals(0, easing.easeIn(0));
    assertEquals(1, easing.easeIn(1));
    assertRoughlyEquals(Math.pow(0.5, 3), easing.easeIn(0.5), 0.01);
  },

  testEaseOut() {
    assertEquals(0, easing.easeOut(0));
    assertEquals(1, easing.easeOut(1));
    assertRoughlyEquals(1 - Math.pow(0.5, 3), easing.easeOut(0.5), 0.01);
  },
});
