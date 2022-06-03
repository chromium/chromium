/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.i18n.ordinalRulesTest');
goog.setTestOnly();

const LocaleFeature = goog.require('goog.i18n.LocaleFeature');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const ordinalRules = goog.require('goog.i18n.ordinalRules');
const testSuite = goog.require('goog.testing.testSuite');

let propertyReplacer;

/** @suppress {missingRequire} */
const Keyword = ordinalRules.Keyword;

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
    return 'OrdinalRules Tests';
  },

  testSimpleSelectEn() {
    /** @suppress {visibility} suppression added to enable type checking */
    const funcSelect = ordinalRules.enSelect_;

    assertEquals(Keyword.OTHER, funcSelect(0));   // 0th
    assertEquals(Keyword.ONE, funcSelect(1));     // 1st dollar
    assertEquals(Keyword.ONE, funcSelect(71));    // 71st dollar
    assertEquals(Keyword.ONE, funcSelect(2381));  // 2381st dollar
    assertEquals(Keyword.TWO, funcSelect(2));     // 2nd dollar
    assertEquals(Keyword.TWO, funcSelect(22));    // 22nd dollar
    assertEquals(Keyword.FEW, funcSelect(3));     // 3rd dollar
    assertEquals(Keyword.FEW, funcSelect(1003));  // 1003rd dollar
    assertEquals(Keyword.OTHER, funcSelect(4));   // 4th dollar
    assertEquals(Keyword.OTHER, funcSelect(12));  // 12th dollar
    assertEquals(Keyword.OTHER, funcSelect(25));  // 25 dollar
  },

  testSimpleSelectEnNative() {
    // Ignore test when Intl class doesn't implement native mode.
    if (undefined === Intl.PluralRules) return;

    propertyReplacer.replace(goog, 'LOCALE', 'en');
    propertyReplacer.replace(
        LocaleFeature, 'USE_ECMASCRIPT_I18N_PLURALRULES', true);


    /** @suppress {visibility} suppression added to enable type checking */
    const funcSelect = ordinalRules.mapToNativeSelect_();
    assert(funcSelect !== null);

    assertEquals(Keyword.OTHER, funcSelect(0));   // 0th
    assertEquals(Keyword.ONE, funcSelect(1));     // 1st dollar
    assertEquals(Keyword.ONE, funcSelect(71));    // 71st dollar
    assertEquals(Keyword.ONE, funcSelect(2381));  // 2381st dollar
    assertEquals(Keyword.TWO, funcSelect(2));     // 2nd dollar
    assertEquals(Keyword.TWO, funcSelect(22));    // 22nd dollar
    assertEquals(Keyword.FEW, funcSelect(3));     // 3rd dollar
    assertEquals(Keyword.FEW, funcSelect(1003));  // 1003rd dollar
    assertEquals(Keyword.OTHER, funcSelect(4));   // 4th dollar
    assertEquals(Keyword.OTHER, funcSelect(12));  // 12th dollar
    assertEquals(Keyword.OTHER, funcSelect(25));  // 25 dollar
  },

  testSimpleSelectEnNativeUnderscore() {
    // Check that underscore in locale is replaced properly.
    if (undefined === Intl.PluralRules) return;

    propertyReplacer.replace(goog, 'LOCALE', 'en_IN');
    propertyReplacer.replace(
        LocaleFeature, 'USE_ECMASCRIPT_I18N_PLURALRULES', true);

    /** @suppress {visibility} suppression added to enable type checking */
    const funcSelect = ordinalRules.mapToNativeSelect_();
    assert(funcSelect !== null);

    assertEquals(Keyword.OTHER, funcSelect(0));   // 0th
    assertEquals(Keyword.ONE, funcSelect(1));     // 1st dollar
    assertEquals(Keyword.ONE, funcSelect(71));    // 71st dollar
    assertEquals(Keyword.ONE, funcSelect(2381));  // 2381st dollar
    assertEquals(Keyword.TWO, funcSelect(2));     // 2nd dollar
    assertEquals(Keyword.TWO, funcSelect(22));    // 22nd dollar
    assertEquals(Keyword.FEW, funcSelect(3));     // 3rd dollar
    assertEquals(Keyword.FEW, funcSelect(1003));  // 1003rd dollar
    assertEquals(Keyword.OTHER, funcSelect(4));   // 4th dollar
    assertEquals(Keyword.OTHER, funcSelect(12));  // 12th dollar
    assertEquals(Keyword.OTHER, funcSelect(25));  // 25 dollar
  },

  tesStimpleSelectCy() {
    /** @suppress {visibility} suppression added to enable type checking */
    const funcSelect = ordinalRules.cySelect_;

    assertEquals(Keyword.ZERO, funcSelect(0));
    assertEquals(Keyword.ZERO, funcSelect(7));
    assertEquals(Keyword.ZERO, funcSelect(8));
    assertEquals(Keyword.ZERO, funcSelect(9));
    assertEquals(Keyword.ONE, funcSelect(1));
    assertEquals(Keyword.TWO, funcSelect(2));
    assertEquals(Keyword.FEW, funcSelect(3));
    assertEquals(Keyword.FEW, funcSelect(4));
    assertEquals(Keyword.MANY, funcSelect(5));
    assertEquals(Keyword.MANY, funcSelect(6));
    assertEquals(Keyword.OTHER, funcSelect(10));
  },

  testSimpleSelectCyNative() {
    if (undefined === Intl.PluralRules) return;

    propertyReplacer.replace(
        LocaleFeature, 'USE_ECMASCRIPT_I18N_PLURALRULES', true);

    propertyReplacer.replace(goog, 'LOCALE', 'cy');

    /** @suppress {visibility} suppression added to enable type checking */
    const funcSelect = ordinalRules.mapToNativeSelect_();
    assert(funcSelect !== null);

    assertEquals(Keyword.ZERO, funcSelect(0));
    assertEquals(Keyword.ZERO, funcSelect(7));
    assertEquals(Keyword.ZERO, funcSelect(8));
    assertEquals(Keyword.ZERO, funcSelect(9));
    assertEquals(Keyword.ONE, funcSelect(1));
    assertEquals(Keyword.TWO, funcSelect(2));
    assertEquals(Keyword.FEW, funcSelect(3));
    assertEquals(Keyword.FEW, funcSelect(4));
    assertEquals(Keyword.MANY, funcSelect(5));
    assertEquals(Keyword.MANY, funcSelect(6));
    assertEquals(Keyword.OTHER, funcSelect(10));
  },

  testSimpleSelectNe() {
    /** @suppress {visibility} suppression added to enable type checking */
    const funcSelect = ordinalRules.neSelect_;  // Nepali

    assertEquals(Keyword.ONE, funcSelect(1));
    assertEquals(Keyword.ONE, funcSelect(2));
    assertEquals(Keyword.ONE, funcSelect(3));
    assertEquals(Keyword.ONE, funcSelect(4));

    assertEquals(Keyword.OTHER, funcSelect(0));
    assertEquals(Keyword.OTHER, funcSelect(5));
    assertEquals(Keyword.OTHER, funcSelect(10));
    assertEquals(Keyword.OTHER, funcSelect(35));
  },

  testSimpleSelectNeNative() {
    if (undefined === Intl.PluralRules) return;

    propertyReplacer.replace(goog, 'LOCALE', 'ne');
    propertyReplacer.replace(
        LocaleFeature, 'USE_ECMASCRIPT_I18N_PLURALRULES', true);

    /** @suppress {visibility} suppression added to enable type checking */
    const funcSelect = ordinalRules.mapToNativeSelect_();
    assert(funcSelect !== null);

    assertEquals(Keyword.ONE, funcSelect(1));
    assertEquals(Keyword.ONE, funcSelect(2));
    assertEquals(Keyword.ONE, funcSelect(3));
    assertEquals(Keyword.ONE, funcSelect(4));

    assertEquals(Keyword.OTHER, funcSelect(0));
    assertEquals(Keyword.OTHER, funcSelect(5));
    assertEquals(Keyword.OTHER, funcSelect(10));
    assertEquals(Keyword.OTHER, funcSelect(35));
  },
});
