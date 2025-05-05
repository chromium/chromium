/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.i18n.DateIntervalFormatTest');
goog.setTestOnly('goog.i18n.DateIntervalFormatTest');

let propertyReplacer;

const DateIntervalFormat = goog.require('goog.i18n.DateIntervalFormat');
const DateRange = goog.require('goog.date.DateRange');
const DateTime = goog.require('goog.date.DateTime');
const DateTimeFormat = goog.require('goog.i18n.DateTimeFormat');
const DateTimeSymbolsType = goog.require('goog.i18n.DateTimeSymbolsType');
const DateTimeSymbols_ar_EG = goog.require('goog.i18n.DateTimeSymbols_ar_EG');
const DateTimeSymbols_en = goog.require('goog.i18n.DateTimeSymbols_en');
const DateTimeSymbols_fr_CA = goog.require('goog.i18n.DateTimeSymbols_fr_CA');
const DateTimeSymbols_gl = goog.require('goog.i18n.DateTimeSymbols_gl');
const DateTimeSymbols_hi = goog.require('goog.i18n.DateTimeSymbols_hi');
const DateTimeSymbols_vi = goog.require('goog.i18n.DateTimeSymbols_vi');
const DateTimeSymbols_zh = goog.require('goog.i18n.DateTimeSymbols_zh');
const GoogDate = goog.require('goog.date.Date');
const Interval = goog.require('goog.date.Interval');
const LocaleFeature = goog.require('goog.i18n.LocaleFeature');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const TimeZone = goog.require('goog.i18n.TimeZone');
const browser = goog.require('goog.labs.userAgent.browser');
const dateIntervalPatterns = goog.require('goog.i18n.dateIntervalPatterns');
const dateIntervalSymbols = goog.require('goog.i18n.dateIntervalSymbols');
const object = goog.require('goog.object');
const testSuite = goog.require('goog.testing.testSuite');
const {addI18nMapping, assertI18nEquals} = goog.require('goog.testing.i18n.asserts');
const {removeWhitespace} = goog.require('goog.testing.i18n.whitespace');

/**
 * List of locales where native ECMAScript mode works.
 * Note that these all work with datetime.Range.
 * @type {!Array<string>} ECMASCRIPT_LISTFORMAT_LOCALES
 */
const nativeLocales = LocaleFeature.ECMASCRIPT_LISTFORMAT_LOCALES;

// Add alternative results for assertI18nEquals.
// These come from ICU72 / CLDR42 updates.
addI18nMapping('10 नवंबर–20', '10 – 20 नवंबर');

// Use same set of locales as ListFormat
let testECMAScriptOptions = [false];
if (!browser.isIE()) {
  if (Intl && Intl.DateTimeFormat &&
      Intl.ListFormat.supportedLocalesOf(['en'])) {
    testECMAScriptOptions.unshift(true);  // Test native before Javascript
  }
}
// For testing with Javascript data.
testECMAScriptOptions.push(false);

/**
 * driveTests sets up each test with local, symbols, and native mode.
 * @param {string} locale
 * @param {!DateTimeSymbolsType} symbols
 * @param {!Function} testCallbackFn
 */
function driveTests(locale, symbols, testCallbackFn) {
  propertyReplacer.replace(goog, 'LOCALE', locale);
  assertTrue(goog.LOCALE == locale);

  const isSupportedNativeLocale = nativeLocales.includes(goog.LOCALE);

  for (let nativeMode of testECMAScriptOptions) {
    if (nativeMode && !isSupportedNativeLocale) {
      continue;
    }
    if (browser.isSafari() && !browser.isAtLeast(browser.Brand.SAFARI, 14)) {
      continue;
    }
    propertyReplacer.replace(
        LocaleFeature, 'USE_ECMASCRIPT_I18N_DATEINTERVALFORMAT', nativeMode);
    testCallbackFn(nativeMode);
  }
}

