/**
 * @fileoverview
 * @suppress {missingRequire} Swapping implementation using namespace
 */
/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.labs.i18n.ListFormatTest');
goog.setTestOnly();

const GenderInfo = goog.require('goog.labs.i18n.GenderInfo');
const ListFormat = goog.require('goog.labs.i18n.ListFormat');
/** @suppress {extraRequire} */
const ListFormatSymbols = goog.require('goog.labs.i18n.ListFormatSymbols');
const ListFormatSymbols_el = goog.require('goog.labs.i18n.ListFormatSymbols_el');
const ListFormatSymbols_en = goog.require('goog.labs.i18n.ListFormatSymbols_en');
const ListFormatSymbols_fr = goog.require('goog.labs.i18n.ListFormatSymbols_fr');
const ListFormatSymbols_ml = goog.require('goog.labs.i18n.ListFormatSymbols_ml');
const ListFormatSymbols_zu = goog.require('goog.labs.i18n.ListFormatSymbols_zu');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  /** @suppress {const} See go/const-js-library-faq */
  setUp() {
    // Always switch back to English on before the next test.
    goog.labs.i18n.ListFormatSymbols = ListFormatSymbols_en;
  },

  testListFormatterArrayDirect() {
    const fmt = new ListFormat();
    assertEquals('One', fmt.format(['One']));
    assertEquals('One and Two', fmt.format(['One', 'Two']));
    assertEquals('One, Two, and Three', fmt.format(['One', 'Two', 'Three']));
    assertEquals(
        'One, Two, Three, Four, Five, and Six',
        fmt.format(['One', 'Two', 'Three', 'Four', 'Five', 'Six']));
  },

  testListFormatterArrayIndirect() {
    const fmt = new ListFormat();

    const items = [];

    items.push('One');
    assertEquals('One', fmt.format(items));

    items.push('Two');
    assertEquals('One and Two', fmt.format(items));
    items.push('Three');
    assertEquals('One, Two, and Three', fmt.format(items));

    items.push('Four');
    items.push('Five');
    items.push('Six');
    assertEquals('One, Two, Three, Four, Five, and Six', fmt.format(items));
  },

  testListFormatterFrench() {
    /**
     * @suppress {constantProperty} suppression added to enable type checking
     */
    goog.labs.i18n.ListFormatSymbols = ListFormatSymbols_fr;

    const fmt = new ListFormat();
    assertEquals('One', fmt.format(['One']));
    assertEquals('One et Two', fmt.format(['One', 'Two']));
    assertEquals('One, Two et Three', fmt.format(['One', 'Two', 'Three']));
    assertEquals(
        'One, Two, Three, Four, Five et Six',
        fmt.format(['One', 'Two', 'Three', 'Four', 'Five', 'Six']));

    /**
     * @suppress {constantProperty} suppression added to enable type checking
     */
    goog.labs.i18n.ListFormatSymbols = ListFormatSymbols_en;
  },

  // Malayalam and Zulu are the only two locales with pathers
  // different than '{0} sometext {1}'
  testListFormatterSpecialLanguages() {
    /**
     * @suppress {constantProperty} suppression added to enable type checking
     */
    goog.labs.i18n.ListFormatSymbols = ListFormatSymbols_ml;
    const fmt_ml = new ListFormat();
    /**
     * @suppress {constantProperty} suppression added to enable type checking
     */
    goog.labs.i18n.ListFormatSymbols = ListFormatSymbols_zu;
    const fmt_zu = new ListFormat();
    /**
     * @suppress {constantProperty} suppression added to enable type checking
     */
    goog.labs.i18n.ListFormatSymbols = ListFormatSymbols_en;

    // Only the end pattern is special with Malayalam
    // Escaped for safety, the string is 'One, Two, Three എന്നിവ'
    assertEquals(
        'One, Two, Three \u0D0E\u0D28\u0D4D\u0D28\u0D3F\u0D35',
        fmt_ml.format(['One', 'Two', 'Three']));

    // Only the two items pattern is special with Zulu
    assertEquals('One ne-Two', fmt_zu.format(['One', 'Two']));
  },

  testVariousObjectTypes() {
    const fmt = new ListFormat();
    const booleanObject = new Boolean(1);
    const arrayObject = ['black', 'white'];
    // Not sure how "flaky" this is. Firefox and Chrome give the same results,
    // but I am not sure if the JavaScript standard specifies exactly what
    // Array toString does, for instance.
    assertEquals(
        'One, black,white, 42, true, and Five',
        fmt.format(['One', arrayObject, 42, booleanObject, 'Five']));
  },

  testListGendersNeutral() {
    const Gender = GenderInfo.Gender;

    /**
     * @suppress {constantProperty} suppression added to enable type checking
     */
    goog.labs.i18n.ListFormatSymbols = ListFormatSymbols_en;
    const listGen = new GenderInfo();

    assertEquals(Gender.MALE, listGen.getListGender([Gender.MALE]));
    assertEquals(Gender.FEMALE, listGen.getListGender([Gender.FEMALE]));
    assertEquals(Gender.OTHER, listGen.getListGender([Gender.OTHER]));

    assertEquals(
        Gender.OTHER, listGen.getListGender([Gender.MALE, Gender.MALE]));
    assertEquals(
        Gender.OTHER, listGen.getListGender([Gender.FEMALE, Gender.FEMALE]));
    assertEquals(
        Gender.OTHER, listGen.getListGender([Gender.OTHER, Gender.OTHER]));

    assertEquals(
        Gender.OTHER, listGen.getListGender([Gender.MALE, Gender.OTHER]));
    assertEquals(
        Gender.OTHER, listGen.getListGender([Gender.OTHER, Gender.MALE]));
    assertEquals(
        Gender.OTHER, listGen.getListGender([Gender.MALE, Gender.FEMALE]));
    assertEquals(
        Gender.OTHER, listGen.getListGender([Gender.FEMALE, Gender.MALE]));
    assertEquals(
        Gender.OTHER, listGen.getListGender([Gender.OTHER, Gender.FEMALE]));
    assertEquals(
        Gender.OTHER, listGen.getListGender([Gender.FEMALE, Gender.OTHER]));

    assertEquals(
        Gender.OTHER,
        listGen.getListGender([Gender.MALE, Gender.FEMALE, Gender.OTHER]));
    assertEquals(
        Gender.OTHER,
        listGen.getListGender([Gender.MALE, Gender.OTHER, Gender.FEMALE]));
    assertEquals(
        Gender.OTHER,
        listGen.getListGender([Gender.FEMALE, Gender.MALE, Gender.OTHER]));
    assertEquals(
        Gender.OTHER,
        listGen.getListGender([Gender.FEMALE, Gender.OTHER, Gender.MALE]));
    assertEquals(
        Gender.OTHER,
        listGen.getListGender([Gender.OTHER, Gender.MALE, Gender.FEMALE]));
    assertEquals(
        Gender.OTHER,
        listGen.getListGender([Gender.OTHER, Gender.FEMALE, Gender.MALE]));
  },

  testListGendersMaleTaints() {
    const Gender = GenderInfo.Gender;

    /**
     * @suppress {constantProperty} suppression added to enable type checking
     */
    goog.labs.i18n.ListFormatSymbols = ListFormatSymbols_fr;
    const listGen = new GenderInfo();
    /**
     * @suppress {constantProperty} suppression added to enable type checking
     */
    goog.labs.i18n.ListFormatSymbols = ListFormatSymbols_en;

    assertEquals(Gender.MALE, listGen.getListGender([Gender.MALE]));
    assertEquals(Gender.FEMALE, listGen.getListGender([Gender.FEMALE]));
    assertEquals(Gender.OTHER, listGen.getListGender([Gender.OTHER]));

    assertEquals(
        Gender.MALE, listGen.getListGender([Gender.MALE, Gender.MALE]));
    assertEquals(
        Gender.FEMALE, listGen.getListGender([Gender.FEMALE, Gender.FEMALE]));
    assertEquals(
        Gender.MALE, listGen.getListGender([Gender.OTHER, Gender.OTHER]));

    assertEquals(
        Gender.MALE, listGen.getListGender([Gender.MALE, Gender.OTHER]));
    assertEquals(
        Gender.MALE, listGen.getListGender([Gender.OTHER, Gender.MALE]));
    assertEquals(
        Gender.MALE, listGen.getListGender([Gender.MALE, Gender.FEMALE]));
    assertEquals(
        Gender.MALE, listGen.getListGender([Gender.FEMALE, Gender.MALE]));
    assertEquals(
        Gender.MALE, listGen.getListGender([Gender.OTHER, Gender.FEMALE]));
    assertEquals(
        Gender.MALE, listGen.getListGender([Gender.FEMALE, Gender.OTHER]));

    assertEquals(
        Gender.MALE,
        listGen.getListGender([Gender.MALE, Gender.FEMALE, Gender.OTHER]));
    assertEquals(
        Gender.MALE,
        listGen.getListGender([Gender.MALE, Gender.OTHER, Gender.FEMALE]));
    assertEquals(
        Gender.MALE,
        listGen.getListGender([Gender.FEMALE, Gender.MALE, Gender.OTHER]));
    assertEquals(
        Gender.MALE,
        listGen.getListGender([Gender.FEMALE, Gender.OTHER, Gender.MALE]));
    assertEquals(
        Gender.MALE,
        listGen.getListGender([Gender.OTHER, Gender.MALE, Gender.FEMALE]));
    assertEquals(
        Gender.MALE,
        listGen.getListGender([Gender.OTHER, Gender.FEMALE, Gender.MALE]));
  },

  testListGendersMixedNeutral() {
    const Gender = GenderInfo.Gender;

    /**
     * @suppress {constantProperty} suppression added to enable type checking
     */
    goog.labs.i18n.ListFormatSymbols = ListFormatSymbols_el;
    const listGen = new GenderInfo();
    /**
     * @suppress {constantProperty} suppression added to enable type checking
     */
    goog.labs.i18n.ListFormatSymbols = ListFormatSymbols_en;

    assertEquals(Gender.MALE, listGen.getListGender([Gender.MALE]));
    assertEquals(Gender.FEMALE, listGen.getListGender([Gender.FEMALE]));
    assertEquals(Gender.OTHER, listGen.getListGender([Gender.OTHER]));

    assertEquals(
        Gender.MALE, listGen.getListGender([Gender.MALE, Gender.MALE]));
    assertEquals(
        Gender.FEMALE, listGen.getListGender([Gender.FEMALE, Gender.FEMALE]));
    assertEquals(
        Gender.OTHER, listGen.getListGender([Gender.OTHER, Gender.OTHER]));

    assertEquals(
        Gender.OTHER, listGen.getListGender([Gender.MALE, Gender.OTHER]));
    assertEquals(
        Gender.OTHER, listGen.getListGender([Gender.OTHER, Gender.MALE]));
    assertEquals(
        Gender.OTHER, listGen.getListGender([Gender.MALE, Gender.FEMALE]));
    assertEquals(
        Gender.OTHER, listGen.getListGender([Gender.FEMALE, Gender.MALE]));
    assertEquals(
        Gender.OTHER, listGen.getListGender([Gender.OTHER, Gender.FEMALE]));
    assertEquals(
        Gender.OTHER, listGen.getListGender([Gender.FEMALE, Gender.OTHER]));

    assertEquals(
        Gender.OTHER,
        listGen.getListGender([Gender.MALE, Gender.FEMALE, Gender.OTHER]));
    assertEquals(
        Gender.OTHER,
        listGen.getListGender([Gender.MALE, Gender.OTHER, Gender.FEMALE]));
    assertEquals(
        Gender.OTHER,
        listGen.getListGender([Gender.FEMALE, Gender.MALE, Gender.OTHER]));
    assertEquals(
        Gender.OTHER,
        listGen.getListGender([Gender.FEMALE, Gender.OTHER, Gender.MALE]));
    assertEquals(
        Gender.OTHER,
        listGen.getListGender([Gender.OTHER, Gender.MALE, Gender.FEMALE]));
    assertEquals(
        Gender.OTHER,
        listGen.getListGender([Gender.OTHER, Gender.FEMALE, Gender.MALE]));
  },

  testListGendersVariousCallTypes() {
    const Gender = GenderInfo.Gender;

    // Using French because with English the results are mostly Gender.OTHER
    // so we can detect fewer problems
    /**
     * @suppress {constantProperty} suppression added to enable type checking
     */
    goog.labs.i18n.ListFormatSymbols = ListFormatSymbols_fr;
    const listGen = new GenderInfo();
    /**
     * @suppress {constantProperty} suppression added to enable type checking
     */
    goog.labs.i18n.ListFormatSymbols = ListFormatSymbols_en;

    // Anynymous Arrays
    assertEquals(Gender.MALE, listGen.getListGender([Gender.MALE]));
    assertEquals(Gender.FEMALE, listGen.getListGender([Gender.FEMALE]));
    assertEquals(Gender.OTHER, listGen.getListGender([Gender.OTHER]));

    assertEquals(
        Gender.MALE, listGen.getListGender([Gender.MALE, Gender.MALE]));
    assertEquals(
        Gender.FEMALE, listGen.getListGender([Gender.FEMALE, Gender.FEMALE]));
    assertEquals(
        Gender.MALE, listGen.getListGender([Gender.OTHER, Gender.OTHER]));

    assertEquals(
        Gender.MALE, listGen.getListGender([Gender.MALE, Gender.FEMALE]));
    assertEquals(
        Gender.MALE, listGen.getListGender([Gender.MALE, Gender.OTHER]));
    assertEquals(
        Gender.MALE, listGen.getListGender([Gender.FEMALE, Gender.OTHER]));

    // Arrays
    const arrayM = [Gender.MALE];
    const arrayF = [Gender.FEMALE];
    const arrayO = [Gender.OTHER];

    const arrayMM = [Gender.MALE, Gender.MALE];
    const arrayFF = [Gender.FEMALE, Gender.FEMALE];
    const arrayOO = [Gender.OTHER, Gender.OTHER];

    const arrayMF = [Gender.MALE, Gender.FEMALE];
    const arrayMO = [Gender.MALE, Gender.OTHER];
    const arrayFO = [Gender.FEMALE, Gender.OTHER];

    assertEquals(Gender.MALE, listGen.getListGender(arrayM));
    assertEquals(Gender.FEMALE, listGen.getListGender(arrayF));
    assertEquals(Gender.OTHER, listGen.getListGender(arrayO));

    assertEquals(Gender.MALE, listGen.getListGender(arrayMM));
    assertEquals(Gender.FEMALE, listGen.getListGender(arrayFF));
    assertEquals(Gender.MALE, listGen.getListGender(arrayOO));

    assertEquals(Gender.MALE, listGen.getListGender(arrayMF));
    assertEquals(Gender.MALE, listGen.getListGender(arrayMO));
    assertEquals(Gender.MALE, listGen.getListGender(arrayFO));
  },
});
