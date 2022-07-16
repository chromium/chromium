/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.TagNameTest');
goog.setTestOnly();

const TagName = goog.require('goog.dom.TagName');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testCorrectNumberOfTagNames() {
    assertEquals(
        130,
        Object.entries(TagName)
            .filter(([k, v]) => typeof v === 'string')
            .length);
  },

  testPropertyNamesEqualValues() {
    Object.entries(TagName)
        .filter(([k, v]) => typeof v === 'string')
        .forEach(([k, v]) => {
          assertEquals(k, v);
        });
  },
});