/** @const {!Object<string, !Object>} */
const localeSymbols = {
  'ar_EG': {
    DateIntervalSymbols: dateIntervalSymbols.DateIntervalSymbols_ar_EG,
    DateTimeSymbols: DateTimeSymbols_ar_EG
  },
  'en': {
    DateIntervalSymbols: dateIntervalSymbols.DateIntervalSymbols_en,
    DateTimeSymbols: DateTimeSymbols_en
  },
  'fr_CA': {
    DateIntervalSymbols: dateIntervalSymbols.DateIntervalSymbols_fr_CA,
    DateTimeSymbols: DateTimeSymbols_fr_CA
  },
  'gl': {
    DateIntervalSymbols: dateIntervalSymbols.DateIntervalSymbols_gl,
    DateTimeSymbols: DateTimeSymbols_gl
  },
  'hi': {
    DateIntervalSymbols: dateIntervalSymbols.DateIntervalSymbols_hi,
    DateTimeSymbols: DateTimeSymbols_hi
  },
  'vi': {
    DateIntervalSymbols: dateIntervalSymbols.DateIntervalSymbols_vi,
    DateTimeSymbols: DateTimeSymbols_vi
  },
  'zh': {
    DateIntervalSymbols: dateIntervalSymbols.DateIntervalSymbols_zh,
    DateTimeSymbols: DateTimeSymbols_zh
  }
};

/** @unrestricted */
const Data = class {
  /**
   * @param {string} locale
   * @param {!Array<number>} firstDate
   * @param {!Array<number>} secondDate
   * @param {number|!dateIntervalSymbols.DateIntervalPatternMap} pattern
   * @param {string} expected
   */
  constructor(locale, firstDate, secondDate, pattern, expected) {
    this.locale = locale;
    this.firstDate = firstDate;
    this.secondDate = secondDate;
    this.pattern = pattern;
    this.expected = expected;
  }

  /**
   * @return {string} Error description.
   */
  getErrorDescription() {
    return 'Error for locale:' + this.locale + ' firstDate:\'' +
        this.firstDate + '\' secondDate:\'' + this.secondDate + '\'';
  }
};

