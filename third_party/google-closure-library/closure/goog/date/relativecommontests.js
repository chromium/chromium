/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.date.relativeCommonTests');
goog.setTestOnly('goog.date.relativeCommonTests');

const DateTime = goog.require('goog.date.DateTime');
const DateTimeFormat = goog.require('goog.i18n.DateTimeFormat');
const DateTimePatterns_ar = goog.require('goog.i18n.DateTimePatterns_ar');
const DateTimePatterns_bn = goog.require('goog.i18n.DateTimePatterns_bn');
const DateTimePatterns_es = goog.require('goog.i18n.DateTimePatterns_es');
const DateTimePatterns_fa = goog.require('goog.i18n.DateTimePatterns_fa');
const DateTimePatterns_fr = goog.require('goog.i18n.DateTimePatterns_fr');
const DateTimePatterns_no = goog.require('goog.i18n.DateTimePatterns_no');
const DateTimeSymbols_ar = goog.require('goog.i18n.DateTimeSymbols_ar');
const DateTimeSymbols_bn = goog.require('goog.i18n.DateTimeSymbols_bn');
const DateTimeSymbols_es = goog.require('goog.i18n.DateTimeSymbols_es');
const DateTimeSymbols_fa = goog.require('goog.i18n.DateTimeSymbols_fa');
const DateTimeSymbols_fr = goog.require('goog.i18n.DateTimeSymbols_fr');
const DateTimeSymbols_no = goog.require('goog.i18n.DateTimeSymbols_no');
/** @suppress {extraRequire} */
const NumberFormatSymbols = goog.require('goog.i18n.NumberFormatSymbols');
const NumberFormatSymbols_bn = goog.require('goog.i18n.NumberFormatSymbols_bn');
const NumberFormatSymbols_en = goog.require('goog.i18n.NumberFormatSymbols_en');
const NumberFormatSymbols_fa = goog.require('goog.i18n.NumberFormatSymbols_fa');
const NumberFormatSymbols_no = goog.require('goog.i18n.NumberFormatSymbols_no');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const relative = goog.require('goog.date.relative');
const relativeDateTimeSymbols = goog.require('goog.i18n.relativeDateTimeSymbols');
const testSuite = goog.require('goog.testing.testSuite');
const {assertI18nEquals} = goog.require('goog.testing.i18n.asserts');

// Testing stubs that autoreset after each test run.
/** @type {!PropertyReplacer} */
const stubs = new PropertyReplacer();

// Timestamp to base times for test on.
/** @type {number} */
const baseTime = new Date(2009, 2, 23, 14, 31, 6).getTime();

/**
 * Quick conversion to national digits, to increase readability of the
 * tests above.
 * @param {string|number} value
 * @return {string}
 */
function localizeNumber(value) {
  if (typeof value == 'number') {
    value = value.toString();
  }
  return DateTimeFormat.localizeNumbers(value);
}

/**
 * Create google DateTime object from timestamp
 * @param {number} timestamp
 * @return {!DateTime}
 */

function gdatetime(timestamp) {
  return new DateTime(new Date(timestamp));
}

/**
 * Create timestamp for specified time.
 * @param {string} str
 * @return {number}
 */
function timestamp(str) {
  return new Date(str).getTime();
}

