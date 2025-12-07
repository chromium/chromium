/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview
 * @suppress {missingRequire} Swapping using fully qualified name
 */

goog.module('goog.i18n.DurationFormatTest');
goog.setTestOnly('goog.i18n.DurationFormatTest');

const DurationSymbols = goog.require('goog.i18n.DurationSymbols');
const DurationSymbolsExt = goog.require('goog.i18n.DurationSymbolsExt');
const NumberFormatSymbols_ar_EG = goog.require('goog.i18n.NumberFormatSymbols_ar_EG');
const NumberFormatSymbols_en = goog.require('goog.i18n.NumberFormatSymbols_en');
const testSuite = goog.require('goog.testing.testSuite');
const {DurationFormat, DurationFormatStyle, DurationFormatUnit} = goog.require('goog.i18n.DurationFormat');
const {assertI18nEquals} = goog.require('goog.testing.i18n.asserts');

/** @suppress {visibility} suppression added to enable type checking */
const Plurals_en = goog.i18n.pluralRules.enSelect_;
/** @suppress {visibility} suppression added to enable type checking */
const Plurals_af = goog.i18n.pluralRules.afSelect_;
/** @suppress {visibility} suppression added to enable type checking */
const Plurals_ar = goog.i18n.pluralRules.arSelect_;
/** @suppress {visibility} suppression added to enable type checking */
const Plurals_zh = goog.i18n.pluralRules.defaultSelect_;
/** @suppress {visibility} suppression added to enable type checking */
const Plurals = goog.i18n.pluralRules.defaultSelect_;

/** @unrestricted */
const DurationData = class {
  /**
   * @param {string} locale
   * @param {number} style
   * @param {number} val
   * @param {string} unit
   * @param {string} expected
   * @param {string|undefined} pluralrules
   */
  constructor(locale, style, val, unit, expected, pluralrules) {
    this.locale = locale;
    this.style = style;
    this.val = val;
    this.unit = unit;
    this.expected = expected;
    this.pluralrules = pluralrules;
  }

  /** @return {string} Error description. */
  getErrorDescription() {
    return 'Error for locale:' + this.locale + ' style: ' + this.style +
        ' value: ' + this.val + ' unit: ' + this.unit +
        ' pluralrules:  ' + this.pluralrules + '\'';
  }
};

/** @const {!Object<string, !Object>} */
const localeSymbols = {
  'af': {
    DurationSymbols: DurationSymbols.DurationSymbols_af,
  },
  'en': {
    DurationSymbols: DurationSymbols.DurationSymbols_en,
  },
  'zh_CN': {
    DurationSymbols: DurationSymbols.DurationSymbols_zh_CN,
  },
  'ar': {
    DurationSymbols: DurationSymbols.DurationSymbols_ar,
  },
  'ar_EG': {
    DurationSymbols: DurationSymbols.DurationSymbols_ar_EG,
  },
  'agq': {
    DurationSymbols: DurationSymbolsExt.DurationSymbols_agq,
  },
  'as': {
    DurationSymbols: DurationSymbolsExt.DurationSymbols_as,
  },
};