// clang-format off
const formatTestData = [
  new Data('en', [2007, 0, 10, 10, 10, 10],  [2007, 0, 10, 10, 10, 20],  dateIntervalPatterns.DateIntervalPatterns_en.YEAR_FULL, '2007'),
  new Data('en', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 10, 20, 10],  dateIntervalPatterns.DateIntervalPatterns_en.YEAR_FULL, '2007'),
  new Data('en', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 14, 10, 10],  dateIntervalPatterns.DateIntervalPatterns_en.YEAR_FULL, '2007'),
  new Data('en', [2007, 10, 10, 10, 10, 10], [2007, 10, 20, 10, 10, 10], dateIntervalPatterns.DateIntervalPatterns_en.YEAR_FULL, '2007'),
  new Data('en', [2007, 9, 10, 10, 10, 10],  [2007, 10, 10, 10, 10, 10], dateIntervalPatterns.DateIntervalPatterns_en.YEAR_FULL, '2007'),
  new Data('en', [2007, 9, 10, 10, 10, 10],  [2008, 9, 10, 10, 10, 10],  dateIntervalPatterns.DateIntervalPatterns_en.YEAR_FULL, '2007 – 2008'),

  new Data('en', [2007, 0, 10, 10, 10, 10],  [2007, 0, 10, 10, 10, 20],  dateIntervalPatterns.DateIntervalPatterns_en.MONTH_DAY_FULL, 'January 10'),
  new Data('en', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 10, 20, 10],  dateIntervalPatterns.DateIntervalPatterns_en.MONTH_DAY_FULL, 'January 10'),
  new Data('en', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 14, 10, 10],  dateIntervalPatterns.DateIntervalPatterns_en.MONTH_DAY_FULL, 'January 10'),
  new Data('en', [2007, 10, 10, 10, 10, 10], [2007, 10, 20, 10, 10, 10], dateIntervalPatterns.DateIntervalPatterns_en.MONTH_DAY_FULL, 'November 10 – 20'),
  new Data('en', [2007, 9, 10, 10, 10, 10],  [2007, 10, 10, 10, 10, 10], dateIntervalPatterns.DateIntervalPatterns_en.MONTH_DAY_FULL, 'October 10 – November 10'),
  new Data('en', [2007, 9, 10, 10, 10, 10],  [2008, 9, 10, 10, 10, 10],  dateIntervalPatterns.DateIntervalPatterns_en.MONTH_DAY_FULL, 'October 10, 2007 – October 10, 2008'),

  new Data('en', [2007, 0, 10, 10, 10, 10],  [2007, 0, 10, 10, 10, 20],  dateIntervalPatterns.DateIntervalPatterns_en.DAY_ABBR, '10'),
  new Data('en', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 10, 20, 10],  dateIntervalPatterns.DateIntervalPatterns_en.DAY_ABBR, '10'),
  new Data('en', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 14, 10, 10],  dateIntervalPatterns.DateIntervalPatterns_en.DAY_ABBR, '10'),
  new Data('en', [2007, 10, 10, 10, 10, 10], [2007, 10, 20, 10, 10, 10], dateIntervalPatterns.DateIntervalPatterns_en.DAY_ABBR, '10 – 20'),
  new Data('en', [2007, 9, 10, 10, 10, 10],  [2007, 10, 10, 10, 10, 10], dateIntervalPatterns.DateIntervalPatterns_en.DAY_ABBR, '10/10 – 11/10'),
  new Data('en', [2007, 9, 10, 10, 10, 10],  [2008, 9, 10, 10, 10, 10],  dateIntervalPatterns.DateIntervalPatterns_en.DAY_ABBR, '10/10/2007 – 10/10/2008'),

  new Data('en', [2007, 0, 10, 10, 10, 10],  [2007, 0, 10, 10, 10, 20],  dateIntervalPatterns.DateIntervalPatterns_en.WEEKDAY_MONTH_DAY_YEAR_MEDIUM, 'Wed, Jan 10, 2007'),
  new Data('en', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 10, 20, 10],  dateIntervalPatterns.DateIntervalPatterns_en.WEEKDAY_MONTH_DAY_YEAR_MEDIUM, 'Wed, Jan 10, 2007'),
  new Data('en', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 14, 10, 10],  dateIntervalPatterns.DateIntervalPatterns_en.WEEKDAY_MONTH_DAY_YEAR_MEDIUM, 'Wed, Jan 10, 2007'),
  new Data('en', [2007, 10, 10, 10, 10, 10], [2007, 10, 20, 10, 10, 10], dateIntervalPatterns.DateIntervalPatterns_en.WEEKDAY_MONTH_DAY_YEAR_MEDIUM, 'Sat, Nov 10 – Tue, Nov 20, 2007'),
  new Data('en', [2007, 9, 10, 10, 10, 10],  [2007, 10, 10, 10, 10, 10], dateIntervalPatterns.DateIntervalPatterns_en.WEEKDAY_MONTH_DAY_YEAR_MEDIUM, 'Wed, Oct 10 – Sat, Nov 10, 2007'),
  new Data('en', [2007, 9, 10, 10, 10, 10],  [2008, 9, 10, 10, 10, 10],  dateIntervalPatterns.DateIntervalPatterns_en.WEEKDAY_MONTH_DAY_YEAR_MEDIUM, 'Wed, Oct 10, 2007 – Fri, Oct 10, 2008'),

  new Data('en', [2007, 0, 10, 10, 10, 10],  [2007, 0, 10, 10, 10, 20],  DateTimeFormat.Format.SHORT_TIME, '10:10 AM'),
  new Data('en', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 10, 20, 10],  DateTimeFormat.Format.SHORT_TIME, '10:00 – 10:20 AM'),
  new Data('en', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 14, 10, 10],  DateTimeFormat.Format.SHORT_TIME, '10:00 AM – 2:10 PM'),
  new Data('en', [2007, 10, 10, 10, 10, 10], [2007, 10, 20, 10, 10, 10], DateTimeFormat.Format.SHORT_TIME, '11/10/2007, 10:10 AM – 11/20/2007, 10:10 AM'),
  new Data('en', [2007, 9, 10, 10, 10, 10],  [2007, 10, 10, 10, 10, 10], DateTimeFormat.Format.SHORT_TIME, '10/10/2007, 10:10 AM – 11/10/2007, 10:10 AM'),
  new Data('en', [2007, 9, 10, 10, 10, 10],  [2008, 9, 10, 10, 10, 10],  DateTimeFormat.Format.SHORT_TIME, '10/10/2007, 10:10 AM – 10/10/2008, 10:10 AM'),

  new Data('en', [2007, 0, 10, 10, 10, 10],  [2007, 0, 10, 10, 10, 20],  DateTimeFormat.Format.SHORT_DATETIME, '1/10/07, 10:10 AM'),
  new Data('en', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 10, 20, 10],  DateTimeFormat.Format.SHORT_DATETIME, '1/10/07, 10:00 – 10:20 AM'),
  new Data('en', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 14, 10, 10],  DateTimeFormat.Format.SHORT_DATETIME, '1/10/07, 10:00 AM – 2:10 PM'),
  new Data('en', [2007, 10, 10, 10, 10, 10], [2007, 10, 20, 10, 10, 10], DateTimeFormat.Format.SHORT_DATETIME, '11/10/07, 10:10 AM – 11/20/07, 10:10 AM'),
  new Data('en', [2007, 9, 10, 10, 10, 10],  [2007, 10, 10, 10, 10, 10], DateTimeFormat.Format.SHORT_DATETIME, '10/10/07, 10:10 AM – 11/10/07, 10:10 AM'),
  new Data('en', [2007, 9, 10, 10, 10, 10],  [2008, 9, 10, 10, 10, 10],  DateTimeFormat.Format.SHORT_DATETIME, '10/10/07, 10:10 AM – 10/10/08, 10:10 AM'),

  new Data('ar_EG', [2007, 0, 10, 10, 10, 10],  [2007, 0, 10, 10, 10, 20],  dateIntervalPatterns.DateIntervalPatterns_ar_EG.MONTH_DAY_ABBR, '١٠ يناير'),
  new Data('ar_EG', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 10, 20, 10],  dateIntervalPatterns.DateIntervalPatterns_ar_EG.MONTH_DAY_ABBR, '١٠ يناير'),
  new Data('ar_EG', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 14, 10, 10],  dateIntervalPatterns.DateIntervalPatterns_ar_EG.MONTH_DAY_ABBR, '١٠ يناير'),
  new Data('ar_EG', [2007, 10, 10, 10, 10, 10], [2007, 10, 20, 10, 10, 10], dateIntervalPatterns.DateIntervalPatterns_ar_EG.MONTH_DAY_ABBR, '١٠–٢٠ نوفمبر'),
  new Data('ar_EG', [2007, 9, 10, 10, 10, 10],  [2007, 10, 10, 10, 10, 10], dateIntervalPatterns.DateIntervalPatterns_ar_EG.MONTH_DAY_ABBR, '١٠ أكتوبر – ١٠ نوفمبر'),
  new Data('ar_EG', [2007, 9, 10, 10, 10, 10],  [2008, 9, 10, 10, 10, 10],  dateIntervalPatterns.DateIntervalPatterns_ar_EG.MONTH_DAY_ABBR, '١٠ أكتوبر، ٢٠٠٧ – ١٠ أكتوبر، ٢٠٠٨'),

  new Data('vi', [2022, 10, 8, 10, 10, 10],  [2023, 1, 5, 10, 10, 10],  dateIntervalPatterns.DateIntervalPatterns_hi.MONTH_DAY_MEDIUM, '8 thg 11 2022–5 thg 2 2023'),

  new Data('fr_CA', [2007, 0, 10, 10, 10, 10],  [2007, 0, 10, 10, 10, 20],  dateIntervalPatterns.DateIntervalPatterns_fr_CA.MONTH_DAY_MEDIUM, '10 janv.'),
  new Data('fr_CA', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 10, 20, 10],  dateIntervalPatterns.DateIntervalPatterns_fr_CA.MONTH_DAY_MEDIUM, '10 janv.'),
  new Data('fr_CA', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 14, 10, 10],  dateIntervalPatterns.DateIntervalPatterns_fr_CA.MONTH_DAY_MEDIUM, '10 janv.'),
  new Data('fr_CA', [2007, 10, 10, 10, 10, 10], [2007, 10, 20, 10, 10, 10], dateIntervalPatterns.DateIntervalPatterns_fr_CA.MONTH_DAY_MEDIUM, '10 – 20 nov.'),
  new Data('fr_CA', [2007, 9, 10, 10, 10, 10],  [2007, 10, 10, 10, 10, 10], dateIntervalPatterns.DateIntervalPatterns_fr_CA.MONTH_DAY_MEDIUM, '10 oct. – 10 nov.'),
  new Data('fr_CA', [2007, 9, 10, 10, 10, 10],  [2008, 9, 10, 10, 10, 10],  dateIntervalPatterns.DateIntervalPatterns_fr_CA.MONTH_DAY_MEDIUM, '10 oct. 2007 – 10 oct. 2008'),

  new Data('gl', [2007, 0, 10, 10, 10, 10],  [2007, 0, 10, 10, 10, 20],  dateIntervalPatterns.DateIntervalPatterns_gl.YEAR_MONTH_FULL, 'xaneiro de 2007'),
  new Data('gl', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 10, 20, 10],  dateIntervalPatterns.DateIntervalPatterns_gl.YEAR_MONTH_FULL, 'xaneiro de 2007'),
  new Data('gl', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 14, 10, 10],  dateIntervalPatterns.DateIntervalPatterns_gl.YEAR_MONTH_FULL, 'xaneiro de 2007'),
  new Data('gl', [2007, 10, 10, 10, 10, 10], [2007, 10, 20, 10, 10, 10], dateIntervalPatterns.DateIntervalPatterns_gl.YEAR_MONTH_FULL, 'novembro de 2007'),
  new Data('gl', [2007, 9, 10, 10, 10, 10],  [2007, 10, 10, 10, 10, 10], dateIntervalPatterns.DateIntervalPatterns_gl.YEAR_MONTH_FULL, 'outubro–novembro de 2007'),
  new Data('gl', [2007, 9, 10, 10, 10, 10],  [2008, 9, 10, 10, 10, 10],  dateIntervalPatterns.DateIntervalPatterns_gl.YEAR_MONTH_FULL, 'outubro de 2007 – outubro de 2008'),

  new Data('fr_CA', [2007, 0, 10, 10, 10, 10],  [2007, 0, 10, 10, 10, 20],  dateIntervalPatterns.DateIntervalPatterns_fr_CA.YEAR_MONTH_SHORT, '2007-01'),
  new Data('fr_CA', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 10, 20, 10],  dateIntervalPatterns.DateIntervalPatterns_fr_CA.YEAR_MONTH_SHORT, '2007-01'),
  new Data('fr_CA', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 14, 10, 10],  dateIntervalPatterns.DateIntervalPatterns_fr_CA.YEAR_MONTH_SHORT, '2007-01'),
  new Data('fr_CA', [2007, 10, 10, 10, 10, 10], [2007, 10, 20, 10, 10, 10], dateIntervalPatterns.DateIntervalPatterns_fr_CA.YEAR_MONTH_SHORT, '2007-11'),
  new Data('fr_CA', [2007, 9, 10, 10, 10, 10],  [2007, 10, 10, 10, 10, 10], dateIntervalPatterns.DateIntervalPatterns_fr_CA.YEAR_MONTH_SHORT, '2007-10 – 2007-11'),
  new Data('fr_CA', [2007, 9, 10, 10, 10, 10],  [2008, 9, 10, 10, 10, 10],  dateIntervalPatterns.DateIntervalPatterns_fr_CA.YEAR_MONTH_SHORT, '2007-10 – 2008-10'),

  new Data('hi', [2007, 0, 10, 10, 10, 10],  [2007, 0, 10, 10, 10, 20],  dateIntervalPatterns.DateIntervalPatterns_hi.MONTH_DAY_MEDIUM, '10 जन॰'),
  new Data('hi', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 10, 20, 10],  dateIntervalPatterns.DateIntervalPatterns_hi.MONTH_DAY_MEDIUM, '10 जन॰'),
  new Data('hi', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 14, 10, 10],  dateIntervalPatterns.DateIntervalPatterns_hi.MONTH_DAY_MEDIUM, '10 जन॰'),
  new Data('hi', [2007, 10, 10, 10, 10, 10], [2007, 10, 20, 10, 10, 10], dateIntervalPatterns.DateIntervalPatterns_hi.MONTH_DAY_MEDIUM, '10–20 नव॰'),
  new Data('hi', [2007, 9, 10, 10, 10, 10],  [2007, 10, 10, 10, 10, 10], dateIntervalPatterns.DateIntervalPatterns_hi.MONTH_DAY_MEDIUM, '10 अक्तू॰ – 10 नव॰'),
  new Data('hi', [2007, 9, 10, 10, 10, 10],  [2008, 9, 10, 10, 10, 10],  dateIntervalPatterns.DateIntervalPatterns_hi.MONTH_DAY_MEDIUM, '10 अक्तू॰ 2007 – 10 अक्तू॰ 2008'),

  new Data('zh', [2007, 0, 10, 10, 10, 10],  [2007, 0, 10, 10, 10, 20],  dateIntervalPatterns.DateIntervalPatterns_zh.WEEKDAY_MONTH_DAY_YEAR_MEDIUM, '2007年1月10日周三'),
  new Data('zh', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 10, 20, 10],  dateIntervalPatterns.DateIntervalPatterns_zh.WEEKDAY_MONTH_DAY_YEAR_MEDIUM, '2007年1月10日周三'),
  new Data('zh', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 14, 10, 10],  dateIntervalPatterns.DateIntervalPatterns_zh.WEEKDAY_MONTH_DAY_YEAR_MEDIUM, '2007年1月10日周三'),
  new Data('zh', [2007, 10, 10, 10, 10, 10], [2007, 10, 20, 10, 10, 10], dateIntervalPatterns.DateIntervalPatterns_zh.WEEKDAY_MONTH_DAY_YEAR_MEDIUM, '2007年11月10日周六至20日周二'),
  new Data('zh', [2007, 9, 10, 10, 10, 10],  [2007, 10, 10, 10, 10, 10], dateIntervalPatterns.DateIntervalPatterns_zh.WEEKDAY_MONTH_DAY_YEAR_MEDIUM, '2007年10月10日周三至11月10日周六'),
  new Data('zh', [2007, 9, 10, 10, 10, 10],  [2008, 9, 10, 10, 10, 10],  dateIntervalPatterns.DateIntervalPatterns_zh.WEEKDAY_MONTH_DAY_YEAR_MEDIUM, '2007年10月10日周三至2008年10月10日周五')

];
// clang-format on

