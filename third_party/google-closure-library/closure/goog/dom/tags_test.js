/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.tagsTest');
goog.setTestOnly();

const tags = goog.require('goog.dom.tags');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testIsVoidTag() {
    assertTrue(tags.isVoidTag('br'));
    assertFalse(tags.isVoidTag('a'));
    assertFalse(tags.isVoidTag('constructor'));
  },
});
