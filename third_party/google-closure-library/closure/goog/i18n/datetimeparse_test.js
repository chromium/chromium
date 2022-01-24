/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview
 * @suppress {missingRequire} Swapping using fully qualified name
 */
goog.module('goog.i18n.DateTimeParseTest');
goog.setTestOnly();

const DateLike = goog.require('goog.date.DateLike');
const DateTimeFormat = goog.require('goog.i18n.DateTimeFormat');
const DateTimeParse = goog.require('goog.i18n.DateTimeParse');
/** @suppress {extraRequire} */
const DateTimeSymbols = goog.require('goog.i18n.DateTimeSymbols');
const DateTimeSymbols_ca = goog.require('goog.i18n.DateTimeSymbols_ca');
const DateTimeSymbols_en = goog.require('goog.i18n.DateTimeSymbols_en');
const DateTimeSymbols_fa = goog.require('goog.i18n.DateTimeSymbols_fa');
const DateTimeSymbols_fr = goog.require('goog.i18n.DateTimeSymbols_fr');
const DateTimeSymbols_ko = goog.require('goog.i18n.DateTimeSymbols_ko');
const DateTimeSymbols_pl = goog.require('goog.i18n.DateTimeSymbols_pl');
const DateTimeSymbols_zh = goog.require('goog.i18n.DateTimeSymbols_zh');
const GoogDate = goog.require('goog.date.Date');
const testSuite = goog.require('goog.testing.testSuite');

goog.i18n.DateTimeSymbols = DateTimeSymbols_en;


/**
 * Asserts that `date` has the expected date field values.
 * @param {number|undefined} expectYear
 * @param {number} expectMonth
 * @param {number|undefined} expectDate
 * @param {!DateLike} date
 */
function assertDateEquals(expectYear, expectMonth, expectDate, date) {
  if (expectYear !== undefined) {
    assertEquals(expectYear, date.getFullYear());
  }
  assertEquals(expectMonth, date.getMonth());
  if (expectDate !== undefined) {
    assertEquals(expectDate, date.getDate());
  }
}

/**
 * Asserts that `date` has the expected time field values.
 * @param {number} expectHour
 * @param {number} expectMin
 * @param {number|undefined} expectSec
 * @param {number|undefined} expectMilli
 * @param {!DateLike} date
 */
function assertTimeEquals(expectHour, expectMin, expectSec, expectMilli, date) {
  assertEquals(expectHour, date.getHours());
  assertEquals(expectMin, date.getMinutes());
  if (expectSec !== undefined) {
    assertEquals(expectSec, date.getSeconds());
  }
  if (expectMilli !== undefined) {
    assertEquals(expectMilli, date.getMilliseconds());
  }
}

/**
 * Parses and asserts that the result has the expected values.
 * @param {number|undefined} expectYear
 * @param {number} expectMonth calendar month (1-based)
 * @param {number|undefined} expectDate
 * @param {!DateTimeParse} parser
 * @param {string} text
 * @param {!DateTimeParse.ParseOptions=} options
 */
function assertParsedDateEquals(
    expectYear, expectMonth, expectDate, parser, text, options) {
  const date = new Date(0);

  assertTrue(parser.parse(text, date, options) > 0);
  assertDateEquals(expectYear, expectMonth, expectDate, date);
}

/**
 * Parses and asserts that the result has the expected values.
 * @param {number} expectHour
 * @param {number} expectMin
 * @param {number|undefined} expectSec
 * @param {number|undefined} expectMilli
 * @param {!DateTimeParse} parser
 * @param {string} text
 * @param {!DateTimeParse.ParseOptions=} options
 */
function assertParsedTimeEquals(
    expectHour, expectMin, expectSec, expectMilli, parser, text, options) {
  const date = new Date(0);

  assertTrue(parser.parse(text, date, options) > 0);
  assertTimeEquals(expectHour, expectMin, expectSec, expectMilli, date);
}

/**
 * Asserts that parsing of `text` fails.
 * @param {!DateTimeParse} parser
 * @param {string} text
 * @param {!DateTimeParse.ParseOptions=} options
 */
function assertParseFails(parser, text, options) {
  const date = new Date(0);

  assertEquals(0, parser.parse(text, date, options));
}

