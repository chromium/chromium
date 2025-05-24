/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Unit tests for goog.testing.i18n.asserts.
 */

goog.module('goog.testing.i18n.assertsTest');
goog.setTestOnly();

const asserts = goog.require('goog.testing.i18n.asserts');
const testSuite = goog.require('goog.testing.testSuite');

// Add this mapping for testing only
asserts.addI18nMapping('mappedValue', 'newValue');
asserts.addI18nMapping('X\u0020Y', 'AB');

testSuite({
  /** @suppress {checkTypes} suppression added to enable type checking */
  testEdgeCases() {
    // Pass
    asserts.assertI18nEquals(null, null);
    asserts.assertI18nEquals('', '');

    // Fail
    assertThrowsJsUnitException(() => {
      asserts.assertI18nEquals(null, '');
    });
    assertThrowsJsUnitException(() => {
      asserts.assertI18nEquals(null, 'test');
    });
    assertThrowsJsUnitException(() => {
      asserts.assertI18nEquals('', null);
    });
    assertThrowsJsUnitException(() => {
      asserts.assertI18nEquals('', 'test');
    });
    assertThrowsJsUnitException(() => {
      asserts.assertI18nEquals('test', null);
    });
    assertThrowsJsUnitException(() => {
      asserts.assertI18nEquals('test', '');
    });
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testEdgeCases_withComments() {
    // Pass
    asserts.assertI18nEquals('Expect null values to match.', null, null);
    asserts.assertI18nEquals('Expect empty values to match.', '', '');

    // Fail
    assertThrowsJsUnitException(() => {
      asserts.assertI18nEquals(
          'Expect null and empty values to not match.', null, '');
    });
    assertThrowsJsUnitException(() => {
      asserts.assertI18nEquals(
          'Expect null and non-empty values to not match.', null, 'test');
    });
    assertThrowsJsUnitException(() => {
      asserts.assertI18nEquals(
          'Expect empty and null values to not match.', '', null);
    });
    assertThrowsJsUnitException(() => {
      asserts.assertI18nEquals(
          'Expect empty and non-empty values to not match.', '', 'test');
    });
    assertThrowsJsUnitException(() => {
      asserts.assertI18nEquals(
          'Expect non-empty and null values to not match.', 'test', null);
    });
    assertThrowsJsUnitException(() => {
      asserts.assertI18nEquals(
          'Expect non-empty and empty values to not match.', 'test', '');
    });
  },

  testContains() {
    // Real contains
    asserts.assertI18nContains('mappedValue', '** mappedValue');
    // i18n mapped contains
    asserts.assertI18nContains('mappedValue', '** newValue');

    // Negative testing
    assertThrowsJsUnitException(() => {
      asserts.assertI18nEquals('mappedValue', '** dummy');
    });

    // Check for containing with horizontal space matching

    asserts.assertI18nContains('abc', ' abc\u1680');
    asserts.assertI18nContains('abc', '\u202fabc \u3000');
    asserts.assertI18nContains('abc', '\u202fabc \u3000');
    asserts.assertI18nContains('a b c', '\u202fabc \u3000');
    asserts.assertI18nContains('a\u202fb\t\xA0c', '\u202fabc \u3000');

  },

  testMappingWorks() {
    // Real equality
    asserts.assertI18nEquals('test', 'test');
    // i18n mapped equality
    asserts.assertI18nEquals('mappedValue', 'newValue');

    // i18n-mapped equality with extra whitespace being removed before lookup
    asserts.assertI18nEquals('  mapped Value ', 'newValue');

    // Negative testing
    assertThrowsJsUnitException(() => {
      asserts.assertI18nEquals('unmappedValue', 'newValue');
    });
  },

  testWhiteSpaceStringWorks() {
    asserts.assertI18nEquals(' ', '\u1680');
    asserts.assertI18nEquals('a\u2001b ', 'a\u3000b');
    asserts.assertI18nEquals('  ab  ', 'a\u3000b');
    asserts.assertI18nEquals('a\u2001b ', 'a\u3000b');
    asserts.assertI18nEquals('\ta\u00a0\u0020b\u205fC ', 'abC');

    // And check mapped value with flexible space mapping, using several
    // strings with different white space characters
    const expectedValue = 'X\u0020Y';
    asserts.assertI18nEquals(expectedValue, 'X\u0020Y');
    asserts.assertI18nEquals(expectedValue, 'XY');
    asserts.assertI18nEquals(expectedValue, 'X\u202fY');
    asserts.assertI18nEquals(expectedValue, 'X\t\u00a0Y');

    // Check that the given expected value is also mapped to a different value
    // and that is compared with the same whitespace removal rules.
    asserts.assertI18nEquals(expectedValue, 'AB');
    asserts.assertI18nEquals(expectedValue, ' A B ');
    asserts.assertI18nEquals(expectedValue, 'A\tB');
    asserts.assertI18nEquals(expectedValue, 'A\u202fB');
    asserts.assertI18nEquals(expectedValue, 'A\u00a0B');
    asserts.assertI18nEquals(expectedValue, 'AB\u2000\t');
    asserts.assertI18nEquals(expectedValue, '\u0020\u2002AB\u3000\t');
  }

});