testSuite({
  setUpPage() {
    propertyReplacer = new PropertyReplacer();
  },

  tearDown() {},

  testFormat: function() {
    driveTests(
        'en', DateTimeSymbols_en,
        /** @param {boolean} nativeMode */
        (nativeMode) => {
          for (let i = 0; i < formatTestData.length; i++) {
            const data = formatTestData[i];
            const symbols = localeSymbols[data.locale];
            const dt1 = new Date(Date.UTC.apply(null, data.firstDate));
            const dt2 = new Date(Date.UTC.apply(null, data.secondDate));
            /**
             * @suppress {strictMissingProperties} suppression added to enable
             * type checking
             */
            const fmt = new DateIntervalFormat(
                data.pattern, symbols.DateIntervalSymbols,
                symbols.DateTimeSymbols);
            const tz = TimeZone.createTimeZone(0);
            // Some browsers use a thin space instead of regular space.
            const result = removeWhitespace(fmt.format(dt1, dt2, tz));
            assertI18nEquals(
                'nativeMode=' + nativeMode + ' version=' +
                    browser.getVersion() + data.getErrorDescription(),
                data.expected, result);
          }
        });
  },

  testRangeFormat() {
    driveTests(
        'en', DateTimeSymbols_en,
        /** @param {boolean} nativeMode */
        (nativeMode) => {
          const dt1 = new GoogDate(2007, 1, 10);
          const dt2 = new GoogDate(2007, 6, 3);
          const dtRng = new DateRange(dt1, dt2);
          const fmt = new DateIntervalFormat(DateTimeFormat.Format.LONG_DATE);
          const result = removeWhitespace(fmt.formatRange(dtRng));
          assertI18nEquals('February 10 – July 3, 2007', result);
        });
  },

  testDateAndIntervalFormat: function() {
    driveTests(
        'en', DateTimeSymbols_en,
        /** @param {boolean} nativeMode */
        (nativeMode) => {
          const dt = new GoogDate(2007, 1, 10);
          const itv = new Interval(0, 4, 23);
          const fmt = new DateIntervalFormat(DateTimeFormat.Format.LONG_DATE);
          const result = removeWhitespace(fmt.format(dt, itv));
          assertI18nEquals(
              'nativeMode=' + nativeMode + ' version=' + browser.getVersion(),
              'February 10 – July 3, 2007', result);
        });
  },

  testNewYearFormat: function() {
    driveTests(
        'en', DateTimeSymbols_en,
        /** @param {boolean} nativeMode */
        (nativeMode) => {
          // Check for crossing the year.
          const dt1 = new Date(Date.UTC(2007, 0, 1, 3, 0, 23));
          const dt2 = new Date(Date.UTC(2007, 0, 1, 3, 40, 23));
          const dt3 = new Date(Date.UTC(2007, 0, 1, 5, 17, 59));
          const fmt =
              new DateIntervalFormat(DateTimeFormat.Format.FULL_DATETIME);
          // Result expected for GMT-3:30 (210 offset west)
          const expected210 = removeWhitespace(
              'Sunday, December 31, 2006 at 11:30:23 PM UTC-3:30 – ' +
              'Monday, January 1, 2007 at 12:10:23 AM UTC-3:30');
          const expected210var2 = removeWhitespace(
              'Sunday, December 31, 2006, 11:30:23 PM UTC-3:30 – ' +
              'Monday, January 1, 2007, 12:10:23 AM UTC-3:30');

          // Note: For native mode, GMT-3:30 is not a real timezone
          let tzMinuteOffset = 210;  // GMT-3:30
          let tz = TimeZone.createTimeZone(tzMinuteOffset);
          let result = removeWhitespace(fmt.format(dt1, dt2, tz));
          // 4:30 hours crosses midnight
          // Handle some variations in formatting
          assertTrue(
              'nativeMode=' + nativeMode + ', result = ' + result,
              result === expected210 || result === expected210var2);

          // Format another range with the same TZ,
          const result2 = removeWhitespace(fmt.format(dt1, dt3, tz));
          const expected2 = removeWhitespace(
              'Sunday, December 31, 2006 at 11:30:23 PM UTC-3:30 – ' +
              'Monday, January 1, 2007 at 1:47:59 AM UTC-3:30');
          const expected2var = removeWhitespace(
              'Sunday, December 31, 2006, 11:30:23 PM UTC-3:30 – ' +
              'Monday, January 1, 2007, 1:47:59 AM UTC-3:30');
          assertTrue(
              'nativeMode=' + nativeMode + ', result = ' + result,
              result2 == expected2 || result2 == expected2var);

          // Check for GMT +3.5 hours (Iran)
          tzMinuteOffset = -210;
          // Check that formatter is reset.
          const tz2 = TimeZone.createTimeZone(tzMinuteOffset);
          let result3 = removeWhitespace(fmt.format(dt1, dt2, tz2));
          const expected3 = removeWhitespace(
              'Monday, January 1, 2007, 6:30:23 AM UTC+3:30 – 7:10:23 AM UTC+3:30');


          // Handle variation with "at 6:30..." in both parts
          const expected3var2 = removeWhitespace(
              'Monday, January 1, 2007 at 6:30:23 AM UTC+3:30 – Monday, January 1, 2007 at 7:10:23 AM UTC+3:30');

          assertTrue(
              'nativeMode=' + nativeMode + ' version=' + browser.getVersion(),
              result3 === expected3 || result3 === expected3var2);
        });
  },

  testTimeZone: function() {
    driveTests(
        'en', DateTimeSymbols_en,
        /** @param {boolean} nativeMode */
        (nativeMode) => {
          const dt1 = new Date(Date.UTC(2007, 0, 10, 6, 0, 23));
          const dt2 = new Date(Date.UTC(2007, 0, 10, 6, 20, 23));
          const fmt = new DateIntervalFormat(DateTimeFormat.Format.LONG_TIME);
          const tz = TimeZone.createTimeZone(240);
          let result = removeWhitespace(fmt.format(dt1, dt2, tz));
          // Standardize the result to "UTC" rather than "GMT"
          result = result.replaceAll('GMT', 'UTC');
          assertI18nEquals(
              'nativeMode=' + nativeMode + ' version=' + browser.getVersion(),
              '2:00:23 AM UTC-4 – 2:20:23 AM UTC-4', result);
        });
  },

  testFormatSecondDateWithFirstPattern: function() {
    driveTests(
        'en', DateTimeSymbols_en,
        /** @param {boolean} nativeMode */
        (nativeMode) => {
          // Set the new fallback pattern.
          const symbols =
              object.clone(dateIntervalSymbols.getDateIntervalSymbols());
          /**
           * @suppress {strictMissingProperties} suppression added to enable
           * type checking
           */
          symbols.FALLBACK = '{1} – {0}';
          // Format the dates.
          const dt1 = new GoogDate(2007, 1, 10);
          const dt2 = new GoogDate(2007, 6, 3);
          /**
           * @suppress {checkTypes} suppression added to enable type checking
           */
          const fmt =
              new DateIntervalFormat(DateTimeFormat.Format.LONG_DATE, symbols);
          const result = removeWhitespace(fmt.format(dt1, dt2));
          if (nativeMode) {
            // EXCEPTION: Native mode doesn't support custom fallback.
            assertI18nEquals(
                'nativeMode=' + nativeMode + ' version=' + browser.getVersion(),
                'February 10 – July 3, 2007', result);
          } else {
            // JavaScript mode supports custom fallback.
            assertI18nEquals(
                'nativeMode=' + nativeMode + ' version=' + browser.getVersion(),
                'July 3 – February 10, 2007', result);
          }
        });
  },

  testGetLargestDifferentCalendarField: function() {
    driveTests(
        'en', DateTimeSymbols_en,
        /** @param {boolean} nativeMode */
        (nativeMode) => {
          // Era
          let dt1 = new DateTime(-1, 1, 10);
          let dt2 = new DateTime(2007, 6, 3);
          /**
           * @suppress {visibility} suppression added to enable type checking
           */
          let calField =
              DateIntervalFormat.getLargestDifferentCalendarField_(dt1, dt2);
          assertEquals('G', calField);
          // Month
          dt1 = new DateTime(2007, 1, 10);
          dt2 = new DateTime(2007, 6, 3);
          /**
           * @suppress {visibility} suppression added to enable type checking
           */
          calField =
              DateIntervalFormat.getLargestDifferentCalendarField_(dt1, dt2);
          assertEquals('M', calField);
          // AmPm
          dt1 = new DateTime(2007, 1, 10, 10);
          dt2 = new DateTime(2007, 1, 10, 14);
          /**
           * @suppress {visibility} suppression added to enable type checking
           */
          calField =
              DateIntervalFormat.getLargestDifferentCalendarField_(dt1, dt2);
          assertEquals('a', calField);
          // AmPm + Timezone
          dt1 = new Date(Date.UTC(2007, 1, 10, 8, 25));
          dt2 = new Date(Date.UTC(2007, 1, 10, 8, 35));
          /**
           * @suppress {checkTypes} suppression added to enable type checking
           */
          const tz = new TimeZone.createTimeZone(-210);
          /**
           * @suppress {visibility} suppression added to enable type checking
           */
          calField = DateIntervalFormat.getLargestDifferentCalendarField_(
              dt1, dt2, tz);
          assertEquals('a', calField);
          // Seconds
          dt1 = new DateTime(2007, 1, 10, 10, 0, 1);
          dt2 = new DateTime(2007, 1, 10, 10, 0, 10);
          /**
           * @suppress {visibility} suppression added to enable type checking
           */
          calField =
              DateIntervalFormat.getLargestDifferentCalendarField_(dt1, dt2);
          assertEquals('s', calField);
        });
  },

  testDivideIntervalPattern: function() {
    driveTests(
        'en', DateTimeSymbols_en,
        /** @param {boolean} nativeMode */
        (nativeMode) => {
          /**
           * @suppress {visibility} suppression added to enable type checking
           */
          let pttn = DateIntervalFormat.divideIntervalPattern_('MMM d – d, y');
          assertObjectEquals({firstPart: 'MMM d – ', secondPart: 'd, y'}, pttn);
          /**
           * @suppress {visibility} suppression added to enable type checking
           */
          pttn = DateIntervalFormat.divideIntervalPattern_('MMM d, y');
          assertNull(pttn);
        });
  },

  testIsCalendarFieldLargerOrEqualThan: /**
                                           @suppress {visibility}
                                           suppression added to enable type
                                           checking
                                         */
      function() {
        assertTrue(
            DateIntervalFormat.isCalendarFieldLargerOrEqualThan_('G', 's'));
        assertTrue(
            DateIntervalFormat.isCalendarFieldLargerOrEqualThan_('a', 'm'));
        assertFalse(
            DateIntervalFormat.isCalendarFieldLargerOrEqualThan_('a', 'y'));
        assertFalse(
            DateIntervalFormat.isCalendarFieldLargerOrEqualThan_('a', '-'));
      },

});
