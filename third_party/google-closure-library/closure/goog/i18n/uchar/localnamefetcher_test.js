/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.i18n.uChar.LocalNameFetcherTest');
goog.setTestOnly();

const LocalNameFetcher = goog.require('goog.i18n.uChar.LocalNameFetcher');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

let nameFetcher = null;

testSuite({
  setUp() {
    nameFetcher = new LocalNameFetcher();
  },

  testGetName_exists() {
    const callback = recordFunction((name) => {
      assertEquals('Space', name);
    });
    nameFetcher.getName(' ', callback);
    assertEquals(1, callback.getCallCount());
  },

  testGetName_variationSelector() {
    const callback = recordFunction((name) => {
      assertEquals('Variation Selector - 1', name);
    });
    nameFetcher.getName('\ufe00', callback);
    assertEquals(1, callback.getCallCount());
  },

  testGetName_missing() {
    const callback = recordFunction((name) => {
      assertNull(name);
    });
    nameFetcher.getName('P', callback);
    assertEquals(1, callback.getCallCount());
  },

  testIsNameAvailable_withAvailableName() {
    assertTrue(nameFetcher.isNameAvailable(' '));
  },

  testIsNameAvailable_withoutAvailableName() {
    assertFalse(nameFetcher.isNameAvailable('a'));
  },
});