testSuite({
  setUp() {
    // Ensure goog.now returns a constant timestamp.
    goog.now = () => baseTime;

    stubs.replace(goog, 'LOCALE', 'en-US');
    /** @suppress {missingRequire} */
    stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_en);
  },

  tearDown() {
    stubs.reset();

    // Resets state to default English
    relativeDateTimeSymbols.setRelativeDateTimeSymbols(
        relativeDateTimeSymbols.RelativeDateTimeSymbols_en);
  },

  testFormatRelativeForPastDates() {
    const fn = relative.format;

    assertI18nEquals(
        'Should round seconds to the minute below', '0 minutes ago',
        fn(timestamp('23 March 2009 14:30:10')));

    assertI18nEquals(
        'Should round seconds to the minute below', '1 minute ago',
        fn(timestamp('23 March 2009 14:29:56')));

    assertI18nEquals(
        'Should round seconds to the minute below', '2 minutes ago',
        fn(timestamp('23 March 2009 14:29:00')));

    assertI18nEquals('10 minutes ago', fn(timestamp('23 March 2009 14:20:10')));
    assertI18nEquals('59 minutes ago', fn(timestamp('23 March 2009 13:31:42')));
    assertI18nEquals('2 hours ago', fn(timestamp('23 March 2009 12:20:56')));
    assertI18nEquals('23 hours ago', fn(timestamp('22 March 2009 15:30:56')));
    assertI18nEquals('1 day ago', fn(timestamp('22 March 2009 12:11:04')));
    assertI18nEquals('1 day ago', fn(timestamp('22 March 2009 00:00:00')));
    assertI18nEquals('2 days ago', fn(timestamp('21 March 2009 23:59:59')));
    assertI18nEquals('2 days ago', fn(timestamp('21 March 2009 10:30:56')));
    assertI18nEquals('2 days ago', fn(timestamp('21 March 2009 00:00:00')));
    assertI18nEquals('3 days ago', fn(timestamp('20 March 2009 23:59:59')));
  },

  testFormatRelativeForFutureDates() {
    const fn = relative.format;

    assertI18nEquals(
        'Should round seconds to the minute below', 'in 1 minute',
        fn(timestamp('23 March 2009 14:32:05')));

    assertI18nEquals(
        'Should round seconds to the minute below', 'in 2 minutes',
        fn(timestamp('23 March 2009 14:33:00')));

    assertI18nEquals('in 1 minute', fn(timestamp('23 March 2009 14:32:00')));
    assertI18nEquals('in 10 minutes', fn(timestamp('23 March 2009 14:40:10')));
    assertI18nEquals('in 59 minutes', fn(timestamp('23 March 2009 15:29:15')));
    assertI18nEquals('in 2 hours', fn(timestamp('23 March 2009 17:20:56')));
    assertI18nEquals('in 23 hours', fn(timestamp('24 March 2009 13:30:56')));
    assertI18nEquals('in 1 day', fn(timestamp('24 March 2009 14:31:07')));
    assertI18nEquals('in 1 day', fn(timestamp('24 March 2009 16:11:04')));
    assertI18nEquals('in 1 day', fn(timestamp('24 March 2009 23:59:59')));
    assertI18nEquals('in 2 days', fn(timestamp('25 March 2009 00:00:00')));
    assertI18nEquals('in 2 days', fn(timestamp('25 March 2009 10:30:56')));
    assertI18nEquals('in 2 days', fn(timestamp('25 March 2009 23:59:59')));
    assertI18nEquals('in 3 days', fn(timestamp('26 March 2009 00:00:00')));
  },


  testFormatPast() {
    const fn = relative.formatPast;

    assertI18nEquals('59 minutes ago', fn(timestamp('23 March 2009 13:31:42')));
    assertI18nEquals('0 minutes ago', fn(timestamp('23 March 2009 14:32:05')));
    assertI18nEquals('0 minutes ago', fn(timestamp('23 March 2009 14:33:00')));
    assertI18nEquals('0 minutes ago', fn(timestamp('25 March 2009 10:30:56')));
  },


  testFormatDayNotShort() {
    stubs.set(relative, 'monthDateFormatter_', null);

    const fn = relative.formatDay;
    assertI18nEquals('Sep 25', fn(timestamp('25 September 2009 10:31:06')));
    assertI18nEquals('Mar 25', fn(timestamp('25 March 2009 00:12:19')));
  },

  testFormatDay() {
    stubs.set(relative, 'monthDateFormatter_', null);

    const fn = relative.formatDay;
    const formatter = new DateTimeFormat(DateTimeFormat.Format.SHORT_DATE);
    const format = goog.bind(formatter.format, formatter);

    assertI18nEquals('Sep 25', fn(timestamp('25 September 2009 10:31:06')));
    assertI18nEquals('Mar 25', fn(timestamp('25 March 2009 00:12:19')));

    relative.setCasingMode(false);
    assertI18nEquals('tomorrow', fn(timestamp('24 March 2009 10:31:06')));
    assertI18nEquals('tomorrow', fn(timestamp('24 March 2009 00:12:19')));
    assertI18nEquals('today', fn(timestamp('23 March 2009 10:31:06')));
    assertI18nEquals('today', fn(timestamp('23 March 2009 00:12:19')));
    assertI18nEquals('yesterday', fn(timestamp('22 March 2009 23:48:12')));
    assertI18nEquals('yesterday', fn(timestamp('22 March 2009 04:11:23')));
    assertI18nEquals('Mar 21', fn(timestamp('21 March 2009 15:54:45')));
    assertI18nEquals('Mar 19', fn(timestamp('19 March 2009 01:22:11')));

    // Test that a formatter can also be accepted as input.
    relative.setCasingMode(true);

    assertI18nEquals('Tomorrow', fn(timestamp('24 March 2009 10:31:06')));
    assertI18nEquals('Tomorrow', fn(timestamp('24 March 2009 00:12:19')));
    assertI18nEquals('Today', fn(timestamp('23 March 2009 10:31:06'), format));
    assertI18nEquals('Today', fn(timestamp('23 March 2009 00:12:19'), format));
    assertI18nEquals(
        'Yesterday', fn(timestamp('22 March 2009 23:48:12'), format));
    assertI18nEquals(
        'Yesterday', fn(timestamp('22 March 2009 04:11:23'), format));

    relative.setCasingMode(false);
    assertI18nEquals('today', fn(timestamp('23 March 2009 10:31:06'), format));
    assertI18nEquals('today', fn(timestamp('23 March 2009 00:12:19'), format));
    assertI18nEquals(
        'yesterday', fn(timestamp('22 March 2009 23:48:12'), format));
    assertI18nEquals(
        'yesterday', fn(timestamp('22 March 2009 04:11:23'), format));

    let expected = format(gdatetime(timestamp('21 March 2009 15:54:45')));
    assertI18nEquals(expected, fn(timestamp('21 March 2009 15:54:45'), format));
    expected = format(gdatetime(timestamp('19 March 2009 01:22:11')));
    assertI18nEquals(expected, fn(timestamp('19 March 2009 01:22:11'), format));

    expected = format(gdatetime(timestamp('1 January 2010 01:22:11')));
    assertI18nEquals(
        expected, fn(timestamp('1 January 2010 01:22:11'), format));
  },

  testFormatDay_daylightSavingTime() {
    // November 7th 2021, 11:57:16 pm PST-08:00 (the day daylight saving time
    // ended that year, at a time after it ended that day)
    const daylightSavingEndMs = 1636358236277;
    goog.now = () => daylightSavingEndMs;
    assertI18nEquals('today', relative.formatDay(daylightSavingEndMs));

    // March 14, 2021 11:12:34 pm PDT-07:00 (end of the day DST began)
    const daylightSavingStartMs = 1615788754000;
    // March 15, 2021 00:12:34 am PDT-07:00 (the day _after_ DST began)
    const nextDayMs = daylightSavingStartMs + 3600000;
    goog.now = () => daylightSavingStartMs;
    assertI18nEquals('tomorrow', relative.formatDay(nextDayMs));
  },

  testGetDateString() {
    const fn = relative.getDateString;

    assertI18nEquals(
        '2:21 PM (10 minutes ago)', fn(new Date(baseTime - 10 * 60 * 1000)));
    assertI18nEquals(
        '4:31 AM (10 hours ago)', fn(new Date(baseTime - 10 * 60 * 60 * 1000)));
    assertI18nEquals(
        'Friday, March 13, 2009 (10 days ago)',
        fn(new Date(baseTime - 10 * 24 * 60 * 60 * 1000)));
    assertI18nEquals(
        'Tuesday, March 3, 2009',
        fn(new Date(baseTime - 20 * 24 * 60 * 60 * 1000)));

    // Test that DateTime can also be accepted as input.
    assertI18nEquals(
        '2:21 PM (10 minutes ago)', fn(gdatetime(baseTime - 10 * 60 * 1000)));
    assertI18nEquals(
        '4:31 AM (10 hours ago)',
        fn(gdatetime(baseTime - 10 * 60 * 60 * 1000)));
    assertI18nEquals(
        'Friday, March 13, 2009 (10 days ago)',
        fn(gdatetime(baseTime - 10 * 24 * 60 * 60 * 1000)));
    assertI18nEquals(
        'Tuesday, March 3, 2009',
        fn(gdatetime(baseTime - 20 * 24 * 60 * 60 * 1000)));
  },

  testGetPastDateString() {
    const fn = relative.getPastDateString;
    assertI18nEquals(
        '2:21 PM (10 minutes ago)', fn(new Date(baseTime - 10 * 60 * 1000)));
    assertI18nEquals(
        '2:30 PM (1 minute ago)', fn(new Date(baseTime - 1 * 60 * 1000)));
    assertI18nEquals(
        '2:41 PM (0 minutes ago)', fn(new Date(baseTime + 10 * 60 * 1000)));

    // Test that DateTime can also be accepted as input.
    assertI18nEquals(
        '2:21 PM (10 minutes ago)', fn(gdatetime(baseTime - 10 * 60 * 1000)));
    assertI18nEquals('2:31 PM (0 minutes ago)', fn(gdatetime(baseTime)));
    assertI18nEquals(
        '2:30 PM (1 minute ago)', fn(gdatetime(baseTime - 1 * 60 * 1000)));
    assertI18nEquals(
        '2:41 PM (0 minutes ago)', fn(gdatetime(baseTime + 10 * 60 * 1000)));
  },

  // Test for non-English locales, too.
  testFormatSpanish() {
    let fn = relative.formatDay;

    stubs.replace(goog, 'LOCALE', 'es');

    // Spanish locale 'es'
    stubs.set(relative, 'monthDateFormatter_', null);
    stubs.set(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_es);
    stubs.set(goog.i18n, 'DateTimePatterns', DateTimePatterns_es);

    relativeDateTimeSymbols.setRelativeDateTimeSymbols(
        relativeDateTimeSymbols.RelativeDateTimeSymbols_es);

    // Checks casing issues.
    relative.setCasingMode(true);

    assertI18nEquals('Pasado mañana', fn(timestamp('25 March 2009 20:59:59')));
    assertI18nEquals('Anteayer', fn(timestamp('21 March 2009 19:00:02')));

    assertI18nEquals('Ayer', fn(timestamp('22 March 2009 04:11:23')));
    assertI18nEquals('Hoy', fn(timestamp('23 March 2009 14:11:23')));
    assertI18nEquals('Mañana', fn(timestamp('24 March 2009 12:10:23')));

    relative.setCasingMode(false);
    assertI18nEquals('pasado mañana', fn(timestamp('25 March 2009 20:59:59')));
    assertI18nEquals('anteayer', fn(timestamp('21 March 2009 19:00:02')));

    assertI18nEquals('ayer', fn(timestamp('22 March 2009 04:11:23')));
    assertI18nEquals('hoy', fn(timestamp('23 March 2009 14:11:23')));
    assertI18nEquals('mañana', fn(timestamp('24 March 2009 12:10:23')));

    // Outside the range. These should be localized.
    assertI18nEquals('26 mar', fn(timestamp('26 March 2009 12:10:23')));  //
    assertI18nEquals('28 feb', fn(timestamp('28 February 2009 12:10:23')));

    fn = relative.format;

    assertI18nEquals(
        'Should round seconds to the minute below', 'dentro de 1 minuto',
        fn(timestamp('23 March 2009 14:32:05')));
    assertI18nEquals(
        'Should round seconds to the minute below', 'dentro de 9 minutos',
        fn(timestamp('23 March 2009 14:39:07')));

    assertI18nEquals(
        'Should round days to the day below', 'dentro de 1 día',
        fn(timestamp('24 March 2009 14:32:05')));
    assertI18nEquals(
        'Should round days to the day below', 'dentro de 8 días',
        fn(timestamp('31 March 2009 14:39:07')));

    assertI18nEquals(
        'Should round hours to the hour below', 'hace 1 hora',
        fn(timestamp('23 March 2009 13:31:05')));
    assertI18nEquals(
        'Should round hour to the hour below', 'hace 8 horas',
        fn(timestamp('23 March 2009 06:31:04')));
  },

  testFormatFrench() {
    let fn = relative.formatDay;

    // Frence locale 'fr'
    stubs.replace(goog, 'LOCALE', 'fr');

    stubs.set(relative, 'monthDateFormatter_', null);
    stubs.set(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_fr);
    stubs.set(goog.i18n, 'DateTimePatterns', DateTimePatterns_fr);

    relativeDateTimeSymbols.setRelativeDateTimeSymbols(
        relativeDateTimeSymbols.RelativeDateTimeSymbols_fr);

    // Check for casing results.
    relative.setCasingMode(true);
    assertI18nEquals('Après-demain', fn(timestamp('25 March 2009 20:59:59')));
    assertI18nEquals('Avant-hier', fn(timestamp('21 March 2009 19:00:02')));

    assertI18nEquals('Hier', fn(timestamp('22 March 2009 04:11:23')));
    assertI18nEquals('Aujourd’hui', fn(timestamp('23 March 2009 14:11:23')));
    assertI18nEquals('Demain', fn(timestamp('24 March 2009 12:10:23')));

    relative.setCasingMode(false);
    assertI18nEquals('après-demain', fn(timestamp('25 March 2009 20:59:59')));
    assertI18nEquals('avant-hier', fn(timestamp('21 March 2009 19:00:02')));

    assertI18nEquals('hier', fn(timestamp('22 March 2009 04:11:23')));
    assertI18nEquals('aujourd’hui', fn(timestamp('23 March 2009 14:11:23')));
    assertI18nEquals('demain', fn(timestamp('24 March 2009 12:10:23')));

    // Outside the range. These should be localized.
    assertI18nEquals('26 mars', fn(timestamp('26 March 2009 12:10:23')));  //
    assertI18nEquals('28 févr.', fn(timestamp('28 February 2009 12:10:23')));
    fn = relative.format;

    assertI18nEquals(
        'Should round seconds to the minute below', 'dans 1 minute',
        fn(timestamp('23 March 2009 14:32:05')));
    assertI18nEquals(
        'Should round seconds to the minute below', 'dans 9 minutes',
        fn(timestamp('23 March 2009 14:39:07')));

    assertI18nEquals(
        'Should round days to the day below', 'dans 1 jour',
        fn(timestamp('24 March 2009 14:32:05')));
    assertI18nEquals(
        'Should round days to the day below', 'dans 8 jours',
        fn(timestamp('31 March 2009 14:39:07')));

    assertI18nEquals(
        'Should round hours to the hour below', 'il y a 1 heure',
        fn(timestamp('23 March 2009 13:31:05')));
    assertI18nEquals(
        'Should round hour to the hour below', 'il y a 8 heures',
        fn(timestamp('23 March 2009 06:31:04')));
  },

  testFormatArabic() {
    const fn = relative.formatDay;

    stubs.replace(goog, 'LOCALE', 'ar');

    // Arabic locale 'ar'
    stubs.set(relative, 'monthDateFormatter_', null);
    stubs.set(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_ar);
    stubs.set(goog.i18n, 'DateTimePatterns', DateTimePatterns_ar);

    relativeDateTimeSymbols.setRelativeDateTimeSymbols(
        relativeDateTimeSymbols.RelativeDateTimeSymbols_ar);

    assertI18nEquals('بعد الغد', fn(timestamp('25 March 2009 20:59:59')));
    assertI18nEquals('أول أمس', fn(timestamp('21 March 2009 19:00:02')));

    assertI18nEquals('أمس', fn(timestamp('22 March 2009 04:11:23')));
    assertI18nEquals('اليوم', fn(timestamp('23 March 2009 14:11:23')));
    assertI18nEquals('غدًا', fn(timestamp('24 March 2009 12:10:23')));

    // Outside the range. These should be localized.
    assertI18nEquals('26 مارس', fn(timestamp('26 March 2009 12:10:23')));  //
    assertI18nEquals('28 فبراير', fn(timestamp('28 February 2009 12:10:23')));
  },

  /* Tests for non-ASCII digits in formatter results */

  testFormatRelativeForPastDatesPersianDigits() {
    stubs.set(relative, 'monthDateFormatter_', null);
    stubs.set(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_fa);
    stubs.set(goog.i18n, 'DateTimePatterns', DateTimePatterns_fa);
    stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_fa);

    const fn = relative.format;

    // The text here is English, as it comes from localized resources, not
    // from CLDR. It works properly in production, but it's not loaded here.
    // Will need to wait for CLDR 24, when the data we need will be available,
    // so that we can add it to DateTimeSymbols and out of localization.

    // For Persian \u06F0 is the base, so \u6F0 = digit 0, \u6F5 = digit 5 ...
    // "Western" digits in square brackets for convenience

    stubs.replace(goog, 'LOCALE', 'en-u-nu-arabext');
    assertI18nEquals(
        'Should round seconds to the minute below',
        localizeNumber(0) + ' minutes ago',  // ۰ minutes ago
        fn(timestamp('23 March 2009 14:30:10')));

    assertI18nEquals(
        'Should round seconds to the minute below',
        localizeNumber(1) + ' minute ago',  // ۱ minute ago
        fn(timestamp('23 March 2009 14:29:56')));

    assertI18nEquals(
        'Should round seconds to the minute below',
        localizeNumber(2) + ' minutes ago',  // ۲ minutes ago
        fn(timestamp('23 March 2009 14:29:00')));

    assertI18nEquals(
        localizeNumber(10) + ' minutes ago',  // ۱۰ minutes ago
        fn(timestamp('23 March 2009 14:20:10')));
    assertI18nEquals(
        localizeNumber(59) + ' minutes ago',  // ۵۹ minutes ago
        fn(timestamp('23 March 2009 13:31:42')));
    assertI18nEquals(
        localizeNumber(2) + ' hours ago',  // ۲ hours ago
        fn(timestamp('23 March 2009 12:20:56')));
    assertI18nEquals(
        localizeNumber(23) + ' hours ago',  // ۲۳ hours ago
        fn(timestamp('22 March 2009 15:30:56')));
    assertI18nEquals(
        localizeNumber(1) + ' day ago',  // ۱ day ago
        fn(timestamp('22 March 2009 12:11:04')));
    assertI18nEquals(
        localizeNumber(1) + ' day ago',  // ۱ day ago
        fn(timestamp('22 March 2009 00:00:00')));
    assertI18nEquals(
        localizeNumber(2) + ' days ago',  // ۲ days ago
        fn(timestamp('21 March 2009 23:59:59')));
    assertI18nEquals(
        localizeNumber(2) + ' days ago',  // ۲ days ago
        fn(timestamp('21 March 2009 10:30:56')));
    assertI18nEquals(
        localizeNumber(2) + ' days ago',  // ۲ days ago
        fn(timestamp('21 March 2009 00:00:00')));
    assertI18nEquals(
        localizeNumber(3) + ' days ago',  // ۳ days ago
        fn(timestamp('20 March 2009 23:59:59')));

    stubs.replace(goog, 'LOCALE', 'fa');
    relativeDateTimeSymbols.setRelativeDateTimeSymbols(
        relativeDateTimeSymbols.RelativeDateTimeSymbols_fa);

    const result1 = fn(timestamp('21 March 2009 10:30:56'));
    assertI18nEquals('۲ روز پیش', result1);
  },

  testFormatRelativeForFutureDatesBengaliDigits() {
    stubs.set(relative, 'monthDateFormatter_', null);
    stubs.set(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_bn);
    stubs.set(goog.i18n, 'DateTimePatterns', DateTimePatterns_bn);
    stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_bn);

    // Get Bengali digits
    stubs.replace(goog, 'LOCALE', 'en-u-nu-beng');

    const fn = relative.format;

    // For Bengali \u09E6 is the base, so \u09E6 = digit 0, \u09EB = digit 5
    // "Western" digits in square brackets for convenience
    assertI18nEquals(
        'Should round seconds to the minute below',
        'in ' + localizeNumber(1) + ' minute',  // in ১ minute
        fn(timestamp('23 March 2009 14:32:05')));

    assertI18nEquals(
        'Should round seconds to the minute below',
        'in ' + localizeNumber(2) + ' minutes',  // in ২ minutes
        fn(timestamp('23 March 2009 14:33:00')));

    assertI18nEquals(
        'in ' + localizeNumber(10) + ' minutes',  // in ১০ minutes
        fn(timestamp('23 March 2009 14:40:10')));
    assertI18nEquals(
        'in ' + localizeNumber(59) + ' minutes',  // in ৫৯ minutes
        fn(timestamp('23 March 2009 15:29:15')));
    assertI18nEquals(
        'in ' + localizeNumber(2) + ' hours',  // in ২ hours
        fn(timestamp('23 March 2009 17:20:56')));
    assertI18nEquals(
        'in ' + localizeNumber(23) + ' hours',  // in ২৩ hours
        fn(timestamp('24 March 2009 13:30:56')));
    assertI18nEquals(
        'in ' + localizeNumber(1) + ' day',  // in ১ day
        fn(timestamp('24 March 2009 14:31:07')));
    assertI18nEquals(
        'in ' + localizeNumber(1) + ' day',  // in ১ day
        fn(timestamp('24 March 2009 16:11:04')));
    assertI18nEquals(
        'in ' + localizeNumber(1) + ' day',  // in ১ day
        fn(timestamp('24 March 2009 23:59:59')));
    assertI18nEquals(
        'in ' + localizeNumber(2) + ' days',  // in ২ days
        fn(timestamp('25 March 2009 00:00:00')));
    assertI18nEquals(
        'in ' + localizeNumber(2) + ' days',  // in ২ days
        fn(timestamp('25 March 2009 10:30:56')));
    assertI18nEquals(
        'in ' + localizeNumber(2) + ' days',  // in ২ days
        fn(timestamp('25 March 2009 23:59:59')));
    assertI18nEquals(
        'in ' + localizeNumber(3) + ' days',  // in ৩ days
        fn(timestamp('26 March 2009 00:00:00')));

    // Try Bengali text and numerals, too.
    relativeDateTimeSymbols.setRelativeDateTimeSymbols(
        relativeDateTimeSymbols.RelativeDateTimeSymbols_bn);

    stubs.replace(goog, 'LOCALE', 'bn');

    // For Bengali \u09E6 is the base, so \u09E6 = digit 0, \u09EB = digit 5
    // "Western" digits in square brackets for convenience

    const result1 = fn(timestamp('23 March 2009 14:32:05'));
    assertI18nEquals(
        'Should round seconds to the minute below', '১ মিনিটে', result1);

    const result2 = fn(timestamp('26 March 2009 00:00:00'));
    assertI18nEquals(
        'Should be Bengali text with Bengali digit.', '৩ দিনের মধ্যে', result2);
  },

  testFormatRelativeForFutureDatesNorwegian() {
    stubs.set(relative, 'monthDateFormatter_', null);
    stubs.set(goog.i18n, 'DateTimeSymbols', DateTimeSymbols_no);
    stubs.set(goog.i18n, 'DateTimePatterns', DateTimePatterns_no);
    stubs.set(goog.i18n, 'NumberFormatSymbols', NumberFormatSymbols_no);

    relativeDateTimeSymbols.setRelativeDateTimeSymbols(
        relativeDateTimeSymbols.RelativeDateTimeSymbols_no);

    // For a locale not in the ECMASCRIPT locale set.
    stubs.replace(goog, 'LOCALE', 'no');

    const fn = relative.format;
    assertI18nEquals('om 1 minutt', fn(timestamp('23 March 2009 14:32:05')));
  },

  testUpcasing() {
    // Tests package function that sentence-cases a string.
    const fn = relative.upcase;

    assertEquals('today', 'Today', fn('today'));

    assertEquals('Today', 'Today', fn('Today'));

    assertEquals('TODAY', 'TODAY', fn('TODAY'));

    assertEquals('tODAY', 'TODAY', fn('tODAY'));

    // Non-ascii
    assertEquals('', 'Ābc', fn('ābc'));

    assertEquals('', 'Ābc', fn('Ābc'));

    // Greek
    assertEquals('Greek 1', '\u0391\u03b2\u03b3', fn('\u0391\u03b2\u03b3'));

    assertEquals('Greek 2', '\u0391\u03b2\u03b3', fn('\u03b1\u03b2\u03b3'));

    // Cyrillic
    assertEquals('Cyrillic', 'Ађё', fn('ађё'));

    // Adlam, SMP, cased
    assertEquals('Adlam', '‮𞤀𞤦𞤷', fn('‮𞤀𞤦𞤷'));

    // Chakma, SMP, uncased
    assertEquals('Chakma', '𑄃𑄬𑄌𑄴𑄥𑄳𑄠', fn('𑄃𑄬𑄌𑄴𑄥𑄳𑄠'));
  }
});
