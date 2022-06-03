/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.i18n.DateIntervalFormatTest');
goog.setTestOnly('goog.i18n.DateIntervalFormatTest');

const DateIntervalFormat = goog.require('goog.i18n.DateIntervalFormat');
const DateRange = goog.require('goog.date.DateRange');
const DateTime = goog.require('goog.date.DateTime');
const DateTimeFormat = goog.require('goog.i18n.DateTimeFormat');
const DateTimeSymbols_ar_EG = goog.require('goog.i18n.DateTimeSymbols_ar_EG');
const DateTimeSymbols_en = goog.require('goog.i18n.DateTimeSymbols_en');
const DateTimeSymbols_fr_CA = goog.require('goog.i18n.DateTimeSymbols_fr_CA');
const DateTimeSymbols_gl = goog.require('goog.i18n.DateTimeSymbols_gl');
const DateTimeSymbols_hi = goog.require('goog.i18n.DateTimeSymbols_hi');
const DateTimeSymbols_zh = goog.require('goog.i18n.DateTimeSymbols_zh');
const GoogDate = goog.require('goog.date.Date');
const Interval = goog.require('goog.date.Interval');
const TimeZone = goog.require('goog.i18n.TimeZone');
const dateIntervalPatterns = goog.require('goog.i18n.dateIntervalPatterns');
const dateIntervalSymbols = goog.require('goog.i18n.dateIntervalSymbols');
const object = goog.require('goog.object');
const testSuite = goog.require('goog.testing.testSuite');

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

  new Data('fr_CA', [2007, 0, 10, 10, 10, 10],  [2007, 0, 10, 10, 10, 20],  dateIntervalPatterns.DateIntervalPatterns_fr_CA.MONTH_DAY_MEDIUM, '10 janvier'),
  new Data('fr_CA', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 10, 20, 10],  dateIntervalPatterns.DateIntervalPatterns_fr_CA.MONTH_DAY_MEDIUM, '10 janvier'),
  new Data('fr_CA', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 14, 10, 10],  dateIntervalPatterns.DateIntervalPatterns_fr_CA.MONTH_DAY_MEDIUM, '10 janvier'),
  new Data('fr_CA', [2007, 10, 10, 10, 10, 10], [2007, 10, 20, 10, 10, 10], dateIntervalPatterns.DateIntervalPatterns_fr_CA.MONTH_DAY_MEDIUM, '10 – 20 novembre'),
  new Data('fr_CA', [2007, 9, 10, 10, 10, 10],  [2007, 10, 10, 10, 10, 10], dateIntervalPatterns.DateIntervalPatterns_fr_CA.MONTH_DAY_MEDIUM, '10 octobre – 10 novembre'),
  new Data('fr_CA', [2007, 9, 10, 10, 10, 10],  [2008, 9, 10, 10, 10, 10],  dateIntervalPatterns.DateIntervalPatterns_fr_CA.MONTH_DAY_MEDIUM, '10 octobre 2007 – 10 octobre 2008'),

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

  new Data('hi', [2007, 0, 10, 10, 10, 10],  [2007, 0, 10, 10, 10, 20],  dateIntervalPatterns.DateIntervalPatterns_hi.MONTH_DAY_MEDIUM, '10 जनवरी'),
  new Data('hi', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 10, 20, 10],  dateIntervalPatterns.DateIntervalPatterns_hi.MONTH_DAY_MEDIUM, '10 जनवरी'),
  new Data('hi', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 14, 10, 10],  dateIntervalPatterns.DateIntervalPatterns_hi.MONTH_DAY_MEDIUM, '10 जनवरी'),
  new Data('hi', [2007, 10, 10, 10, 10, 10], [2007, 10, 20, 10, 10, 10], dateIntervalPatterns.DateIntervalPatterns_hi.MONTH_DAY_MEDIUM, '10 नवंबर–20'),
  new Data('hi', [2007, 9, 10, 10, 10, 10],  [2007, 10, 10, 10, 10, 10], dateIntervalPatterns.DateIntervalPatterns_hi.MONTH_DAY_MEDIUM, '10 अक्तूबर – 10 नवंबर'),
  new Data('hi', [2007, 9, 10, 10, 10, 10],  [2008, 9, 10, 10, 10, 10],  dateIntervalPatterns.DateIntervalPatterns_hi.MONTH_DAY_MEDIUM, '10 अक्तूबर 2007 – 10 अक्तूबर 2008'),

  new Data('zh', [2007, 0, 10, 10, 10, 10],  [2007, 0, 10, 10, 10, 20],  dateIntervalPatterns.DateIntervalPatterns_zh.WEEKDAY_MONTH_DAY_YEAR_MEDIUM, '2007年1月10日周三'),
  new Data('zh', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 10, 20, 10],  dateIntervalPatterns.DateIntervalPatterns_zh.WEEKDAY_MONTH_DAY_YEAR_MEDIUM, '2007年1月10日周三'),
  new Data('zh', [2007, 0, 10, 10, 0, 10],   [2007, 0, 10, 14, 10, 10],  dateIntervalPatterns.DateIntervalPatterns_zh.WEEKDAY_MONTH_DAY_YEAR_MEDIUM, '2007年1月10日周三'),
  new Data('zh', [2007, 10, 10, 10, 10, 10], [2007, 10, 20, 10, 10, 10], dateIntervalPatterns.DateIntervalPatterns_zh.WEEKDAY_MONTH_DAY_YEAR_MEDIUM, '2007年11月10日周六至20日周二'),
  new Data('zh', [2007, 9, 10, 10, 10, 10],  [2007, 10, 10, 10, 10, 10], dateIntervalPatterns.DateIntervalPatterns_zh.WEEKDAY_MONTH_DAY_YEAR_MEDIUM, '2007年10月10日周三至11月10日周六'),
  new Data('zh', [2007, 9, 10, 10, 10, 10],  [2008, 9, 10, 10, 10, 10],  dateIntervalPatterns.DateIntervalPatterns_zh.WEEKDAY_MONTH_DAY_YEAR_MEDIUM, '2007年10月10日周三至2008年10月10日周五')
];
// clang-format on