testSuite({
  tearDown() {
    goog.i18n.DateTimeSymbols = DateTimeSymbols_en;
  },

  testNegativeYear() {
    const parser = new DateTimeParse('MM/dd, yyyy');

    assertParsedDateEquals(1999, 11 - 1, 22, parser, '11/22, 1999');
    assertParsedDateEquals(-1999, 11 - 1, 22, parser, '11/22, -1999');
  },

  testEra() {
    const parser = new DateTimeParse('MM/dd, yyyyG');

    assertParsedDateEquals(-1998, 11 - 1, 22, parser, '11/22, 1999BC');
    assertParsedDateEquals(0, 11 - 1, 22, parser, '11/22, 1BC');
    assertParsedDateEquals(1999, 11 - 1, 22, parser, '11/22, 1999AD');
  },

  testAmbiguousYear() {
    // assume this year is 2006, year 27 to 99 will be interpret as 1927 to 1999
    // year 00 to 25 will be 2000 to 2025. Year 26 can be either 1926 or 2026
    // depend on the time being parsed and the time when this program runs.
    // For example, if the program is run at 2006/03/03 12:12:12, the following
    // code should work.
    // assertTrue(parser.parse('01/01/26 00:00:00:001', date) > 0);
    // assertTrue(date.getFullYear() == 2026 - 1900);
    // assertTrue(parser.parse('12/30/26 23:59:59:999', date) > 0);
    // assertTrue(date.getFullYear() == 1926 - 1900);

    // Since this test can run in any time, some logic needed here.

    let futureDate = new Date();
    futureDate.setFullYear(
        futureDate.getFullYear() + 100 -
        DateTimeParse.ambiguousYearCenturyStart);
    let ambiguousYear = futureDate.getFullYear() % 100;

    const parser = new DateTimeParse('MM/dd/yy HH:mm:ss:SSS');
    const date = new Date();

    let str = `01/01/${ambiguousYear} 00:00:00:001`;
    assertTrue(parser.parse(str, date) > 0);
    assertEquals(futureDate.getFullYear(), date.getFullYear());

    str = `12/31/${ambiguousYear} 23:59:59:999`;
    assertTrue(parser.parse(str, date) > 0);
    assertEquals(futureDate.getFullYear(), date.getFullYear() + 100);

    // Test the ability to move the disambiguation century
    DateTimeParse.ambiguousYearCenturyStart = 60;

    futureDate = new Date();
    futureDate.setFullYear(
        futureDate.getFullYear() + 100 -
        DateTimeParse.ambiguousYearCenturyStart);
    ambiguousYear = futureDate.getFullYear() % 100;

    str = `01/01/${ambiguousYear} 00:00:00:001`;
    assertTrue(parser.parse(str, date) > 0);
    assertEquals(futureDate.getFullYear(), date.getFullYear());

    str = `12/31/${ambiguousYear} 23:59:59:999`;
    assertTrue(parser.parse(str, date) > 0);
    assertEquals(futureDate.getFullYear(), date.getFullYear() + 100);

    // Reset parameter for other test cases
    DateTimeParse.ambiguousYearCenturyStart = 80;
  },

  testLeapYear() {
    const parser = new DateTimeParse('MMdd, yyyy');

    assertParsedDateEquals(2001, 3 - 1, 1, parser, '0229, 2001');
    assertParsedDateEquals(2000, 2 - 1, 29, parser, '0229, 2000');
  },

  testAbuttingNumberPatterns() {
    let parser = new DateTimeParse('hhmm');
    assertParsedTimeEquals(11, 22, 0, 0, parser, '1122');
    assertParsedTimeEquals(1, 22, 0, 0, parser, '122');
    assertParseFails(parser, '22');
    // Probable bug: non-digit can cause too-short abutting run to succeed
    assertParsedTimeEquals(2, 2, 0, 0, parser, '22b');

    parser = new DateTimeParse('HHmmss');
    assertParsedTimeEquals(12, 34, 56, 0, parser, '123456789');
    assertParsedTimeEquals(12, 34, 56, 0, parser, '123456');
    assertParsedTimeEquals(1, 23, 45, 0, parser, '12345');
    assertParseFails(parser, '1234');

    parser = new DateTimeParse('yyyyMMdd');
    assertParsedDateEquals(1999, 12 - 1, 2, parser, '19991202');
    assertParsedDateEquals(999, 12 - 1, 2, parser, '9991202');
    assertParsedDateEquals(99, 12 - 1, 2, parser, '991202');
    assertParsedDateEquals(9, 12 - 1, 2, parser, '91202');
    assertParseFails(parser, '1202');
  },

  testDelimitedNumberPatterns() {
    const parser = new DateTimeParse('H:mm');

    assertParsedTimeEquals(0, 22, 0, 0, parser, '0:22');
    assertParsedTimeEquals(0, 22, 0, 0, parser, '00:22');
    assertParsedTimeEquals(0, 22, 0, 0, parser, '000:22');
    assertParsedTimeEquals(1, 22, 0, 0, parser, '001:22');
    assertParsedTimeEquals(11, 22, 0, 0, parser, '011:22');
    assertParsedTimeEquals(11, 0, 0, 0, parser, '11:0');
    assertParsedTimeEquals(11, 0, 0, 0, parser, '11:00');
    assertParsedTimeEquals(11, 2, 0, 0, parser, '11:002');
    assertParsedTimeEquals(11, 22, 0, 0, parser, '11:0022');
  },

  testYearParsing() {
    let parser = new DateTimeParse('yyMMdd');
    assertParsedDateEquals(1999, 12 - 1, 2, parser, '991202');
    assertParseFails(parser, '-91202');
    assertParseFails(parser, '+91202');

    parser = new DateTimeParse('yyyyMMdd');
    assertParsedDateEquals(2005, 12 - 1, 2, parser, '20051202');

    parser = new DateTimeParse('MM/y');
    assertParsedDateEquals(1999, 12 - 1, undefined, parser, '12/1999');
    assertParsedDateEquals(19999, 12 - 1, undefined, parser, '12/19999');

    parser = new DateTimeParse('MM-y');
    assertParsedDateEquals(1999, 12 - 1, undefined, parser, '12-1999');
  },

  testMonthParsing() {
    let parser = new DateTimeParse('MMM d, y');
    assertParsedDateEquals(2021, 3 - 1, 31, parser, 'Mar 31, 2021');

    parser = new DateTimeParse('d MMM y', DateTimeSymbols_ca);
    assertParsedDateEquals(2021, 2 - 1, 1, parser, '1 de febrer 2021');
    assertParsedDateEquals(2021, 2 - 1, 1, parser, '1 febrer 2021');
    assertParsedDateEquals(2021, 2 - 1, 1, parser, '1 de febr. 2021');
    assertParsedDateEquals(2021, 2 - 1, 1, parser, '1 febr. 2021');
  },

  testGoogDateParsing() {
    const parser = new DateTimeParse('yyMMdd');
    const date = new GoogDate();

    assertTrue(parser.parse('991202', date) > 0);
    assertDateEquals(1999, 12 - 1, 2, date);
  },

  testTimeParsing_hh() {
    let parser = new DateTimeParse('hhmm');
    assertParsedTimeEquals(0, 22, 0, 0, parser, '0022');
    assertParsedTimeEquals(11, 22, 0, 0, parser, '1122');
    assertParsedTimeEquals(0, 22, 0, 0, parser, '1222');
    assertParsedTimeEquals(23, 22, 0, 0, parser, '2322');
    assertParsedTimeEquals(0, 22, 0, 0, parser, '2422');

    parser = new DateTimeParse('hhmma');
    assertParsedTimeEquals(0, 22, 0, 0, parser, '0022am');
    assertParsedTimeEquals(11, 22, 0, 0, parser, '1122am');
    assertParsedTimeEquals(0, 22, 0, 0, parser, '1222am');
    assertParsedTimeEquals(23, 22, 0, 0, parser, '2322am');
    assertParsedTimeEquals(0, 22, 0, 0, parser, '2422am');
    assertParsedTimeEquals(12, 22, 0, 0, parser, '0022pm');
    assertParsedTimeEquals(23, 22, 0, 0, parser, '1122pm');
    assertParsedTimeEquals(12, 22, 0, 0, parser, '1222pm');
    assertParsedTimeEquals(23, 22, 0, 0, parser, '2322pm');
    assertParsedTimeEquals(0, 22, 0, 0, parser, '2422pm');
  },

  testTimeParsing_KK() {
    let parser = new DateTimeParse('KKmm');
    assertParsedTimeEquals(0, 22, 0, 0, parser, '0022');
    assertParsedTimeEquals(11, 22, 0, 0, parser, '1122');
    assertParsedTimeEquals(12, 22, 0, 0, parser, '1222');
    assertParsedTimeEquals(23, 22, 0, 0, parser, '2322');
    assertParsedTimeEquals(0, 22, 0, 0, parser, '2422');

    parser = new DateTimeParse('KKmma');
    assertParsedTimeEquals(0, 22, 0, 0, parser, '0022am');
    assertParsedTimeEquals(11, 22, 0, 0, parser, '1122am');
    assertParsedTimeEquals(12, 22, 0, 0, parser, '1222am');
    assertParsedTimeEquals(23, 22, 0, 0, parser, '2322am');
    assertParsedTimeEquals(0, 22, 0, 0, parser, '2422am');
    assertParsedTimeEquals(12, 22, 0, 0, parser, '0022pm');
    assertParsedTimeEquals(23, 22, 0, 0, parser, '1122pm');
    assertParsedTimeEquals(12, 22, 0, 0, parser, '1222pm');
    assertParsedTimeEquals(23, 22, 0, 0, parser, '2322pm');
    assertParsedTimeEquals(0, 22, 0, 0, parser, '2422pm');
  },

  testTimeParsing_kk() {
    let parser = new DateTimeParse('kkmm');
    assertParsedTimeEquals(0, 22, 0, 0, parser, '0022');
    assertParsedTimeEquals(11, 22, 0, 0, parser, '1122');
    assertParsedTimeEquals(12, 22, 0, 0, parser, '1222');
    assertParsedTimeEquals(23, 22, 0, 0, parser, '2322');
    assertParsedTimeEquals(0, 22, 0, 0, parser, '2422');

    parser = new DateTimeParse('kkmma');
    assertParsedTimeEquals(0, 22, 0, 0, parser, '0022am');
    assertParsedTimeEquals(11, 22, 0, 0, parser, '1122am');
    assertParsedTimeEquals(12, 22, 0, 0, parser, '1222am');
    assertParsedTimeEquals(23, 22, 0, 0, parser, '2322am');
    assertParsedTimeEquals(0, 22, 0, 0, parser, '2422am');
    assertParsedTimeEquals(12, 22, 0, 0, parser, '0022pm');
    assertParsedTimeEquals(23, 22, 0, 0, parser, '1122pm');
    assertParsedTimeEquals(12, 22, 0, 0, parser, '1222pm');
    assertParsedTimeEquals(23, 22, 0, 0, parser, '2322pm');
    assertParsedTimeEquals(0, 22, 0, 0, parser, '2422pm');
  },

  testTimeParsing_HH() {
    let parser = new DateTimeParse('HHmm');
    assertParsedTimeEquals(0, 22, 0, 0, parser, '0022');
    assertParsedTimeEquals(11, 22, 0, 0, parser, '1122');
    assertParsedTimeEquals(12, 22, 0, 0, parser, '1222');
    assertParsedTimeEquals(23, 22, 0, 0, parser, '2322');
    assertParsedTimeEquals(0, 22, 0, 0, parser, '2422');

    parser = new DateTimeParse('HHmma');
    assertParsedTimeEquals(0, 22, 0, 0, parser, '0022am');
    assertParsedTimeEquals(11, 22, 0, 0, parser, '1122am');
    assertParsedTimeEquals(12, 22, 0, 0, parser, '1222am');
    assertParsedTimeEquals(23, 22, 0, 0, parser, '2322am');
    assertParsedTimeEquals(0, 22, 0, 0, parser, '2422am');
    assertParsedTimeEquals(12, 22, 0, 0, parser, '0022pm');
    assertParsedTimeEquals(23, 22, 0, 0, parser, '1122pm');
    assertParsedTimeEquals(12, 22, 0, 0, parser, '1222pm');
    assertParsedTimeEquals(23, 22, 0, 0, parser, '2322pm');
    assertParsedTimeEquals(0, 22, 0, 0, parser, '2422pm');
  },

  testTimeParsing_milliseconds() {
    const parser = new DateTimeParse('hh:mm:ss.SSS');

    assertParsedTimeEquals(11, 12, 13, 956, parser, '11:12:13.956');
    assertParsedTimeEquals(11, 12, 13, 950, parser, '11:12:13.95');
    assertParsedTimeEquals(11, 12, 13, 900, parser, '11:12:13.9');
  },

  testTimeParsing_partial() {
    let parser = new DateTimeParse('h:mma');
    assertParseFails(parser, '5');
    assertParsedTimeEquals(5, 0, 0, 0, parser, '5:');
    assertParsedTimeEquals(5, 4, 0, 0, parser, '5:4');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44p');
    assertParsedTimeEquals(17, 44, 0, 0, parser, '5:44pm');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44ym');

    parser = new DateTimeParse('h:mm a');
    assertParseFails(parser, '5');
    assertParseFails(parser, '5:');
    assertParseFails(parser, '5:4');
    assertParseFails(parser, '5:44');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44 ');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44 p');
    assertParsedTimeEquals(17, 44, 0, 0, parser, '5:44 pm');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44 ym');

    parser = new DateTimeParse('mm:ss');
    const date = new Date(0);
    assertTrue(parser.parse('15:', date) > 0);
    assertEquals(15, date.getMinutes());
    assertEquals(0, date.getSeconds());
  },

  testTimeParsing_overflow() {
    const parser = new DateTimeParse('H:mm');

    assertParsedTimeEquals(0, 0, 0, 0, parser, '24:00');
    assertParsedTimeEquals(0, 30, 0, 0, parser, '23:90');
  },

  testTimeParsing_predictive() {
    const opts =
        /** @type {!DateTimeParse.ParseOptions} */ ({predictive: true});

    let parser = new DateTimeParse('h:mm a');
    assertParsedTimeEquals(5, 0, 0, 0, parser, '5', opts);
    assertParsedTimeEquals(5, 0, 0, 0, parser, '5:', opts);
    assertParsedTimeEquals(5, 40, 0, 0, parser, '5:4', opts);
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44', opts);
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44 ', opts);
    assertParseFails(parser, '5:44 x', opts);
    assertParsedTimeEquals(17, 44, 0, 0, parser, '5:44 p', opts);
    assertParsedTimeEquals(17, 44, 0, 0, parser, '5:44 pm', opts);
    assertParsedTimeEquals(17, 44, 0, 0, parser, '5:44 pmx', opts);

    parser = new DateTimeParse('HH:mm');
    assertParsedTimeEquals(0, 0, 0, 0, parser, '0', opts);
    // 50 % 24 == 2
    assertParsedTimeEquals(2, 0, 0, 0, parser, '5', opts);
    assertParsedTimeEquals(2, 0, 0, 0, parser, '50', opts);
    assertParseFails(parser, '5:', opts);
    assertParsedTimeEquals(5, 0, 0, 0, parser, '05:', opts);
    assertParsedTimeEquals(5, 40, 0, 0, parser, '05:4', opts);
    assertParsedTimeEquals(5, 44, 0, 0, parser, '05:44', opts);
    assertParsedTimeEquals(17, 44, 0, 0, parser, '17:44', opts);
    // overflow
    assertParsedTimeEquals(18, 1, 0, 0, parser, '17:061', opts);

    parser = new DateTimeParse('a h시 m분', DateTimeSymbols_ko);
    assertParsedTimeEquals(10, 0, 0, 0, parser, '오전 10', opts);
    assertParsedTimeEquals(10, 2, 0, 0, parser, '오전 10시 2', opts);
    assertParsedTimeEquals(10, 2, 0, 0, parser, '오전 10시 2분', opts);
    assertParsedTimeEquals(16, 20, 0, 0, parser, '오후 4시 20', opts);
    assertParsedTimeEquals(16, 20, 0, 0, parser, '오후 4시 20분', opts);
  },

  testTimeParsing_predictiveValidate() {
    const opts =
        /** @type {!DateTimeParse.ParseOptions} */ (
            {predictive: true, validate: true});

    const parser = new DateTimeParse('HH:mm');
    assertParsedTimeEquals(5, 0, 0, 0, parser, '05', opts);
    assertParsedTimeEquals(12, 34, 0, 0, parser, '12:34', opts);
    assertParseFails(parser, '5', opts);
    assertParseFails(parser, '50', opts);
    assertParseFails(parser, '123', opts);
    assertParseFails(parser, '123:45', opts);
    assertParseFails(parser, '12:345', opts);
    assertParseFails(parser, '5:', opts);
  },

  testEnglishDate() {
    const parser = new DateTimeParse('yyyy MMM dd hh:mm');
    const date = new Date(0);

    assertTrue(parser.parse('2006 Jul 10 15:44', date) > 0);
    assertDateEquals(2006, 7 - 1, 10, date);
    assertTimeEquals(15, 44, undefined, undefined, date);
  },

  testChineseDate() {
    goog.i18n.DateTimeSymbols = DateTimeSymbols_zh;

    // JavaScript month start from 0, July is 7 - 1
    const date = new Date(2006, 7 - 1, 24, 12, 12, 12, 0);
    const formatter = new DateTimeFormat(DateTimeFormat.Format.FULL_DATE);
    const dateStr = formatter.format(date);
    let parser = new DateTimeParse(DateTimeFormat.Format.FULL_DATE);

    assertParsedDateEquals(2006, 7 - 1, 24, parser, dateStr);

    parser = new DateTimeParse(DateTimeFormat.Format.LONG_DATE);
    assertParsedDateEquals(
        2006, 7 - 1, 24, parser, '2006\u5E747\u670824\u65E5');

    parser = new DateTimeParse(DateTimeFormat.Format.FULL_TIME);
    assertTrue(parser.parse('GMT-07:00 \u4E0B\u534803:26:28', date) > 0);

    assertEquals(
        22, (24 + date.getHours() + date.getTimezoneOffset() / 60) % 24);
    assertEquals(26, date.getMinutes());
    assertEquals(28, date.getSeconds());
  },

  // For languages with goog.i18n.DateTimeSymbols.ZERODIGIT defined, the int
  // digits are localized by the locale in datetimeformat.js. This test case is
  // for parsing dates with such native digits.
  testDatesWithNativeDigits() {
    // Language Arabic is one example with
    // goog.i18n.DateTimeSymbols.ZERODIGIT defined.
    goog.i18n.DateTimeSymbols = DateTimeSymbols_fa;

    let formatter = new DateTimeFormat(DateTimeFormat.Format.FULL_DATE);
    let parser = new DateTimeParse(DateTimeFormat.Format.FULL_DATE);
    // JavaScript month starts from 0, July is 7 - 1
    let text = formatter.format(new Date(2006, 7 - 1, 24, 12, 12, 12, 0));

    assertParsedDateEquals(2006, 7 - 1, 24, parser, text);

    formatter = new DateTimeFormat(DateTimeFormat.Format.SHORT_DATE);
    parser = new DateTimeParse(DateTimeFormat.Format.SHORT_DATE);
    text = formatter.format(new Date(2006, 7 - 1, 24));

    assertParsedDateEquals(2006, 7 - 1, 24, parser, text);

    parser = new DateTimeParse('y/MM/dd H:mm:ss٫SS');

    assertParsedDateEquals(2006, 7 - 1, 27, parser, '۲۰۰۶/۰۷/۲۷ ۱۳:۱۰:۱۰٫۲۵');
  },

  testTimeZone() {
    const parser = new DateTimeParse('MM/dd/yyyy, hh:mm:ss zzz');
    const date = new Date();

    assertTrue(parser.parse('07/21/2003, 11:22:33 GMT-0700', date) > 0);
    const hourGmtMinus07 = date.getHours();

    assertTrue(parser.parse('07/21/2003, 11:22:33 GMT-0600', date) > 0);
    const hourGmtMinus06 = date.getHours();
    assertEquals(1, (hourGmtMinus07 + 24 - hourGmtMinus06) % 24);

    assertTrue(parser.parse('07/21/2003, 11:22:33 GMT-0800', date) > 0);
    const hourGmtMinus08 = date.getHours();
    assertEquals(1, (hourGmtMinus08 + 24 - hourGmtMinus07) % 24);

    assertTrue(parser.parse('07/21/2003, 23:22:33 GMT-0800', date) > 0);
    assertEquals((date.getHours() + 24 - hourGmtMinus07) % 24, 13);

    assertTrue(parser.parse('07/21/2003, 11:22:33 GMT+0800', date) > 0);
    const hourGmt08 = date.getHours();
    assertEquals(16, (hourGmtMinus08 + 24 - hourGmt08) % 24);

    assertTrue(parser.parse('07/21/2003, 11:22:33 GMT+08', date) > 0);
    assertEquals(hourGmt08, date.getHours());

    assertTrue(parser.parse('07/21/2003, 11:22:33 GMT0800', date) > 0);
    assertEquals(hourGmt08, date.getHours());

    // 'foo' is not a timezone
    assertParseFails(parser, '07/21/2003, 11:22:33 foo');
  },

  testWeekDay() {
    let parser = new DateTimeParse('EEEE, MM/dd/yyyy');
    const date = new Date();

    assertParsedDateEquals(2006, 8 - 1, 16, parser, 'Wednesday, 08/16/2006');
    assertParseFails(parser, 'Tuesday, 08/16/2006');
    assertParseFails(parser, 'Thursday, 08/16/2006');
    assertParsedDateEquals(2006, 8 - 1, 16, parser, 'Wed, 08/16/2006');
    assertParseFails(parser, 'Wasdfed, 08/16/2006');

    parser = new DateTimeParse('EEEE, MM/yyyy');

    date.setDate(25);
    assertParsedDateEquals(2006, 9 - 1, 27, parser, 'Wed, 09/2006');

    date.setDate(30);
    assertParsedDateEquals(2006, 9 - 1, 27, parser, 'Wed, 09/2006');

    date.setDate(30);
    assertParsedDateEquals(2006, 9 - 1, 25, parser, 'Mon, 09/2006');
  },

  testPredictiveOption() {
    const opts =
        /** @type {!DateTimeParse.ParseOptions} */ ({predictive: true});

    const parser = new DateTimeParse('h \'text\' m');
    assertParsedTimeEquals(9, 5, 0, 0, parser, '9 text 5', opts);
    assertParsedTimeEquals(9, 0, 0, 0, parser, '9 te', opts);
    assertParseFails(parser, '9 te 5', opts);
  },

  testValidateOption() {
    const opts = /** @type {!DateTimeParse.ParseOptions} */ ({validate: true});

    let parser = new DateTimeParse('yyyy/MM/dd');
    assertParseFails(parser, '2000/13/10', opts);
    assertParseFails(parser, '2000/13/40', opts);
    assertParsedDateEquals(2000, 11 - 1, 10, parser, '2000/11/10', opts);

    parser = new DateTimeParse('yy/MM/dd');
    assertParsedDateEquals(2000, 11 - 1, 10, parser, '00/11/10', opts);
    assertParsedDateEquals(1999, 11 - 1, 10, parser, '99/11/10', opts);
    assertParseFails(parser, '00/13/10', opts);
    assertParseFails(parser, '00/11/32', opts);
    assertParsedDateEquals(1900, 11 - 1, 2, parser, '1900/11/2', opts);

    parser = new DateTimeParse('hh:mm');
    assertParsedTimeEquals(15, 44, 0, 0, parser, '15:44', opts);
    assertParseFails(parser, '25:44', opts);
    assertParseFails(parser, '15:64', opts);

    // leap year
    parser = new DateTimeParse('yy/MM/dd');
    assertParsedDateEquals(2000, 2 - 1, 29, parser, '00/02/29', opts);
    assertParseFails(parser, '01/02/29', opts);
  },

  testEnglishQuarter() {
    const parser = new DateTimeParse('QQQQ yyyy');

    assertParsedDateEquals(2009, 1 - 1, 1, parser, '1st quarter 2009');
  },

  testEnglishShortQuarter() {
    const parser = new DateTimeParse('yyyyQQ');

    assertParsedDateEquals(2006, 4 - 1, 1, parser, '2006Q2');
  },

  testFrenchShortQuarter() {
    goog.i18n.DateTimeSymbols = DateTimeSymbols_fr;
    const parser = new DateTimeParse('yyyyQQ');

    assertParsedDateEquals(2009, 7 - 1, 1, parser, '2009T3');
  },

  testDate() {
    const parser = new DateTimeParse('M/d/yy');

    assertParsedDateEquals(1987, 5 - 1, 25, parser, '5/25/1987');
    assertParsedDateEquals(1987, 5 - 1, 25, parser, '05/25/1987');
    // Probable bug: numeric month parsing accepts text inputs
    assertParsedDateEquals(1987, 5 - 1, 25, parser, 'May/25/1987');
  },

  testDateTime() {
    const formatter = new DateTimeFormat(DateTimeFormat.Format.MEDIUM_DATETIME);
    const parser = new DateTimeParse(DateTimeFormat.Format.MEDIUM_DATETIME);
    const text = formatter.format(new Date(2006, 7 - 1, 24, 17, 21, 42, 0));

    const date = new Date(0);

    assertTrue(parser.parse(text, date) > 0);
    assertDateEquals(2006, 7 - 1, 24, date);
    assertTimeEquals(17, 21, 42, 0, date);
  },

  /** @bug 10075434 */
  testParseDateWithOverflow() {
    // We force the initial day of month to 30 so that it will always cause an
    // overflow in February, no matter if it is a leap year or not.
    const dateOrg = new Date(2006, 7 - 1, 30, 17, 21, 42, 0);
    let dateParsed;  // this will receive the result of the parsing

    const parserMonthYear = new DateTimeParse('MMMM yyyy');

    // The API can be a bit confusing, as this date is both input and output.
    // Benefit: fields that don't come from parsing are preserved.
    // In the typical use case, dateParsed = new Date()
    // and when you parse "February 3" the year is implied as "this year"
    // This works as intended.
    // But because of this we will initialize dateParsed from dateOrg
    // before every test (because the previous test changes it).

    dateParsed = new Date(dateOrg.getTime());

    // If preserved February 30 overflows, so we get the closest February day,
    // 28
    assertTrue(parserMonthYear.parse('February 2013', dateParsed) > 0);
    assertDateEquals(2013, 2 - 1, 28, dateParsed);

    dateParsed = new Date(dateOrg.getTime());

    // Same as above, but the last February date is 29 (leap year)
    assertTrue(parserMonthYear.parse('February 2012', dateParsed) > 0);
    assertDateEquals(2012, 2 - 1, 29, dateParsed);

    dateParsed = new Date(dateOrg.getTime());

    // Same as above, but no overflow (March has 31 days, 30 is OK)
    assertTrue(parserMonthYear.parse('March 2013', dateParsed) > 0);
    assertDateEquals(2013, 3 - 1, 30, dateParsed);

    dateParsed = new Date(dateOrg.getTime());

    // The pattern does not expect the day of month, so 12 is interpreted
    // as year, 12. May be weird, but this is the original behavior.
    // The overflow for leap year applies, same as above.
    assertTrue(parserMonthYear.parse('February 12, 2013', dateParsed) > 0);
    assertDateEquals(12, 2 - 1, 29, dateParsed);

    // We make sure that the fix did not break parsing with day of month present
    const parserMonthDayYear = new DateTimeParse('MMMM d, yyyy');

    dateParsed = new Date(dateOrg.getTime());

    assertTrue(parserMonthDayYear.parse('February 12, 2012', dateParsed) > 0);
    assertDateEquals(2012, 2 - 1, 12, dateParsed);

    dateParsed = new Date(dateOrg.getTime());

    // The current behavior when parsing 'February 31, 2012' is to
    // return 'March 2, 2012'
    // Expected or not, we make sure the fix does not break this.
    assertTrue(parserMonthDayYear.parse('February 31, 2012', dateParsed) > 0);
    assertDateEquals(2012, 3 - 1, 2, dateParsed);
  },

  /** @bug 9901750 */
  testStandaloneMonthPattern() {
    goog.i18n.DateTimeSymbols = DateTimeSymbols_pl;
    const date1 = new GoogDate(2006, 7 - 1);
    const date2 = new GoogDate();
    const formatter = new DateTimeFormat('LLLL yyyy');
    let parser = new DateTimeParse('LLLL yyyy');
    let dateStr = formatter.format(date1);

    assertTrue(parser.parse(dateStr, date2) > 0);
    assertDateEquals(date1.getFullYear(), date1.getMonth(), undefined, date2);

    // Sanity tests to make sure MMM... (and LLL...) formats still work for
    // different locales.
    const symbols = [DateTimeSymbols_en, DateTimeSymbols_pl];

    for (let i = 0; i < symbols.length; i++) {
      goog.i18n.DateTimeSymbols = symbols[i];
      const tests = {
        'MMMM yyyy': goog.i18n.DateTimeSymbols.MONTHS,
        'LLLL yyyy': goog.i18n.DateTimeSymbols.STANDALONEMONTHS,
        'MMM yyyy': goog.i18n.DateTimeSymbols.SHORTMONTHS,
        'LLL yyyy': goog.i18n.DateTimeSymbols.STANDALONESHORTMONTHS,
      };

      for (const format in tests) {
        const parser = new DateTimeParse(format);
        const months = tests[format];
        for (let m = 0; m < months.length; m++) {
          const dateStr = months[m] + ' 2006';
          const date = new GoogDate();

          assertTrue(parser.parse(dateStr, date) > 0);
          assertDateEquals(2006, m, undefined, date);
        }
      }
    }
  },

  testConstructorSymbols() {
    const fmtFr =
        new DateTimeFormat(DateTimeFormat.Format.FULL_DATE, DateTimeSymbols_fr);
    const parserFr =
        new DateTimeParse(DateTimeFormat.Format.FULL_DATE, DateTimeSymbols_fr);
    const dateStrFr = fmtFr.format(new Date(2015, 9 - 1, 28));

    assertParsedDateEquals(2015, 9 - 1, 28, parserFr, dateStrFr);

    const fmtZh =
        new DateTimeFormat(DateTimeFormat.Format.FULL_DATE, DateTimeSymbols_zh);
    const parserZh =
        new DateTimeParse(DateTimeFormat.Format.FULL_DATE, DateTimeSymbols_zh);
    const dateStrZh = fmtZh.format(new Date(2015, 9 - 1, 28));

    assertParsedDateEquals(2015, 9 - 1, 28, parserZh, dateStrZh);
  },

  testQuotedPattern() {
    // Regression test for b/29990921.
    goog.i18n.DateTimeSymbols = DateTimeSymbols_en;

    // Literal apostrophe
    let parser = new DateTimeParse('MMM \'\'yy');
    assertParsedDateEquals(2013, 11 - 1, undefined, parser, 'Nov \'13');

    // Quoted text
    parser = new DateTimeParse('MMM dd\'th\' yyyy');
    assertParsedDateEquals(2013, 11 - 1, 15, parser, 'Nov 15th 2013');

    // Quoted text (only opening apostrophe)
    parser = new DateTimeParse('MMM dd\'th yyyy');
    assertParsedDateEquals(undefined, 11 - 1, 15, parser, 'Nov 15th yyyy');

    // Quoted text with literal apostrophe
    parser = new DateTimeParse('MMM dd\'th\'\'\'');
    assertParsedDateEquals(undefined, 11 - 1, 15, parser, 'Nov 15th\'');

    // Quoted text with literal apostrophe (only opening apostrophe)
    parser = new DateTimeParse('MMM dd\'th\'\'');
    assertParsedDateEquals(undefined, 11 - 1, 15, parser, 'Nov 15th\'');
  },

  testNullDate() {
    const date = new Date();
    const parser = new DateTimeParse('MM/dd, yyyyG');
    assertNotThrows(() => {
      parser.parse('11/22, 1999', date);
    });
    assertThrows(() => {
      parser.parse('11/22, 1999', null);
    });
  },

  testPredictiveParseWithUnsupportedPattern() {
    const date = new Date();
    const opts =
        /** @type {!DateTimeParse.ParseOptions} */ ({predictive: true});

    // Abutting runs of numbers are not supported for predictive parsing.
    let parser = new DateTimeParse('hhmm');
    assertThrows(() => parser.parse('1234', date, opts));

    // The year field is not supported for predictive parsing.
    parser = new DateTimeParse('yyyy');
    assertThrows(() => parser.parse('1234', date, opts));
  },
});