const durationTestENData = [
  new DurationData(
      'en', DurationFormatStyle.LONG, 1, DurationFormatUnit.YEAR, '1 year',
      Plurals_en(1)),
  new DurationData(
      'en', DurationFormatStyle.LONG, 2, DurationFormatUnit.YEAR, '2 years',
      Plurals_en(2)),
  new DurationData(
      'en', DurationFormatStyle.LONG, 0, DurationFormatUnit.YEAR, '0 years',
      Plurals_en(0)),
  new DurationData(
      'en', DurationFormatStyle.LONG, 1, DurationFormatUnit.DAY, '1 day',
      Plurals_en(1)),
  new DurationData(
      'en', DurationFormatStyle.LONG, 2, DurationFormatUnit.DAY, '2 days',
      Plurals_en(2)),
  new DurationData(
      'en', DurationFormatStyle.LONG, 0, DurationFormatUnit.DAY, '0 days',
      Plurals_en(0)),
  new DurationData(
      'en', DurationFormatStyle.LONG, 1, DurationFormatUnit.MINUTE, '1 minute',
      Plurals_en(1)),
  new DurationData(
      'en', DurationFormatStyle.LONG, 2, DurationFormatUnit.MINUTE, '2 minutes',
      Plurals_en(2)),
  new DurationData(
      'en', DurationFormatStyle.LONG, 0, DurationFormatUnit.MINUTE, '0 minutes',
      Plurals_en(0)),
  new DurationData(
      'en', DurationFormatStyle.SHORT, 1, DurationFormatUnit.YEAR, '1 yr',
      Plurals_en(1)),
  new DurationData(
      'en', DurationFormatStyle.SHORT, 2, DurationFormatUnit.YEAR, '2 yrs',
      Plurals_en(2)),
  new DurationData(
      'en', DurationFormatStyle.SHORT, 0, DurationFormatUnit.YEAR, '0 yrs',
      Plurals_en(0)),
  new DurationData(
      'en', DurationFormatStyle.SHORT, 1, DurationFormatUnit.DAY, '1 day',
      Plurals_en(1)),
  new DurationData(
      'en', DurationFormatStyle.SHORT, 2, DurationFormatUnit.DAY, '2 days',
      Plurals_en(2)),
  new DurationData(
      'en', DurationFormatStyle.SHORT, 0, DurationFormatUnit.DAY, '0 days',
      Plurals_en(0)),
  new DurationData(
      'en', DurationFormatStyle.NARROW, 1, DurationFormatUnit.YEAR, '1y',
      Plurals_en(1)),
  new DurationData(
      'en', DurationFormatStyle.NARROW, 2, DurationFormatUnit.YEAR, '2y',
      Plurals_en(2)),
  new DurationData(
      'en', DurationFormatStyle.NARROW, 0, DurationFormatUnit.YEAR, '0y',
      Plurals_en(0)),
  new DurationData(
      'en', DurationFormatStyle.NARROW, 1, DurationFormatUnit.DAY, '1d',
      Plurals_en(1)),
  new DurationData(
      'en', DurationFormatStyle.NARROW, 2, DurationFormatUnit.DAY, '2d',
      Plurals_en(2)),
  new DurationData(
      'en', DurationFormatStyle.NARROW, 0, DurationFormatUnit.DAY, '0d',
      Plurals_en(0))
];
const durationTestAFData = [
  new DurationData(
      'af', DurationFormatStyle.LONG, 1, DurationFormatUnit.MONTH, '1 maand',
      Plurals_af(1)),
  new DurationData(
      'af', DurationFormatStyle.LONG, 2, DurationFormatUnit.MONTH, '2 maande',
      Plurals_af(2)),
  new DurationData(
      'af', DurationFormatStyle.LONG, 1, DurationFormatUnit.WEEK, '1 week',
      Plurals_af(1)),
  new DurationData(
      'af', DurationFormatStyle.LONG, 2, DurationFormatUnit.WEEK, '2 weke',
      Plurals_af(2)),
  new DurationData(
      'af', DurationFormatStyle.SHORT, 1, DurationFormatUnit.MONTH, '1 md.',
      Plurals_af(1)),
  new DurationData(
      'af', DurationFormatStyle.SHORT, 2, DurationFormatUnit.MONTH, '2 md.',
      Plurals_af(2)),
  new DurationData(
      'af', DurationFormatStyle.SHORT, 1, DurationFormatUnit.WEEK, '1 w.',
      Plurals_af(1)),
  new DurationData(
      'af', DurationFormatStyle.SHORT, 2, DurationFormatUnit.WEEK, '2 w.',
      Plurals_af(2)),
  new DurationData(
      'af', DurationFormatStyle.NARROW, 1, DurationFormatUnit.MONTH, '1 md.',
      Plurals_af(1)),
  new DurationData(
      'af', DurationFormatStyle.NARROW, 2, DurationFormatUnit.MONTH, '2 md.',
      Plurals_af(2)),
  new DurationData(
      'af', DurationFormatStyle.NARROW, 1, DurationFormatUnit.WEEK, '1 w.',
      Plurals_af(1)),
  new DurationData(
      'af', DurationFormatStyle.NARROW, 2, DurationFormatUnit.WEEK, '2 w.',
      Plurals_af(2)),
];
const durationTestZHData = [
  new DurationData(
      'zh_CN', DurationFormatStyle.LONG, 1, DurationFormatUnit.MONTH, '1个月',
      Plurals_zh(1)),
  new DurationData(
      'zh_CN', DurationFormatStyle.LONG, 2, DurationFormatUnit.MONTH, '2个月',
      Plurals_zh(2)),
  new DurationData(
      'zh_CN', DurationFormatStyle.LONG, 1, DurationFormatUnit.WEEK, '1周',
      Plurals_zh(1)),
  new DurationData(
      'zh_CN', DurationFormatStyle.LONG, 2, DurationFormatUnit.WEEK, '2周',
      Plurals_zh(2)),
  new DurationData(
      'zh_CN', DurationFormatStyle.SHORT, 1, DurationFormatUnit.MONTH, '1个月',
      Plurals_zh(1)),
  new DurationData(
      'zh_CN', DurationFormatStyle.SHORT, 2, DurationFormatUnit.MONTH, '2个月',
      Plurals_zh(2)),
  new DurationData(
      'zh_CN', DurationFormatStyle.SHORT, 1, DurationFormatUnit.WEEK, '1周',
      Plurals_zh(1)),
  new DurationData(
      'zh_CN', DurationFormatStyle.SHORT, 2, DurationFormatUnit.WEEK, '2周',
      Plurals_zh(2)),
  new DurationData(
      'zh_CN', DurationFormatStyle.NARROW, 1, DurationFormatUnit.MONTH, '1个月',
      Plurals_zh(1)),
  new DurationData(
      'zh_CN', DurationFormatStyle.NARROW, 2, DurationFormatUnit.MONTH, '2个月',
      Plurals_zh(2)),
  new DurationData(
      'zh_CN', DurationFormatStyle.NARROW, 1, DurationFormatUnit.WEEK, '1周',
      Plurals_zh(1)),
  new DurationData(
      'zh_CN', DurationFormatStyle.NARROW, 2, DurationFormatUnit.WEEK, '2周',
      Plurals_zh(2)),
];
const durationTestArEgData = [
  new DurationData(
      'ar_EG', DurationFormatStyle.LONG, 0, DurationFormatUnit.DAY, '٠ يوم',
      Plurals_ar(0)),
  new DurationData(
      'ar_EG', DurationFormatStyle.SHORT, 0, DurationFormatUnit.DAY, '٠ يوم',
      Plurals_ar(0)),
  new DurationData(
      'ar_EG', DurationFormatStyle.SHORT, 1, DurationFormatUnit.DAY, 'يوم',
      Plurals_ar(1)),
  new DurationData(
      'ar_EG', DurationFormatStyle.SHORT, 2, DurationFormatUnit.DAY, 'يومان',
      Plurals_ar(2)),
  new DurationData(
      'ar_EG', DurationFormatStyle.SHORT, 1, DurationFormatUnit.MONTH, 'شهر',
      Plurals_ar(1)),
  new DurationData(
      'ar_EG', DurationFormatStyle.SHORT, 3, DurationFormatUnit.HOUR, '٣ س',
      Plurals_ar(3)),
  new DurationData(
      'ar_EG', DurationFormatStyle.SHORT, 28, DurationFormatUnit.SECOND, '٢٨ ث',
      Plurals_ar(28)),
  new DurationData(
      'ar_EG', DurationFormatStyle.SHORT, 101, DurationFormatUnit.WEEK,
      '١٠١ أسبوع', Plurals_ar(101)),
  new DurationData(
      'ar_EG', DurationFormatStyle.SHORT, 1.5, DurationFormatUnit.YEAR,
      '١٫٥ سنة', Plurals_ar(1.5))
];
const durationTestEXTData = [
  new DurationData(
      'agq', DurationFormatStyle.SHORT, 2, DurationFormatUnit.DAY, '2 d',
      Plurals(2)),
  new DurationData(
      'agq', DurationFormatStyle.LONG, 1, DurationFormatUnit.DAY, '1 d',
      Plurals(1)),
  new DurationData(
      'agq', DurationFormatStyle.LONG, 3, DurationFormatUnit.DAY, '3 d',
      Plurals(3)),
  new DurationData(
      'agq', DurationFormatStyle.SHORT, 1, DurationFormatUnit.DAY, '1 d',
      Plurals(1)),
  new DurationData(
      'as', DurationFormatStyle.LONG, 1, DurationFormatUnit.DAY, '1 দিন',
      Plurals(1)),
  new DurationData(
      'as', DurationFormatStyle.LONG, 2, DurationFormatUnit.DAY, '2 দিন',
      Plurals(2)),
  new DurationData(
      'as', DurationFormatStyle.SHORT, 1, DurationFormatUnit.MINUTE, '1 মিনিট',
      Plurals(1)),
  new DurationData(
      'as', DurationFormatStyle.SHORT, 2, DurationFormatUnit.MINUTE, '2 মিনিট',
      Plurals(2))
];
testSuite({
  getTestName: function() {
    return 'DurationFormat Tests';
  },

  setUpPage() {},

  setUp: function() {
    // Use computed properties to avoid compiler checks of defines.
    goog['LOCALE'] = 'en';
    /**
     * @suppress {constantProperty} suppression added to enable type checking
     */
    goog.i18n.NumberFormatSymbols = NumberFormatSymbols_en;
    goog.i18n.pluralRules.select = Plurals_en;
  },

  tearDown: function() {
    /**
     * @suppress {constantProperty} suppression added to enable type checking
     */
    goog.i18n.NumberFormatSymbols = NumberFormatSymbols_en;
    // Use computed properties to avoid compiler checks of defines.
    goog['LOCALE'] = 'en';
  },

  // Test with style in en.
  testFormatStyle: function() {
    for (let i = 0; i < durationTestENData.length; i++) {
      const data = durationTestENData[i];
      const symbols = localeSymbols[data.locale];
      // Use computed properties to avoid compiler checks of defines.
      goog['LOCALE'] = data.locale;

      /**
       * @suppress {strictMissingProperties} suppression added to enable type
       * checking
       */
      const fmt =
          new DurationFormat({style: data.style}, symbols.DurationSymbols);
      const durationdata = {};
      durationdata[data.unit] = data.val;
      const result = fmt.format(durationdata);
      assertI18nEquals(data.getErrorDescription(), data.expected, result);
    }
  },

  // Test with multiple units in short style in en.
  testMultiUnitsFormatShortStyle: function() {
    goog['LOCALE'] = 'en';

    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    const fmt = new DurationFormat(
        {style: DurationFormatStyle.SHORT}, DurationSymbols.DurationSymbols_en);
    const durationdata = {hours: 2, seconds: 1};
    const expect = '2 hr 1 sec';
    const result = fmt.format(durationdata);
    assertI18nEquals('', expect, result);
  },

  // Test with multiple units in narrow style in en.
  testMultiUnitsFormatNarrowStyle: function() {
    goog['LOCALE'] = 'en';

    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    const fmt = new DurationFormat(
        {style: DurationFormatStyle.NARROW},
        DurationSymbols.DurationSymbols_en);
    const durationdata = {days: 1, hours: 2, minutes: 1};
    const expect = '1d 2h 1m';
    const result = fmt.format(durationdata);
    assertI18nEquals('', expect, result);
  },

  // Test with multiple units in long style in en.
  testMultiUnitsFormatLongStyle: function() {
    goog['LOCALE'] = 'en';

    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    const fmt = new DurationFormat(
        {style: DurationFormatStyle.LONG}, DurationSymbols.DurationSymbols_en);
    const durationdata = {years: 1, months: 2, days: 1};
    const expect = '1 year 2 months 1 day';
    const result = fmt.format(durationdata);
    assertI18nEquals('', expect, result);
  },

  // Test with style in af.
  testAfrikaansFormatStyle: function() {
    goog['LOCALE'] = 'af';
    for (let i = 0; i < durationTestAFData.length; i++) {
      const data = durationTestAFData[i];
      const symbols = localeSymbols[data.locale];
      /**
       * @suppress {strictMissingProperties} suppression added to enable type
       * checking
       */
      const fmt =
          new DurationFormat({style: data.style}, symbols.DurationSymbols);

      const durationdata = {};
      durationdata[data.unit] = data.val;
      const result = fmt.format(durationdata);
      assertI18nEquals(data.getErrorDescription(), data.expected, result);
    }
  },

  // Test with multiple units in long style in af.
  testAFMultiUnitsFormat: function() {
    goog['LOCALE'] = 'af';

    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    const fmt = new DurationFormat(
        {style: DurationFormatStyle.LONG}, DurationSymbols.DurationSymbols_af);
    const durationdata = {years: 1, months: 2, weeks: 1};
    const expect = '1 jaar 2 maande 1 week';
    const result = fmt.format(durationdata);
    assertI18nEquals('', expect, result);
  },

  // Test with style in zh_CN.
  testZHFormatStyle: function() {
    goog['LOCALE'] = 'zh_CN';
    for (let i = 0; i < durationTestZHData.length; i++) {
      const data = durationTestZHData[i];
      const symbols = localeSymbols[data.locale];
      /**
       * @suppress {strictMissingProperties} suppression added to enable type
       * checking
       */
      const fmt =
          new DurationFormat({style: data.style}, symbols.DurationSymbols);

      const durationdata = {};
      durationdata[data.unit] = data.val;
      const result = fmt.format(durationdata);
      assertI18nEquals(data.getErrorDescription(), data.expected, result);
    }
  },

  // Test with multiple units in long style in zh_CN.
  testZHMultiUnitsFormat: function() {
    goog['LOCALE'] = 'zh_CN';

    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    const fmt = new DurationFormat(
        {style: DurationFormatStyle.LONG},
        DurationSymbols.DurationSymbols_zh_CN);
    const durationdata = {years: 1, months: 2, days: 1};
    const expect = '1年 2个月 1天';
    const result = fmt.format(durationdata);
    assertI18nEquals('', expect, result);
  },

  // Test with style in ar_EG.
  testAREGFormatStyle: function() {
    for (let i = 0; i < durationTestArEgData.length; i++) {
      const data = durationTestArEgData[i];
      const symbols = localeSymbols[data.locale];
      /**
       * @suppress {constantProperty} suppression added to enable type
       * checking
       */
      goog.i18n.NumberFormatSymbols = NumberFormatSymbols_ar_EG;
      // Use computed properties to avoid compiler checks of defines.
      goog['LOCALE'] = data.locale;
      goog.i18n.pluralRules.select = Plurals_ar;
      /**
       * @suppress {strictMissingProperties} suppression added to enable type
       * checking
       */
      const fmt =
          new DurationFormat({style: data.style}, symbols.DurationSymbols);

      const durationdata = {};
      durationdata[data.unit] = data.val;
      const result = fmt.format(durationdata);
      assertI18nEquals(data.getErrorDescription(), data.expected, result);
    }
  },

  // Test with multiple units in long style in af.
  testAREGMultiUnitsFormat: function() {
    goog['LOCALE'] = 'ar_EG';

    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    const fmt = new DurationFormat(
        {style: DurationFormatStyle.LONG},
        DurationSymbols.DurationSymbols_ar_EG);
    const durationdata = {years: 1, months: 2, days: 7};
    const expect = 'سنة2شهر7يوم';
    const result = fmt.format(durationdata);
    assertI18nEquals('', expect, result);
  },

  // Test locales in durationsymbolsext.
  testSymbolEXTFormatStyle: function() {
    for (let i = 0; i < durationTestEXTData.length; i++) {
      const data = durationTestEXTData[i];
      const symbols = localeSymbols[data.locale];

      // Use computed properties to avoid compiler checks of defines.
      goog['LOCALE'] = data.locale;
      /**
       * @suppress {strictMissingProperties} suppression added to enable type
       * checking
       */
      const fmt =
          new DurationFormat({style: data.style}, symbols.DurationSymbols);

      const durationdata = {};
      durationdata[data.unit] = data.val;
      const result = fmt.format(durationdata);
      assertI18nEquals(data.getErrorDescription(), data.expected, result);
    }
  },
});