testSuite({
  testFormat: function() {
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
          data.pattern, symbols.DateIntervalSymbols, symbols.DateTimeSymbols);
      const tz = TimeZone.createTimeZone(0);
      assertEquals(
          data.getErrorDescription(), data.expected, fmt.format(dt1, dt2, tz));
    }
  },

  testRangeFormat: function() {
    const dt1 = new GoogDate(2007, 1, 10);
    const dt2 = new GoogDate(2007, 6, 3);
    const dtRng = new DateRange(dt1, dt2);
    const fmt = new DateIntervalFormat(DateTimeFormat.Format.LONG_DATE);
    assertEquals('February 10 – July 3, 2007', fmt.formatRange(dtRng));
  },

  testDateAndIntervalFormat: function() {
    const dt = new GoogDate(2007, 1, 10);
    const itv = new Interval(0, 4, 23);
    const fmt = new DateIntervalFormat(DateTimeFormat.Format.LONG_DATE);
    assertEquals('February 10 – July 3, 2007', fmt.format(dt, itv));
  },

  testNewYearFormat: function() {
    const dt1 = new Date(Date.UTC(2007, 0, 1, 3, 0, 23));
    const dt2 = new Date(Date.UTC(2007, 0, 1, 3, 40, 23));
    const fmt = new DateIntervalFormat(DateTimeFormat.Format.FULL_DATETIME);
    const tz = TimeZone.createTimeZone(210);
    assertEquals(
        'Sunday, December 31, 2006 at 11:30:23 PM UTC-3:30 – ' +
            'Monday, January 1, 2007 at 12:10:23 AM UTC-3:30',
        fmt.format(dt1, dt2, tz));
  },

  testTimeZone: function() {
    const dt1 = new Date(Date.UTC(2007, 0, 10, 6, 0, 23));
    const dt2 = new Date(Date.UTC(2007, 0, 10, 6, 20, 23));
    const fmt = new DateIntervalFormat(DateTimeFormat.Format.LONG_TIME);
    const tz = TimeZone.createTimeZone(240);
    assertEquals(
        '2:00:23 AM UTC-4 – 2:20:23 AM UTC-4', fmt.format(dt1, dt2, tz));
  },

  testFormatSecondDateWithFirstPattern: function() {
    // Set the new fallback pattern.
    const symbols = object.clone(dateIntervalSymbols.getDateIntervalSymbols());
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    symbols.FALLBACK = '{1} – {0}';
    // Format the dates.
    const dt1 = new GoogDate(2007, 1, 10);
    const dt2 = new GoogDate(2007, 6, 3);
    /** @suppress {checkTypes} suppression added to enable type checking */
    const fmt =
        new DateIntervalFormat(DateTimeFormat.Format.LONG_DATE, symbols);
    assertEquals('July 3 – February 10, 2007', fmt.format(dt1, dt2));
  },

  testGetLargestDifferentCalendarField: function() {
    // Era
    let dt1 = new DateTime(-1, 1, 10);
    let dt2 = new DateTime(2007, 6, 3);
    /** @suppress {visibility} suppression added to enable type checking */
    let calField =
        DateIntervalFormat.getLargestDifferentCalendarField_(dt1, dt2);
    assertEquals('G', calField);
    // Month
    dt1 = new DateTime(2007, 1, 10);
    dt2 = new DateTime(2007, 6, 3);
    /** @suppress {visibility} suppression added to enable type checking */
    calField = DateIntervalFormat.getLargestDifferentCalendarField_(dt1, dt2);
    assertEquals('M', calField);
    // AmPm
    dt1 = new DateTime(2007, 1, 10, 10);
    dt2 = new DateTime(2007, 1, 10, 14);
    /** @suppress {visibility} suppression added to enable type checking */
    calField = DateIntervalFormat.getLargestDifferentCalendarField_(dt1, dt2);
    assertEquals('a', calField);
    // AmPm + Timezone
    dt1 = new Date(Date.UTC(2007, 1, 10, 8, 25));
    dt2 = new Date(Date.UTC(2007, 1, 10, 8, 35));
    /** @suppress {checkTypes} suppression added to enable type checking */
    const tz = new TimeZone.createTimeZone(-210);
    /** @suppress {visibility} suppression added to enable type checking */
    calField =
        DateIntervalFormat.getLargestDifferentCalendarField_(dt1, dt2, tz);
    assertEquals('a', calField);
    // Seconds
    dt1 = new DateTime(2007, 1, 10, 10, 0, 1);
    dt2 = new DateTime(2007, 1, 10, 10, 0, 10);
    /** @suppress {visibility} suppression added to enable type checking */
    calField = DateIntervalFormat.getLargestDifferentCalendarField_(dt1, dt2);
    assertEquals('s', calField);
  },

  testDivideIntervalPattern: function() {
    /** @suppress {visibility} suppression added to enable type checking */
    let pttn = DateIntervalFormat.divideIntervalPattern_('MMM d – d, y');
    assertObjectEquals({firstPart: 'MMM d – ', secondPart: 'd, y'}, pttn);
    /** @suppress {visibility} suppression added to enable type checking */
    pttn = DateIntervalFormat.divideIntervalPattern_('MMM d, y');
    assertNull(pttn);
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
      }
});
