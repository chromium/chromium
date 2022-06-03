/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.graphics.ext.coordinatesTest');
goog.setTestOnly();

const coordinates = goog.require('goog.graphics.ext.coordinates');
const graphics = goog.require('goog.graphics');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  /** @suppress {visibility} suppression added to enable type checking */
  testIsPercent() {
    assert('50% is a percent', coordinates.isPercent_('50%'));
    assert('50 is not a percent', !coordinates.isPercent_('50'));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testIsPixels() {
    assert('50px is pixels', coordinates.isPixels_('50px'));
    assert('50 is not pixels', !coordinates.isPixels_('50'));
  },

  testIsSpecial() {
    assert('50px is special', coordinates.isSpecial('50px'));
    assert('50% is special', coordinates.isSpecial('50%'));
    assert('50 is not special', !coordinates.isSpecial('50'));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testComputeValue() {
    assertEquals(
        '50% of 100 is 50', 50, coordinates.computeValue('50%', 100, null));
    assertEquals(
        '50.5% of 200 is 101', 101,
        coordinates.computeValue('50.5%', 200, null));
    assertEquals(
        '50px = 25 units when in 2x view', 25,
        coordinates.computeValue('50px', null, 2));
  },

  testGenericGetValue() {
    const getValue = coordinates.getValue;

    let cache = {};

    assertEquals('Testing 50%', 50, getValue('50%', false, 100, 2, cache));

    let count = 0;
    for (let x in cache) {
      count++;
      cache[x] = 'OVERWRITE';
    }

    assertEquals('Testing cache size', 1, count);
    assertEquals(
        'Testing cache usage', 'OVERWRITE',
        getValue('50%', false, 100, 2, cache));

    cache = {};

    assertEquals('Testing 0%', 0, getValue('0%', false, 100, 2, cache));
  },
});
