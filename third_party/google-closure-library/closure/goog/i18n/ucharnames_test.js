/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.i18n.uCharNamesTest');
goog.setTestOnly();

const testSuite = goog.require('goog.testing.testSuite');
const uCharNames = goog.require('goog.i18n.uCharNames');

testSuite({
  testToName() {
    const result = uCharNames.toName(' ');
    assertEquals('Space', result);
  },

  testToNameForNumberKey() {
    const result = uCharNames.toName('\u2028');
    assertEquals('Line Separator', result);
  },

  testToNameForVariationSelector() {
    const result = uCharNames.toName('\ufe00');
    assertEquals('Variation Selector - 1', result);
  },

  testToNameForVariationSelectorSupp() {
    const result = uCharNames.toName('\uDB40\uDD00');
    assertEquals('Variation Selector - 17', result);
  },

  testToNameForNull() {
    const result = uCharNames.toName('a');
    assertNull(result);
  },
});
