/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Assert functions that account for locale data changes.
 *
 * The locale data gets updated from CLDR (http://cldr.unicode.org/),
 * and CLDR gets an update about twice per year.
 * So the locale data are expected to change.
 * This can make unit tests quite fragile:
 *   assertEquals("Dec 31, 2013, 1:23pm", format);
 * Now imagine that the decision is made to add a dot after abbreviations,
 * and a comma between date and time.
 * The previous assert will fail, because the string is now
 *   "Dec. 31 2013, 1:23pm"
 *
 * One option is to not unit test the results of the formatters client side,
 * and just trust that CLDR and closure/i18n takes care of that.
 * The other option is to be a more flexible when testing.
 * This is the role of assertI18nEquals, to centralize all the small
 * differences between hard-coded values in unit tests and the current result.
 * It allows some decupling, so that the closure/i18n can be updated without
 * breaking all the clients using it.
 * For the example above, this will succeed:
 *   assertI18nEquals("Dec 31, 2013, 1:23pm", "Dec. 31, 2013 1:23pm");
 * It does this by white-listing, no "guessing" involved.
 *
 * But I would say that the best practice is the first option: trust the
 * library, stop unit-testing it.
 */

goog.provide('goog.testing.i18n.asserts');
goog.setTestOnly('goog.testing.i18n.asserts');

goog.require('goog.testing.jsunit');


/**
 * A map of known tests where locale data changed, but the old values are
 * still tested for by various clients.
 * @const {!Object<string, string>}
 * @private
 */
goog.testing.i18n.asserts.EXPECTED_VALUE_MAP_ = {
    // NOTE: Add mappings for each test file using addI18nMapping.
};


/**
 * Asserts that the two values are "almost equal" from i18n perspective.
 * I18n-equivalent strings are set with addI18nMapping.
 *
 * @param {string} expected The expected value.
 * @param {string} actual The actual value.
 */
goog.testing.i18n.asserts.assertI18nEquals = function(expected, actual) {
  'use strict';
  if (expected === actual) {
    return;
  }

  const newExpected = goog.testing.i18n.asserts.EXPECTED_VALUE_MAP_[expected];
  if (newExpected === actual) {
    return;
  }

  assertEquals(expected, actual);
};


/**
 * Asserts that needle, or a string i18n-equivalent to needle, is a substring of
 * haystack. I18n-equivalent strings are set with addI18nMapping.
 *
 * @param {string} needle The substring to search for.
 * @param {string} haystack The string to search within.
 */
goog.testing.i18n.asserts.assertI18nContains = function(needle, haystack) {
  'use strict';
  if (needle === haystack) {
    return;
  }

  const newNeedle = goog.testing.i18n.asserts.EXPECTED_VALUE_MAP_[needle];
  if (haystack.indexOf(newNeedle) !== -1) {
    return;
  }

  assertContains(needle, haystack);
};


/**
 * Adds two strings as being i18n-equivalent. Call this
 * method in your unit test file to add mappings scoped to the file.
 *
 * @param {string} expected The expected string in assertI18nEquals.
 * @param {string} equivalent A string which is i18n-equal.
 */
goog.testing.i18n.asserts.addI18nMapping = function(expected, equivalent) {
  'use strict';
  if (goog.testing.i18n.asserts.EXPECTED_VALUE_MAP_.hasOwnProperty(expected)) {
    throw new RangeError('Mapping for string already exists');
  }
  goog.testing.i18n.asserts.EXPECTED_VALUE_MAP_[expected] = equivalent;
};
