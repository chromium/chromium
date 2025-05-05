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

const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const replacer = new PropertyReplacer();

const DateLike = goog.require('goog.date.DateLike');
const DateTimeFormat = goog.require('goog.i18n.DateTimeFormat');
const DateTimeParse = goog.require('goog.i18n.DateTimeParse');
/** @suppress {extraRequire} */
const DateTimePatterns_ru = goog.require('goog.i18n.DateTimePatterns_ru');
const DateTimeSymbols_ca = goog.require('goog.i18n.DateTimeSymbols_ca');
const DateTimeSymbols_en = goog.require('goog.i18n.DateTimeSymbols_en');
const DateTimeSymbols_fa = goog.require('goog.i18n.DateTimeSymbols_fa');
const DateTimeSymbols_fr = goog.require('goog.i18n.DateTimeSymbols_fr');
const DateTimeSymbols_ko = goog.require('goog.i18n.DateTimeSymbols_ko');
const DateTimeSymbols_pl = goog.require('goog.i18n.DateTimeSymbols_pl');
const DateTimeSymbols_ru = goog.require('goog.i18n.DateTimeSymbols_ru');
const DateTimeSymbols_zh = goog.require('goog.i18n.DateTimeSymbols_zh');
const DateTimeSymbols_zh_TW = goog.require('goog.i18n.DateTimeSymbols_zh_TW');
const GoogDate = goog.require('goog.date.Date');
const testSuite = goog.require('goog.testing.testSuite');

const {DayPeriods_zh_Hant, setDayPeriods} = goog.require('goog.i18n.DayPeriods');

const DATETIMESYMBOLS =
    goog.reflect.objectProperty('DateTimeSymbols', goog.i18n);
const LOCALE = goog.reflect.objectProperty('LOCALE', goog);

replacer.replace(goog.i18n, DATETIMESYMBOLS, DateTimeSymbols_en);

/**
 * @record
 * @extends {DateTimeParse.ParseOptions}
 */
function AssertParseOptions() {
  /**
   * Expect only a partial parse (i.e. `parse` must return this value instead
   * of the full length of the input string).
   * @type {number|undefined}
   */
  this.partial;
}

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
 * @suppress {missingProperties} loose subclass property checks
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
 * @param {!AssertParseOptions=} options
 */
