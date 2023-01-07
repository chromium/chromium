/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.i18n.pluralRulesTest');
goog.setTestOnly();

const LocaleFeature = goog.require('goog.i18n.LocaleFeature');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const pluralRules = goog.require('goog.i18n.pluralRules');
const testSuite = goog.require('goog.testing.testSuite');

let propertyReplacer;

/** @suppress {missingRequire} */
const Keyword = pluralRules.Keyword;

// Tests both JavaScript and ECMAScript on supporting browsers.
testSuite({
  setUpPage() {
    propertyReplacer = new PropertyReplacer();
  },

  setUp() {
    propertyReplacer.replace(goog, 'LOCALE', 'en');
    propertyReplacer.replace(
        LocaleFeature, 'USE_ECMASCRIPT_I18N_PLURALRULES', false);
  },

  getTestName: function() {
    return 'PluralRules Tests';
  },

  testSimpleSelectEn() {
    /** @suppress {visibility} suppression added to enable type checking */
    const funcSelect = pluralRules.enSelect_;

    assertEquals(Keyword.OTHER, funcSelect(0));  // 0 dollars
    assertEquals(Keyword.ONE, funcSelect(1));    // 1 dollar
    assertEquals(Keyword.OTHER, funcSelect(2));  // 2 dollars

    assertEquals(Keyword.OTHER, funcSelect(0, 2));  // 0.00 dollars
    assertEquals(Keyword.OTHER, funcSelect(1, 2));  // 1.00 dollars
    assertEquals(Keyword.OTHER, funcSelect(2, 2));  // 2.00 dollars
  },

  testSimpleSelectEnNative() {
    // Ignore test when Intl class doesn't implement native mode.
    if (undefined === Intl.PluralRules) return;

    propertyReplacer.replace(goog, 'LOCALE', 'en');
    propertyReplacer.replace(
        LocaleFeature, 'USE_ECMASCRIPT_I18N_PLURALRULES', true);

    /** @suppress {visibility} suppression added to enable type checking */
    const funcSelect = pluralRules.mapToNativeSelect_();
    assert(funcSelect != null);

    assertEquals(Keyword.OTHER, funcSelect(0));  // 0 dollars
    assertEquals(Keyword.ONE, funcSelect(1));    // 1 dollar
    assertEquals(Keyword.OTHER, funcSelect(2));  // 2 dollars

    // Need new function for different precision?
    assertEquals(Keyword.OTHER, funcSelect(0, 2));  // 0.00 dollars
    assertEquals(Keyword.OTHER, funcSelect(1, 2));  // 1.00 dollars
    assertEquals(Keyword.OTHER, funcSelect(2, 2));  // 2.00 dollars
  },

  testSimpleNativeMultiPrecision() {
    if (undefined === Intl.PluralRules) return;

    propertyReplacer.replace(goog, 'LOCALE', 'en');

    propertyReplacer.replace(
        LocaleFeature, 'USE_ECMASCRIPT_I18N_PLURALRULES', true);

    /** @suppress {visibility} suppression added to enable type checking */
    const funcSelect = pluralRules.mapToNativeSelect_();
    assert(funcSelect != null);

    // Test creating with precision set first.
    assertEquals(Keyword.OTHER, funcSelect(0, 2));  // 0.00 dollars
    assertEquals(Keyword.OTHER, funcSelect(0));     // 0 dollars
    assertEquals(Keyword.OTHER, funcSelect(7, 2));  // 7.00 dollars
    assertEquals(Keyword.ONE, funcSelect(1));       // 1 dollars

    assertEquals(Keyword.OTHER, funcSelect(0, 1));   // 0.00 dollars
    assertEquals(Keyword.OTHER, funcSelect(-3, 1));  // -2.00 dollars
    assertEquals(Keyword.OTHER, funcSelect(1, 17));  // 1.00 dollars
    assertEquals(Keyword.OTHER, funcSelect(0, 17));  // 1.00 dollars
  },

  testSimpleSelectRo() {
    /** @suppress {visibility} suppression added to enable type checking */
    const funcSelect = pluralRules.roSelect_;

    assertEquals(Keyword.FEW, funcSelect(0));       // 0 dolari
    assertEquals(Keyword.ONE, funcSelect(1));       // 1 dolar
    assertEquals(Keyword.FEW, funcSelect(2));       // 2 dolari
    assertEquals(Keyword.FEW, funcSelect(12));      // 12 dolari
    assertEquals(Keyword.OTHER, funcSelect(23));    // 23 de dolari
    assertEquals(Keyword.FEW, funcSelect(1212));    // 1212 dolari
    assertEquals(Keyword.OTHER, funcSelect(1223));  // 1223 de dolari

    assertEquals(Keyword.FEW, funcSelect(0, 2));     // 0.00 dolari
    assertEquals(Keyword.FEW, funcSelect(1, 2));     // 1.00 dolari
    assertEquals(Keyword.FEW, funcSelect(2, 2));     // 2.00 dolari
    assertEquals(Keyword.FEW, funcSelect(12, 2));    // 12.00 dolari
    assertEquals(Keyword.FEW, funcSelect(23, 2));    // 23.00  dolari
    assertEquals(Keyword.FEW, funcSelect(1212, 2));  // 1212.00  dolari
    assertEquals(Keyword.FEW, funcSelect(1223, 2));  // 1223.00 dolari
  },

  testSimpleSelectRoNative() {
    if (undefined === Intl.PluralRules) return;

    propertyReplacer.replace(goog, 'LOCALE', 'ro');
    propertyReplacer.replace(
        LocaleFeature, 'USE_ECMASCRIPT_I18N_PLURALRULES', true);

    /** @suppress {visibility} suppression added to enable type checking */
    const funcSelect = pluralRules.mapToNativeSelect_();
    assert(funcSelect != null);

    assertEquals(Keyword.FEW, funcSelect(0));       // 0 dolari
    assertEquals(Keyword.ONE, funcSelect(1));       // 1 dolar
    assertEquals(Keyword.FEW, funcSelect(2));       // 2 dolari
    assertEquals(Keyword.FEW, funcSelect(12));      // 12 dolari
    assertEquals(Keyword.OTHER, funcSelect(23));    // 23 de dolari
    assertEquals(Keyword.FEW, funcSelect(1212));    // 1212 dolari
    assertEquals(Keyword.OTHER, funcSelect(1223));  // 1223 de dolari

    assertEquals(Keyword.FEW, funcSelect(0, 2));     // 0.00 dolari
    assertEquals(Keyword.FEW, funcSelect(1, 2));     // 1.00 dolari
    assertEquals(Keyword.FEW, funcSelect(2, 2));     // 2.00 dolari
    assertEquals(Keyword.FEW, funcSelect(12, 2));    // 12.00 dolari
    assertEquals(Keyword.FEW, funcSelect(23, 2));    // 23.00  dolari
    assertEquals(Keyword.FEW, funcSelect(1212, 2));  // 1212.00  dolari
    assertEquals(Keyword.FEW, funcSelect(1223, 2));  // 1223.00 dolari
  },

  testSimpleSelectSr() {
    /** @suppress {visibility} suppression added to enable type checking */
    const funcSelect = pluralRules.srSelect_;  // Serbian

    assertEquals(Keyword.ONE, funcSelect(1));
    assertEquals(Keyword.ONE, funcSelect(31));
    assertEquals(Keyword.ONE, funcSelect(0.1));
    assertEquals(Keyword.ONE, funcSelect(1.1));
    assertEquals(Keyword.ONE, funcSelect(2.1));

    assertEquals(Keyword.FEW, funcSelect(3));
    assertEquals(Keyword.FEW, funcSelect(33));
    assertEquals(Keyword.FEW, funcSelect(0.2));
    assertEquals(Keyword.FEW, funcSelect(0.3));
    assertEquals(Keyword.FEW, funcSelect(0.4));
    assertEquals(Keyword.FEW, funcSelect(2.2));

    assertEquals(Keyword.OTHER, funcSelect(2.11));
    assertEquals(Keyword.OTHER, funcSelect(2.12));
    assertEquals(Keyword.OTHER, funcSelect(2.13));
    assertEquals(Keyword.OTHER, funcSelect(2.14));
    assertEquals(Keyword.OTHER, funcSelect(2.15));

    assertEquals(Keyword.OTHER, funcSelect(0));
    assertEquals(Keyword.OTHER, funcSelect(5));
    assertEquals(Keyword.OTHER, funcSelect(10));
    assertEquals(Keyword.OTHER, funcSelect(35));
    assertEquals(Keyword.OTHER, funcSelect(37));
    assertEquals(Keyword.OTHER, funcSelect(40));
    assertEquals(Keyword.OTHER, funcSelect(0.0, 1));
    assertEquals(Keyword.OTHER, funcSelect(0.5));
    assertEquals(Keyword.OTHER, funcSelect(0.6));

    assertEquals(Keyword.FEW, funcSelect(2));
    assertEquals(Keyword.ONE, funcSelect(2.1));
    assertEquals(Keyword.FEW, funcSelect(2.2));
    assertEquals(Keyword.FEW, funcSelect(2.3));
    assertEquals(Keyword.FEW, funcSelect(2.4));
    assertEquals(Keyword.OTHER, funcSelect(2.5));

    assertEquals(Keyword.OTHER, funcSelect(20));
    assertEquals(Keyword.ONE, funcSelect(21));
    assertEquals(Keyword.FEW, funcSelect(22));
    assertEquals(Keyword.FEW, funcSelect(23));
    assertEquals(Keyword.FEW, funcSelect(24));
    assertEquals(Keyword.OTHER, funcSelect(25));
  },

  testSimpleSelectSrNative() {
    if (undefined === Intl.PluralRules) return;

    propertyReplacer.replace(goog, 'LOCALE', 'sr-Latn');
    propertyReplacer.replace(
        LocaleFeature, 'USE_ECMASCRIPT_I18N_PLURALRULES', true);

    /** @suppress {visibility} suppression added to enable type checking */
    const funcSelect = pluralRules.mapToNativeSelect_();
    assert(funcSelect != null);

    assertEquals(Keyword.ONE, funcSelect(1));
    assertEquals(Keyword.ONE, funcSelect(31));
    assertEquals(Keyword.ONE, funcSelect(0.1));
    assertEquals(Keyword.ONE, funcSelect(1.1));
    assertEquals(Keyword.ONE, funcSelect(2.1));

    assertEquals(Keyword.FEW, funcSelect(2));

    assertEquals(Keyword.FEW, funcSelect(3));
    assertEquals(Keyword.FEW, funcSelect(33));
    assertEquals(Keyword.FEW, funcSelect(0.2));
    assertEquals(Keyword.FEW, funcSelect(0.3));
    assertEquals(Keyword.FEW, funcSelect(0.4));
    assertEquals(Keyword.FEW, funcSelect(2.2));

    assertEquals(Keyword.OTHER, funcSelect(2.11));
    assertEquals(Keyword.OTHER, funcSelect(2.12));
    assertEquals(Keyword.OTHER, funcSelect(2.13));
    assertEquals(Keyword.OTHER, funcSelect(2.14));
    assertEquals(Keyword.OTHER, funcSelect(2.15));

    assertEquals(Keyword.OTHER, funcSelect(0));
    assertEquals(Keyword.OTHER, funcSelect(5));
    assertEquals(Keyword.OTHER, funcSelect(10));
    assertEquals(Keyword.OTHER, funcSelect(35));
    assertEquals(Keyword.OTHER, funcSelect(37));
    assertEquals(Keyword.OTHER, funcSelect(40));
    assertEquals(Keyword.OTHER, funcSelect(0.0, 1));
    assertEquals(Keyword.OTHER, funcSelect(0.5));
    assertEquals(Keyword.OTHER, funcSelect(0.6));

    assertEquals(Keyword.FEW, funcSelect(2));
    assertEquals(Keyword.ONE, funcSelect(2.1));
    assertEquals(Keyword.FEW, funcSelect(2.2));
    assertEquals(Keyword.FEW, funcSelect(2.3));
    assertEquals(Keyword.FEW, funcSelect(2.4));
    assertEquals(Keyword.OTHER, funcSelect(2.5));

    assertEquals(Keyword.OTHER, funcSelect(20));
    assertEquals(Keyword.ONE, funcSelect(21));
    assertEquals(Keyword.FEW, funcSelect(22));
    assertEquals(Keyword.FEW, funcSelect(23));
    assertEquals(Keyword.FEW, funcSelect(24));
    assertEquals(Keyword.OTHER, funcSelect(25));
    assertEquals(Keyword.ONE, funcSelect(71));
  },

  // Arabic - starting tests
  testSimpleSelectArabic() {
    /** @suppress {visibility} suppression added to enable type checking */
    const funcSelect = pluralRules.arSelect_;  // Arabic

    assertEquals(Keyword.ZERO, funcSelect(0));
    assertEquals(Keyword.ONE, funcSelect(1));
    assertEquals(Keyword.ONE, funcSelect(1.0, 1));
    assertEquals(Keyword.ONE, funcSelect(1.0, 2));
    assertEquals(Keyword.ONE, funcSelect(1.0, 3));
    assertEquals(Keyword.ONE, funcSelect(1.0, 4));
    assertEquals(Keyword.TWO, funcSelect(2));
    assertEquals(Keyword.FEW, funcSelect(7));
    assertEquals(Keyword.FEW, funcSelect(107));
    assertEquals(Keyword.FEW, funcSelect(907));
    assertEquals(Keyword.MANY, funcSelect(84));
    assertEquals(Keyword.MANY, funcSelect(111));
    assertEquals(Keyword.OTHER, funcSelect(0.1));
    assertEquals(Keyword.OTHER, funcSelect(99.01));

    assertEquals(Keyword.TWO, funcSelect(2.0, 1));
    assertEquals(Keyword.TWO, funcSelect(2.00, 2));

    assertEquals(Keyword.OTHER, funcSelect(1.1));
    assertEquals(Keyword.OTHER, funcSelect(1.1));
    assertEquals(Keyword.OTHER, funcSelect(2.1));

    assertEquals(Keyword.FEW, funcSelect(3));
    assertEquals(Keyword.FEW, funcSelect(7));
    assertEquals(Keyword.FEW, funcSelect(10));
    assertEquals(Keyword.MANY, funcSelect(11));
    assertEquals(Keyword.MANY, funcSelect(12));
    assertEquals(Keyword.MANY, funcSelect(26));
    assertEquals(Keyword.MANY, funcSelect(27));
    assertEquals(Keyword.MANY, funcSelect(40));

    assertEquals(Keyword.OTHER, funcSelect(0.4));

    assertEquals(Keyword.MANY, funcSelect(33));
    assertEquals(Keyword.MANY, funcSelect(99));
    assertEquals(Keyword.OTHER, funcSelect(100));
    assertEquals(Keyword.OTHER, funcSelect(101));
    assertEquals(Keyword.OTHER, funcSelect(102));
    assertEquals(Keyword.FEW, funcSelect(103));
    assertEquals(Keyword.MANY, funcSelect(111));
    assertEquals(Keyword.FEW, funcSelect(1003));

    assertEquals(Keyword.OTHER, funcSelect(99.1));
  },

  testSimpleSelectArNative() {
    if (undefined === Intl.PluralRules) return;

    propertyReplacer.replace(goog, 'LOCALE', 'ar');
    propertyReplacer.replace(
        LocaleFeature, 'USE_ECMASCRIPT_I18N_PLURALRULES', true);

    /** @suppress {visibility} suppression added to enable type checking */
    const funcSelect = pluralRules.mapToNativeSelect_();
    assert(funcSelect != null);

    assertEquals(Keyword.ZERO, funcSelect(0));
    assertEquals(Keyword.ONE, funcSelect(1));
    assertEquals(Keyword.ONE, funcSelect(1.0, 1));
    assertEquals(Keyword.ONE, funcSelect(1.0, 2));
    assertEquals(Keyword.ONE, funcSelect(1.0, 3));
    assertEquals(Keyword.ONE, funcSelect(1.0, 4));
    assertEquals(Keyword.TWO, funcSelect(2));
    assertEquals(Keyword.FEW, funcSelect(7));
    assertEquals(Keyword.FEW, funcSelect(107));
    assertEquals(Keyword.FEW, funcSelect(907));
    assertEquals(Keyword.MANY, funcSelect(84));
    assertEquals(Keyword.MANY, funcSelect(111));
    assertEquals(Keyword.OTHER, funcSelect(0.1));
    assertEquals(Keyword.OTHER, funcSelect(99.01));

    assertEquals(Keyword.TWO, funcSelect(2.0, 1));
    assertEquals(Keyword.TWO, funcSelect(2.00, 2));

    assertEquals(Keyword.OTHER, funcSelect(1.1));
    assertEquals(Keyword.OTHER, funcSelect(1.1));
    assertEquals(Keyword.OTHER, funcSelect(2.1));

    assertEquals(Keyword.FEW, funcSelect(3));
    assertEquals(Keyword.FEW, funcSelect(7));
    assertEquals(Keyword.FEW, funcSelect(10));
    assertEquals(Keyword.MANY, funcSelect(11));
    assertEquals(Keyword.MANY, funcSelect(12));
    assertEquals(Keyword.MANY, funcSelect(26));
    assertEquals(Keyword.MANY, funcSelect(27));
    assertEquals(Keyword.MANY, funcSelect(40));

    assertEquals(Keyword.OTHER, funcSelect(0.4));

    assertEquals(Keyword.MANY, funcSelect(33));
    assertEquals(Keyword.MANY, funcSelect(99));
    assertEquals(Keyword.OTHER, funcSelect(100));
    assertEquals(Keyword.OTHER, funcSelect(101));
    assertEquals(Keyword.OTHER, funcSelect(102));
    assertEquals(Keyword.FEW, funcSelect(103));
    assertEquals(Keyword.MANY, funcSelect(111));
    assertEquals(Keyword.FEW, funcSelect(1003));

    assertEquals(Keyword.OTHER, funcSelect(99.1));
  },

  testSimpleSelectArNativeUnderscore() {
    // Check that underscore in locale is replaced properly.
    if (undefined === Intl.PluralRules) return;

    propertyReplacer.replace(goog, 'LOCALE', 'ar_DZ');
    propertyReplacer.replace(
        LocaleFeature, 'USE_ECMASCRIPT_I18N_PLURALRULES', true);

    /** @suppress {visibility} suppression added to enable type checking */
    const funcSelect = pluralRules.mapToNativeSelect_();
    assert(funcSelect != null);

    assertEquals(Keyword.ZERO, funcSelect(0));
    assertEquals(Keyword.ONE, funcSelect(1));
    assertEquals(Keyword.ONE, funcSelect(1.0, 1));
    assertEquals(Keyword.ONE, funcSelect(1.0, 2));
    assertEquals(Keyword.TWO, funcSelect(2));

    assertEquals(Keyword.FEW, funcSelect(10));
    assertEquals(Keyword.MANY, funcSelect(11));

    assertEquals(Keyword.OTHER, funcSelect(99.1));
  },

  testSimpleSelectFil() {
    /** @suppress {visibility} suppression added to enable type checking */
    const funcSelect = pluralRules.filSelect_;  // Filipino

    assertEquals(Keyword.ONE, funcSelect(0));
    assertEquals(Keyword.ONE, funcSelect(1));
    assertEquals(Keyword.ONE, funcSelect(3));
    assertEquals(Keyword.ONE, funcSelect(15));
    assertEquals(Keyword.ONE, funcSelect(21));
    assertEquals(Keyword.ONE, funcSelect(101));

    assertEquals(Keyword.OTHER, funcSelect(9));
    assertEquals(Keyword.OTHER, funcSelect(16));
  },

  testSimpleSelectFilNative() {
    if (undefined === Intl.PluralRules) return;

    propertyReplacer.replace(
        LocaleFeature, 'USE_ECMASCRIPT_I18N_PLURALRULES', true);
    propertyReplacer.replace(goog, 'LOCALE', 'fil');

    /** @suppress {visibility} suppression added to enable type checking */
    const funcSelect = pluralRules.mapToNativeSelect_();
    assert(funcSelect != null);

    assertEquals(Keyword.ONE, funcSelect(0));
    assertEquals(Keyword.ONE, funcSelect(1));
    assertEquals(Keyword.ONE, funcSelect(3));
    assertEquals(Keyword.ONE, funcSelect(15));
    assertEquals(Keyword.ONE, funcSelect(21));
    assertEquals(Keyword.ONE, funcSelect(101));

    assertEquals(Keyword.OTHER, funcSelect(9));
    assertEquals(Keyword.OTHER, funcSelect(16));
  },


  testSimpleSelectIs() {
    /** @suppress {visibility} suppression added to enable type checking */
    const funcSelect = pluralRules.isSelect_;  // Icelandic

    assertEquals(Keyword.ONE, funcSelect(1));
    assertEquals(Keyword.ONE, funcSelect(21));
    assertEquals(Keyword.ONE, funcSelect(31));
    assertEquals(Keyword.ONE, funcSelect(591));
    assertEquals(Keyword.ONE, funcSelect(101));

    assertEquals(Keyword.OTHER, funcSelect(0));
    assertEquals(Keyword.OTHER, funcSelect(2));
    assertEquals(Keyword.OTHER, funcSelect(9));
    assertEquals(Keyword.OTHER, funcSelect(11));
    assertEquals(Keyword.OTHER, funcSelect(2011));
  },
});