function assertParsedDateEquals(
    expectYear, expectMonth, expectDate, parser, text, options) {
  const date = new Date(0);

  assertEquals(
      options?.partial ?? text.length, parser.parse(text, date, options));
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
 * @param {!AssertParseOptions=} options
 */
function assertParsedTimeEquals(
    expectHour, expectMin, expectSec, expectMilli, parser, text, options) {
  const date = new Date(0);

  assertEquals(
      options?.partial ?? text.length, parser.parse(text, date, options));
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
  getTestName: function() {
    return 'DateTimeParse Tests';
  },

  setUpPage() {},

  setUp() {
    replacer.replace(goog, LOCALE, 'en');
    replacer.replace(goog.i18n, DATETIMESYMBOLS, DateTimeSymbols_en);
  },

  tearDown() {
    replacer.replace(goog.i18n, DATETIMESYMBOLS, DateTimeSymbols_en);
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
    assertParseFails(parser, '22b');
    assertParsedTimeEquals(1, 23, 0, 0, parser, '123b', {partial: 3});

    parser = new DateTimeParse('hhmma');
    assertParsedTimeEquals(1, 23, 0, 0, parser, '123');
    assertParsedTimeEquals(13, 23, 0, 0, parser, '123pm');
    assertParseFails(parser, '12');
    assertParseFails(parser, '12am');

    parser = new DateTimeParse('HHmmss');
    assertParsedTimeEquals(12, 34, 56, 0, parser, '123456789', {partial: 6});
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
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44p', {partial: 4});
    assertParsedTimeEquals(17, 44, 0, 0, parser, '5:44pm');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44ym', {partial: 4});

    parser = new DateTimeParse('h:mm a');
    assertParseFails(parser, '5');
    assertParseFails(parser, '5:');
    assertParseFails(parser, '5:4');
    assertParseFails(parser, '5:44');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44 ');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44   ');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44 p', {partial: 5});
    assertParsedTimeEquals(
        5, 44, 0, 0, parser, '5:44\u202f\u202fp', {partial: 6});
    assertParsedTimeEquals(17, 44, 0, 0, parser, '5:44 pm');
    assertParsedTimeEquals(17, 44, 0, 0, parser, '5:44\u202fpm');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44 ym', {partial: 5});
    assertParsedTimeEquals(
        5, 44, 0, 0, parser, '5:44\u1680\u00a0\t\u0020 ym', {partial: 9});

    parser = new DateTimeParse('mm:ss');
    const date = new Date(0);
    assertTrue(parser.parse('15:', date) > 0);
    assertEquals(15, date.getMinutes());
    assertEquals(0, date.getSeconds());
  },

  testTimeParsing_partial_nonBreakableSpace() {
    const parser = new DateTimeParse('h:mm\u00a0a');
    assertParseFails(parser, '5');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u00a0');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u00a0p', {partial: 5});
    assertParsedTimeEquals(17, 44, 0, 0, parser, '5:44\u00a0pm');
  },

  testTimeParsing_partial_narrowNonBreakableSpace() {
    const parser = new DateTimeParse('h:mm\u202fa');
    assertParseFails(parser, '5');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u202f');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u202fp', {partial: 5});
    assertParsedTimeEquals(17, 44, 0, 0, parser, '5:44\u202fpm');
  },

  testTimeParsing_partial_emQuad() {
    const parser = new DateTimeParse('h:mm\u2001a');
    assertParseFails(parser, '5');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u2001');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u2001p', {partial: 5});
    assertParsedTimeEquals(17, 44, 0, 0, parser, '5:44\u2001pm');
  },

  testTimeParsing_partial_enQuad() {
    const parser = new DateTimeParse('h:mm\u2000a');
    assertParseFails(parser, '5');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u2000');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u2000p', {partial: 5});
    assertParsedTimeEquals(17, 44, 0, 0, parser, '5:44\u2000pm');
  },

  testTimeParsing_partial_emSpace() {
    const parser = new DateTimeParse('h:mm\u2003a');
    assertParseFails(parser, '5');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u2003');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u2003p', {partial: 5});
    assertParsedTimeEquals(17, 44, 0, 0, parser, '5:44\u2003pm');
  },

  testTimeParsing_partial_enSpace() {
    const parser = new DateTimeParse('h:mm\u2002a');
    assertParseFails(parser, '5');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u2002');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u2002p', {partial: 5});
    assertParsedTimeEquals(17, 44, 0, 0, parser, '5:44\u2002pm');
  },

  testTimeParsing_partial_thickSpace() {
    const parser = new DateTimeParse('h:mm\u2004a');
    assertParseFails(parser, '5');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u2004');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u2004p', {partial: 5});
    assertParsedTimeEquals(17, 44, 0, 0, parser, '5:44\u2004pm');
  },

  testTimeParsing_partial_midSpace() {
    const parser = new DateTimeParse('h:mm\u2005a');
    assertParseFails(parser, '5');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u2005');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u2005p', {partial: 5});
    assertParsedTimeEquals(17, 44, 0, 0, parser, '5:44\u2005pm');
  },

  testTimeParsing_partial_thinSpace() {
    const parser = new DateTimeParse('h:mm\u2006a');
    assertParseFails(parser, '5');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u2006');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u2006p', {partial: 5});
    assertParsedTimeEquals(17, 44, 0, 0, parser, '5:44\u2006pm');
  },

  testTimeParsing_partial_thinSpace2() {
    const parser = new DateTimeParse('h:mm\u2009a');
    assertParseFails(parser, '5');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u2009');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u2009p', {partial: 5});
    assertParsedTimeEquals(17, 44, 0, 0, parser, '5:44\u2009pm');
  },

  testTimeParsing_partial_figureSpace() {
    const parser = new DateTimeParse('h:mm\u2007a');
    assertParseFails(parser, '5');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u2007');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u2007p', {partial: 5});
    assertParsedTimeEquals(17, 44, 0, 0, parser, '5:44\u2007pm');
  },

  testTimeParsing_partial_punctuationSpace() {
    const parser = new DateTimeParse('h:mm\u2008a');
    assertParseFails(parser, '5');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u2008');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u2008p', {partial: 5});
    assertParsedTimeEquals(17, 44, 0, 0, parser, '5:44\u2008pm');
  },

  testTimeParsing_partial_hairSpace() {
    const parser = new DateTimeParse('h:mm\u200aa');
    assertParseFails(parser, '5');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u200a');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u200ap', {partial: 5});
    assertParsedTimeEquals(17, 44, 0, 0, parser, '5:44\u200apm');
  },

  testTimeParsing_partial_mediumMatematicalSpace() {
    const parser = new DateTimeParse('h:mm\u205fa');
    assertParseFails(parser, '5');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u205f');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u205fp', {partial: 5});
    assertParsedTimeEquals(17, 44, 0, 0, parser, '5:44\u205fpm');
  },

  testTimeParsing_partial_ideographicSpace() {
    const parser = new DateTimeParse('h:mm\u3000a');
    assertParseFails(parser, '5');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u3000');
    assertParsedTimeEquals(5, 44, 0, 0, parser, '5:44\u3000p', {partial: 5});
    assertParsedTimeEquals(17, 44, 0, 0, parser, '5:44\u3000pm');
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
    assertParsedTimeEquals(
        17, 44, 0, 0, parser, '5:44 pmx', {...opts, partial: 7});

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
    replacer.replace(goog.i18n, DATETIMESYMBOLS, DateTimeSymbols_zh);

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
    let gmtDateStringPm = 'GMT-07:00 \u4E0B\u534803:26:28';

    // CLDR 40 data does not expect AM/PM for locale zh
    assertFalse(parser.parse(gmtDateStringPm, date) > 0);

    // Two digit hour with no am/pm
    const gmtDateStringHH = 'GMT-07:00 03:26:28';
    assertTrue(parser.parse(gmtDateStringHH, date) > 0);

    // This is now 10:00, not 10PM
    const normalizedHour =
        (24 + date.getHours() + date.getTimezoneOffset() / 60) % 24;
    assertEquals(10, normalizedHour);
    assertEquals(26, date.getMinutes());
    assertEquals(28, date.getSeconds());

    // Two digit 24-hour time with no am/pm
    const gmtDateString13 = 'GMT-07:00 13:26:28';
    assertTrue(parser.parse(gmtDateString13, date) > 0);

    // This is now 20:00, based in 24 hour time.
    const normalizedHour13 =
        (24 + date.getHours() + date.getTimezoneOffset() / 60) % 24;
    assertEquals(20, normalizedHour13);
  },

  testZhTwBFormat() {
    let nativeMode = true;
    replacer.replace(goog.i18n, DATETIMESYMBOLS, DateTimeSymbols_zh_TW);

    // Make sure we have the day period info.
    setDayPeriods(DayPeriods_zh_Hant);

    // Test for AM/PM with B for zh_TW
    let parser = new DateTimeParse(DateTimeFormat.Format.FULL_TIME);

    // 3:26 下午 (afternoon1)
    let gmtDateStringPm = '\u4E0B\u534803:26:28 [GMT-07:00]';
    let date = new Date(2006, 7 - 1, 24, 12, 12, 12, 0);

    let parsedDate = parser.parse(gmtDateStringPm, date);

    assertTrue(
        'nativeMode=' + nativeMode + ', parsedDate=' + parsedDate,
        parsedDate > 0);
    // This should be give 10PM == 22:00.
    const normalizedHourPm =
        (24 + date.getHours() + date.getTimezoneOffset() / 60) % 24;
    assertEquals(22, normalizedHourPm);
  },

  // For languages with goog.i18n.DateTimeSymbols.ZERODIGIT defined, the int
  // digits are localized by the locale in datetimeformat.js. This test case is
  // for parsing dates with such native digits.
  testDatesWithNativeDigits() {
    // Language Arabic is one example with
    replacer.replace(goog.i18n, DATETIMESYMBOLS, DateTimeSymbols_fa);

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
    replacer.replace(goog.i18n, DATETIMESYMBOLS, DateTimeSymbols_fr);
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
    replacer.replace(goog.i18n, DATETIMESYMBOLS, DateTimeSymbols_pl);
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
      replacer.replace(goog.i18n, DATETIMESYMBOLS, symbols[i]);
      const dateTimeSymbols = symbols[i];
      const tests = {
        'MMMM yyyy': dateTimeSymbols.MONTHS,
        'LLLL yyyy': dateTimeSymbols.STANDALONEMONTHS,
        'MMM yyyy': dateTimeSymbols.SHORTMONTHS,
        'LLL yyyy': dateTimeSymbols.STANDALONESHORTMONTHS,
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
    replacer.replace(goog.i18n, DATETIMESYMBOLS, DateTimeSymbols_en);

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


  testZhHantTwDayPeriods() {
    // b/208532468, 3-Dec-2021

    replacer.replace(goog, LOCALE, 'zh-TW');
    replacer.replace(goog.i18n, DATETIMESYMBOLS, DateTimeSymbols_zh_TW);
    // Set up for parts of the day in Chinese.
    setDayPeriods(DayPeriods_zh_Hant);

    // These cover the time periods for this locale. Note that this set
    // is generated with flexible time periods, producing several different
    // day period names.
    const testStringsZhHantTw = [
      '午夜12:00:00',
      '清晨5:30:00',
      '上午8:58:00',
      '中午12:00:00',
      '中午12:58:59',
      '下午2:17:00',
      '晚上7:00:00',
      '凌晨3:37:17',
    ];

    // These use AM/PM formats.
    const testStringsZhHantMo = [
      '上午12:00:00',
      '上午5:30:00',
      '上午8:58:00',
      '下午12:00:00',
      '下午12:58:59',
      '下午2:17:00',
      '下午7:00:00',
      '上午3:37:17',
    ];

    // Expected date time values
    const parseInfoZhHantTw = [
      [2022, 4, 20, 0, 0, 0], [2022, 4, 20, 5, 30, 0], [2022, 4, 20, 8, 58, 0],
      [2022, 4, 20, 12, 0, 0], [2022, 4, 20, 12, 58, 59],
      [2022, 4, 20, 14, 17, 0], [2022, 4, 20, 19, 0, 0],
      [2022, 4, 20, 3, 37, 17]
    ];

    let date = new Date(0);
    const parser = new DateTimeParse(DateTimeFormat.Format.MEDIUM_TIME);
    // Check the results make sense with both B and AM/PM formatting
    for (let index = 0; index < testStringsZhHantTw.length; index++) {
      const dVals = parseInfoZhHantTw[index];

      parser.parse(testStringsZhHantTw[index], date);
      assertTimeEquals(dVals[3], dVals[4], dVals[5], 0, date);
    }

    let date2 = new Date(0);
    const shortParser = new DateTimeParse(DateTimeFormat.Format.SHORT_TIME);
    // Check strings with only AM/PM data in Hant_MO.
    for (let index = 0; index < testStringsZhHantMo.length; index++) {
      const dVals = parseInfoZhHantTw[index];

      let parsedOK = shortParser.parse(testStringsZhHantMo[index], date2);
      assertTrue('index=' + index, parsedOK > 0);
      assertTimeEquals(dVals[3], dVals[4], 0, 0, date2);
    }
  },

  testRoundTripZhTw() {
    // Test for b/208532468 round trip with zh_TW with flexible time periods
    const date = new Date(0, 0, 0, 17);
    replacer.replace(goog, LOCALE, 'zh-TW');
    replacer.replace(goog.i18n, DATETIMESYMBOLS, DateTimeSymbols_zh_TW);

    setDayPeriods(DayPeriods_zh_Hant);

    // !!! TODO: check other mode
    let nativeMode = false;
    // replacer.replace(
    //     LocaleFeature, 'USE_ECMASCRIPT_I18N_DATETIMEF', nativeMode);
    const fmt = new DateTimeFormat(DateTimeFormat.Format.SHORT_TIME);
    assertTrue(fmt !== null);
    const result = fmt.format(date);
    const expected = '下午5:00';
    assertEquals('Native=' + nativeMode, expected, result);

    // Now parse this
    let parser = new DateTimeParse(DateTimeFormat.Format.SHORT_TIME);
    let newDate = new Date(0);
    let parsedOK = parser.parse(result, newDate);
    assertTrue(parsedOK > 0);
    assertTimeEquals(17, 0, 0, 0, newDate);
  },

  testNonAsciiSpaces() {
    const time_part = '3:26';
    const white_spaces = [
      ' ',
      '\t',
      '\xA0',
      '\u1680',
      '\u180e',
      '\u2000',
      '\u2001',
      '\u2002',
      '\u2003',
      '\u2004',
      '\u2005',
      '\u2006',
      '\u2007',
      '\u2008',
      '\u2009',
      '\u200a',
      '\u202f',
      '\u205f',
      '\u3000',
      '   ',                 // Multiple spaces
      '\u202f\u00a0\u200a',  // Multiple non-ASCII spaces
    ];

    let parser = new DateTimeParse(DateTimeFormat.Format.SHORT_TIME);
    let newDate = new Date(0);
    for (let index = 0; index < white_spaces.length; index++) {
      let input_string = time_part + white_spaces[index] + 'AM';
      let parsedOK = parser.parse(input_string, newDate);
      assertTrue(
          'Fails on index ' + index + ' >' + input_string + '<: ' + parsedOK,
          parsedOK > 0);
      assertTimeEquals(3, 26, 0, 0, newDate);
    }
  },

  testNonAsciiWithPatterns() {
    // Cloned from
    // google3/alkali/apps/twitteralerts/client/app/new_alert/parse_twitter.ts
    const dateFormats = [
      'h:mm a - d MMM yyyy',
      'h:mm a · d MMM yyyy',
      'h:mm a · d MMM, yyyy',
      'h:mm a · MMM d, yyyy',
    ];

    const twitter_dates = [
      '4:44\u202fAM  19 Jan 2018',
      '4:44 AM - 19 Jan 2018',
      '4:44 AM · 19 Jan 2018',
      '4:44 AM · 19 Jan, 2018',
    ];
    const date = new Date();
    for (let i = 0; i < dateFormats.length; i++) {
      for (let j = 0; j < twitter_dates.length; j++) {
        let text = twitter_dates[j];
        const parser = new DateTimeParse(dateFormats[i]);
        const parseDate = parser.parse(text, date, {validate: true});
        if (parseDate !== 0) {
          // DateTimeParse uses current seconds since they are not provided in
          // the input string. We prefer to have stable output, so we set
          // seconds to 0.
          assertParsedDateEquals(2018, 0, 19, parser, text);
        }
      }
    }
  },

  testRussianParseWithNnbs() {
    // Check that dates

    replacer.replace(goog.i18n, DATETIMESYMBOLS, DateTimeSymbols_ru);
    // Checking parse of output for non-ASCII whitespace characters.
    const test_cases = [
      '28 июн. 2012 г.',       // ASCII Space
      '28 июн. 2012 г.',       // Narrow non-breaking space
      '28 июн. 2012\tг.',      // Horizontal tab
      '28 июн. 2012\u3000г.',  // Ideographic space
      'чт, 28 июн. 2012 г.',
    ];

    // From datetimepatterns for Russian.
    const format_patterns = [
      DateTimePatterns_ru.MONTH_DAY_YEAR_MEDIUM,
      DateTimePatterns_ru.MONTH_DAY_YEAR_MEDIUM,
      DateTimePatterns_ru.MONTH_DAY_YEAR_MEDIUM,
      DateTimePatterns_ru.MONTH_DAY_YEAR_MEDIUM,
      DateTimePatterns_ru.WEEKDAY_MONTH_DAY_YEAR_MEDIUM,
    ];

    const output_date = new Date();
    for (let index = 0; index < test_cases.length; index++) {
      const string_date = test_cases[index];
      const pattern = format_patterns[index];

      const parser = new DateTimeParse(pattern);
      const parseDate =
          parser.parse(string_date, output_date, {validate: true});
      if (parseDate !== 0) {
        assertParsedDateEquals(2012, 5, 28, parser, string_date);
      }
    }
  },

  testBulgarian() {
    const short_time_string = '20:29 ч.';

    const short_time_bg_pattern = 'H:mm \'ч\'.';
    const parser = new DateTimeParse(short_time_bg_pattern);
    const output_date = new Date();
    const parseDate =
        parser.parse(short_time_string, output_date, {validate: true});
    if (parseDate !== 0) {
      const hr = output_date.getHours();
      const min = output_date.getMinutes();
      assertEquals(20, hr);
      assertEquals(29, min);
    }
  }
});
