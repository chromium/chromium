/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dateTest');
goog.setTestOnly();

const DateDate = goog.require('goog.date.Date');
const DateTime = goog.require('goog.date.DateTime');
const Interval = goog.require('goog.date.Interval');
const googRequiredGoogDate = goog.require('goog.date');
const month = goog.require('goog.date.month');
const testSuite = goog.require('goog.testing.testSuite');
const weekDay = goog.require('goog.date.weekDay');


testSuite({
  /** Unit test for Closure's 'googRequiredGoogDate'. */
  testIsLeapYear() {
    const f = googRequiredGoogDate.isLeapYear;

    assertFalse('Year 1900 was not a leap year (the 100 rule)', f(1900));
    assertFalse('Year 1901 was not a leap year (the 100 rule)', f(1901));
    assertTrue('Year 1904 was a leap year', f(1904));
    assertFalse('Year 1999 was not a leap year', f(1999));
    assertTrue('Year 2000 was a leap year (the 400 rule)', f(2000));
    assertTrue('Year 2004 was a leap year', f(2004));
    assertFalse('Year 2006 was not a leap year', f(2006));
  },

  testIsLongIsoYear() {
    const f = googRequiredGoogDate.isLongIsoYear;

    // see http://en.wikipedia.org/wiki/ISO_week_date#The_leap_year_cycle
    assertFalse('1902 was not an ISO leap year', f(1902));
    assertTrue('1903 was an ISO leap year', f(1903));
    assertFalse('1904 was not an ISO leap year', f(1904));
    assertTrue('1981 was an ISO leap year', f(1981));
    assertTrue('1987 was an ISO leap year', f(1987));
    assertFalse('2000 was not an ISO leap year', f(2000));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testGetNumberOfDaysInMonth() {
    const f = googRequiredGoogDate.getNumberOfDaysInMonth;

    assertEquals('January has 31 days', f(2006, month.JAN, 2000), 31);
    assertEquals('February has 28 days', f(2006, month.FEB), 28);
    assertEquals('February has 29 days (leap year)', f(2008, month.FEB), 29);
    assertEquals('March has 31 days', f(2006, month.MAR), 31);
    assertEquals('April has 30 days', f(2006, month.APR), 30);
    assertEquals('May has 31 days', f(2006, month.MAY), 31);
    assertEquals('June has 30 days', f(2006, month.JUN), 30);
    assertEquals('July has 31 days', f(2006, month.JUL), 31);
    assertEquals('August has 31 days', f(2006, month.AUG), 31);
    assertEquals('September has 30 days', f(2006, month.SEP), 30);
    assertEquals('October has 31 days', f(2006, month.OCT), 31);
    assertEquals('November has 30 days', f(2006, month.NOV), 30);
    assertEquals('December has 31 days', f(2006, month.DEC), 31);
  },

  testIsSameDay() {
    assertTrue(
        'Dates are on the same day',
        googRequiredGoogDate.isSameDay(
            new Date('2009/02/01 12:45:12'), new Date('2009/02/01 01:15:49')));

    assertFalse(
        'Days are different',
        googRequiredGoogDate.isSameDay(
            new Date('2009/02/01 12:45:12'), new Date('2009/02/02 01:15:49')));

    assertFalse(
        'Months are different',
        googRequiredGoogDate.isSameDay(
            new Date('2009/02/01 12:45:12'), new Date('2009/03/01 01:15:49')));

    assertFalse(
        'Years are different',
        googRequiredGoogDate.isSameDay(
            new Date('2009/02/01 12:45:12'), new Date('2010/02/01 01:15:49')));

    assertFalse(
        'Wrong millennium',
        googRequiredGoogDate.isSameDay(
            new Date('2009/02/01 12:45:12'), new Date('1009/02/01 01:15:49')));
  },

  testIsSameMonth() {
    assertTrue(
        'Dates are on the same day',
        googRequiredGoogDate.isSameMonth(
            new Date('2009/02/01 12:45:12'), new Date('2009/02/01 01:15:49')));

    assertTrue(
        'Dates are in the same month',
        googRequiredGoogDate.isSameMonth(
            new Date('2009/02/01 12:45:12'), new Date('2009/02/10 01:15:49')));

    assertFalse(
        'Month is different',
        googRequiredGoogDate.isSameMonth(
            new Date('2009/02/01 12:45:12'), new Date('2009/03/01 01:15:49')));

    assertFalse(
        'Year is different',
        googRequiredGoogDate.isSameMonth(
            new Date('2008/02/01 12:45:12'), new Date('2009/02/01 01:15:49')));

    assertFalse(
        'Wrong millennium',
        googRequiredGoogDate.isSameMonth(
            new Date('2009/02/01 12:45:12'), new Date('1009/02/01 01:15:49')));
  },

  testIsSameYear() {
    assertTrue(
        'Dates are on the same day',
        googRequiredGoogDate.isSameYear(
            new Date('2009/02/01 12:45:12'), new Date('2009/02/01 01:15:49')));

    assertTrue(
        'Only days are different',
        googRequiredGoogDate.isSameYear(
            new Date('2009/02/01 12:45:12'), new Date('2009/02/11 01:15:49')));

    assertTrue(
        'Only months are different',
        googRequiredGoogDate.isSameYear(
            new Date('2009/02/01 12:45:12'), new Date('2009/02/01 01:15:49')));

    assertFalse(
        'Years are different',
        googRequiredGoogDate.isSameYear(
            new Date('2009/02/01 12:45:12'), new Date('2010/02/01 01:15:49')));

    assertFalse(
        'Years are different',
        googRequiredGoogDate.isSameYear(
            new Date('2009/02/01 12:45:12'), new Date('2008/02/01 01:15:49')));
  },

  testGetWeekNumber() {
    const f = googRequiredGoogDate.getWeekNumber;

    // Test cases from http://en.wikipedia.org/wiki/ISO_week_date#Examples
    assertEquals(
        '2005-01-01 is the week 53 of the previous year', 53,
        f(2005, month.JAN, 1));
    assertEquals(
        '2005-01-02 is the week 53 of the previous year', 53,
        f(2005, month.JAN, 2));
    assertEquals('2005-12-31 is the week 52', 52, f(2005, month.DEC, 31));
    assertEquals('2007-01-01 is the week 1', 1, f(2007, month.JAN, 1));
    assertEquals('2007-12-30 is the week 52', 52, f(2007, month.DEC, 30));
    assertEquals(
        '2007-12-31 is the week 1 of the following year', 1,
        f(2007, month.DEC, 31));
    assertEquals('2008-01-01 is the week 1', 1, f(2008, month.JAN, 1));
    assertEquals('2008-12-28 is the week 52', 52, f(2008, month.DEC, 28));
    assertEquals(
        '2008-12-29 is the week 1 of the following year', 1,
        f(2008, month.DEC, 29));
    assertEquals(
        '2008-12-31 is the week 1 of the following year', 1,
        f(2008, month.DEC, 31));
    assertEquals('2009-01-01 is the week 1', 1, f(2009, month.JAN, 1));
    assertEquals(
        '2009-12-31 is the week 53 of the previous year', 53,
        f(2009, month.DEC, 31));
    assertEquals(
        '2010-01-01 is the week 53 of the previous year', 53,
        f(2010, month.JAN, 1));
    assertEquals(
        '2010-01-03 is the week 53 of the previous year', 53,
        f(2010, month.JAN, 3));
    assertEquals('2010-01-04 is the week 1', 1, f(2010, month.JAN, 4));

    assertEquals(
        '2006-01-01 is in week 52 of the following year', 52,
        f(2006, month.JAN, 1));
    assertEquals('2006-01-02 is in week 1', 1, f(2006, month.JAN, 2));
    assertEquals('2006-10-16 is in week 42', 42, f(2006, month.OCT, 16));
    assertEquals('2006-10-19 is in week 42', 42, f(2006, month.OCT, 19));
    assertEquals('2006-10-22 is in week 42', 42, f(2006, month.OCT, 22));
    assertEquals('2006-10-23 is in week 43', 43, f(2006, month.OCT, 23));
    assertEquals(
        '2008-12-29 is in week 1 of the following year', 1,
        f(2008, month.DEC, 29));
    assertEquals(
        '2010-01-03 is in week 53 of the previous year', 53,
        f(2010, month.JAN, 3));

    assertEquals('2008-02-01 is in week 5', 5, f(2008, month.FEB, 1));
    assertEquals('2008-02-04 is in week 6', 6, f(2008, month.FEB, 4));

    // Tests for different cutoff days.
    assertEquals(
        '2006-01-01 is in week 52 of the prev. year (cutoff=Monday)', 52,
        f(2006, month.JAN, 1, weekDay.MON));
    assertEquals(
        '2006-01-01 is in week 1 (cutoff=Sunday)', 1,
        f(2006, month.JAN, 1, weekDay.SUN));
    assertEquals(
        '2006-12-31 is in week 52 (cutoff=Monday)', 52,
        f(2006, month.DEC, 31, weekDay.MON));
    assertEquals(
        '2006-12-31 is in week 53 (cutoff=Sunday)', 53,
        f(2006, month.DEC, 31, weekDay.SUN));
    assertEquals(
        '2007-01-01 is in week 1 (cutoff=Monday)', 1,
        f(2007, month.JAN, 1, weekDay.MON));
    assertEquals(
        '2007-01-01 is in week 1 (cutoff=Sunday)', 1,
        f(2007, month.JAN, 1, weekDay.SUN));
    assertEquals(
        '2015-01-01 is in week 52 of the previous year (cutoff=Monday)', 52,
        f(2015, month.JAN, 1, weekDay.MON));

    // Tests for leap year 2000.
    assertEquals('2000-02-27 is in week 8', 8, f(2000, month.FEB, 27));
    assertEquals('2000-02-28 is in week 9', 9, f(2000, month.FEB, 28));
    assertEquals('2000-02-29 is in week 9', 9, f(2000, month.FEB, 29));
    assertEquals('2000-03-01 is in week 9', 9, f(2000, month.MAR, 1));
    assertEquals('2000-03-05 is in week 9', 9, f(2000, month.MAR, 5));
    assertEquals('2000-03-06 is in week 10', 10, f(2000, month.MAR, 6));

    // Check that week number is strictly incremented by 1.
    const dt = new DateDate(2008, month.JAN, 1);
    for (let i = 0; i < 52; ++i) {
      const expected_week = i + 1;
      assertEquals(
          dt.toUTCIsoString(true) + ' is in week ' + expected_week,
          expected_week, dt.getWeekNumber());
      dt.add(new Interval(Interval.DAYS, 7));
    }
  },

  testGetYearOfWeek() {
    const f = googRequiredGoogDate.getYearOfWeek;

    // Test cases from http://en.wikipedia.org/wiki/ISO_week_date#Examples
    assertEquals(
        '2005-01-01 is the week 53 of the previous year', 2004,
        f(2005, month.JAN, 1));
    assertEquals(
        '2005-01-02 is the week 53 of the previous year', 2004,
        f(2005, month.JAN, 2));
    assertEquals(
        '2005-12-31 is the week 52 of current year', 2005,
        f(2005, month.DEC, 31));
    assertEquals(
        '2007-01-01 is the week 1 of 2007', 2007, f(2007, month.JAN, 1));
    assertEquals(
        '2007-12-30 is the week 52 of 2007', 2007, f(2007, month.DEC, 30));
    assertEquals(
        '2007-12-31 is the week 1 of the following year', 2008,
        f(2007, month.DEC, 31));
    assertEquals(
        '2008-01-01 is the week 1 of 2008', 2008, f(2008, month.JAN, 1));
    assertEquals(
        '2008-12-28 is the week 52 of 2008', 2008, f(2008, month.DEC, 28));
    assertEquals(
        '2008-12-29 is the week 1 of the following year', 2009,
        f(2008, month.DEC, 29));
    assertEquals(
        '2008-12-31 is the week 1 of the following year', 2009,
        f(2008, month.DEC, 31));
    assertEquals(
        '2009-01-01 is the week 1 of 2009', 2009, f(2009, month.JAN, 1));
    assertEquals(
        '2009-12-31 is the week 53 of the previous year', 2009,
        f(2009, month.DEC, 31));
    assertEquals(
        '2010-01-01 is the week 53 of the previous year', 2009,
        f(2010, month.JAN, 1));
    assertEquals(
        '2010-01-03 is the week 53 of the previous year', 2009,
        f(2010, month.JAN, 3));
    assertEquals(
        '2010-01-04 is the week 1 of 2010', 2010, f(2010, month.JAN, 4));

    assertEquals(
        '2006-01-01 is in week 52 of the perv. year', 2005,
        f(2006, month.JAN, 1));
    assertEquals(
        '2006-01-02 is in week 1 of 2006', 2006, f(2006, month.JAN, 2));

    // Tests for different cutoff days.
    assertEquals(
        '2006-01-01 is in week 52 of the prev. year (cutoff=Monday)', 2005,
        f(2006, month.JAN, 1, weekDay.MON));
    assertEquals(
        '2006-01-01 is in week 1 (cutoff=Sunday)', 2006,
        f(2006, month.JAN, 1, weekDay.SUN));
    assertEquals(
        '2006-12-31 is in 2006 year of week (cutoff=Monday)', 2006,
        f(2006, month.DEC, 31, weekDay.MON));
    assertEquals(
        '2006-12-31 is in 2006 year of week (cutoff=Sunday)', 2006,
        f(2006, month.DEC, 31, weekDay.SUN));
    assertEquals(
        '2007-01-01 is in 2007 year of week (cutoff=Monday)', 2007,
        f(2007, month.JAN, 1, weekDay.MON));
    assertEquals(
        '2007-01-01 is in 2007 year of week (cutoff=Sunday)', 2007,
        f(2007, month.JAN, 1, weekDay.SUN));
    assertEquals(
        '2015-01-01 is in the previous year of week (cutoff=Monday)', 2014,
        f(2015, month.JAN, 1, weekDay.MON));
  },

  testIsDateLikeWithGoogDate() {
    const jsDate = new Date();
    const googDate = new DateDate();
    const string = 'foo';
    const number = 1;
    const nullVar = null;
    let notDefined;

    assertTrue('js Date should be date-like', goog.isDateLike(jsDate));
    assertTrue('goog Date should be date-like', goog.isDateLike(googDate));
    assertFalse('string should not be date-like', goog.isDateLike(string));
    assertFalse('number should not be date-like', goog.isDateLike(number));
    assertFalse('nullVar should not be date-like', goog.isDateLike(nullVar));
    assertFalse(
        'undefined should not be date-like', goog.isDateLike(notDefined));
  },

  testDateConstructor() {
    let date = new DateDate(2001, 2, 3);
    assertEquals(2001, date.getFullYear());
    assertEquals(2, date.getMonth());
    assertEquals(3, date.getDate());

    date = new DateDate(2001);
    assertEquals(2001, date.getFullYear());
    assertEquals(0, date.getMonth());
    assertEquals(1, date.getDate());

    date = new DateDate(new Date(2001, 2, 3, 4, 5, 6, 7));
    assertEquals(2001, date.getFullYear());
    assertEquals(2, date.getMonth());
    assertEquals(3, date.getDate());
    assertEquals(new Date(2001, 2, 3).getTime(), date.getTime());

    goog.now = () => new Date(2001, 2, 3, 4).getTime();
    date = new DateDate();
    assertEquals(2001, date.getFullYear());
    assertEquals(2, date.getMonth());
    assertEquals(3, date.getDate());
    assertEquals(new Date(2001, 2, 3).getTime(), date.getTime());
  },

  testDateConstructor_yearBelow100() {
    const date = new DateDate(14, 7, 19);
    assertEquals(
        'Date constructor should respect passed in full year', 14,
        date.getFullYear());

    const copied = new DateDate(date);
    assertEquals(
        'Copying a should return identical date', date.getTime(),
        copied.getTime());
    assertEquals(
        'Full year should be left intact by copying', 14, copied.getFullYear());

    // Test boundaries.
    assertEquals(-1, new DateDate(-1, 0, 1).getFullYear());
    assertEquals(
        'There is no year zero, but JS dates accept it', 0,
        new DateDate(0, 0, 1).getFullYear());
    assertEquals(1, new DateDate(1, 0, 1).getFullYear());
    assertEquals(99, new DateDate(99, 0, 1).getFullYear());
    assertEquals(100, new DateDate(100, 0, 1).getFullYear());
  },

  testDateConstructor_flipOver() {
    let date = new DateDate(2012, 12, 1);
    assertEquals('20130101', date.toIsoString());

    date = new DateDate(12, 12, 1);
    assertEquals('130101', date.toIsoString());
  },

  testDateToIsoString() {
    let d = new DateDate(2006, month.JAN, 1);
    assertEquals('1 Jan 2006 is 20060101', d.toIsoString(), '20060101');

    d = new DateDate(2007, month.JUN, 12);
    assertEquals('12 Jun 2007 is 20070612', d.toIsoString(), '20070612');

    d = new DateDate(2218, month.DEC, 31);
    assertEquals('31 Dec 2218 is 22181231', d.toIsoString(), '22181231');
  },

  testDateTimeFromTimestamp() {
    assertEquals(0, DateTime.fromTimestamp(0).getTime());
    assertEquals(1234, DateTime.fromTimestamp(1234).getTime());
  },

  testRfc822StringToDate() {
    let date = DateTime.fromRfc822String('October 2, 2002 8:00:00');
    assertNotNull(date);
    assertEquals(2002, date.getFullYear());
    assertEquals(9, date.getMonth());
    assertEquals(2, date.getDate());
    assertEquals(8, date.getHours());
    assertEquals(0, date.getMinutes());
    assertEquals(0, date.getSeconds());
    assertEquals(0, date.getMilliseconds());
    assertEquals(new Date(2002, 9, 2, 8).getTime(), date.getTime());

    date = DateTime.fromRfc822String('Sat, 02 Oct 2010 08:00:00 UTC');
    assertEquals(2010, date.getFullYear());
    assertEquals(9, date.getUTCMonth());
    assertEquals(2, date.getUTCDate());
    assertEquals(8, date.getUTCHours());
    assertEquals(0, date.getUTCMinutes());
    assertEquals(0, date.getUTCSeconds());
    assertEquals(0, date.getUTCMilliseconds());

    date = DateTime.fromRfc822String('');
    assertEquals(null, date);

    date = DateTime.fromRfc822String('Invalid Date String');
    assertEquals(null, date);

    date = DateTime.fromRfc822String('Sat, 02 Oct 2010');
    assertEquals(2010, date.getFullYear());
    assertEquals(9, date.getMonth());
    assertEquals(2, date.getDate());
    assertEquals(0, date.getHours());
    assertEquals(0, date.getMinutes());
    assertEquals(0, date.getSeconds());
    assertEquals(0, date.getMilliseconds());
    assertEquals(new Date(2010, 9, 2).getTime(), date.getTime());
  },

  testIsoStringToDate() {
    let iso = '20060210T000000Z';
    let date = DateTime.fromIsoString(iso);

    assertEquals(`Got 2006 from ${iso}`, 2006, date.getFullYear());
    assertEquals(`Got February from ${iso}`, 1, date.getMonth());
    // use getUTCDate() here, since in 'iso' var we specified timezone
    // as being a zero offset from GMT
    assertEquals(`Got 10th from ${iso}`, 10, date.getUTCDate());

    // YYYY-MM-DD
    iso = '2005-02-22';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2005 from ${iso}`, 2005, date.getFullYear());
    assertEquals(`Got February from ${iso}`, 1, date.getMonth());
    assertEquals(`Got 22nd from ${iso}`, 22, date.getDate());

    // YYYYMMDD
    iso = '20050222';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2005 from ${iso}`, 2005, date.getFullYear());
    assertEquals(`Got February from ${iso}`, 1, date.getMonth());
    assertEquals(`Got 22nd from ${iso}`, 22, date.getDate());

    // YYYY-MM
    iso = '2005-08';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2005 from ${iso}`, 2005, date.getFullYear());
    assertEquals(`Got August from ${iso}`, 7, date.getMonth());

    // YYYYMM
    iso = '200502';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2005 from ${iso}`, 2005, date.getFullYear());
    assertEquals(`Got February from ${iso}`, 1, date.getMonth());

    // YYYY
    iso = '2005';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2005 from ${iso}`, 2005, date.getFullYear());

    // 1997-W01 or 1997W01
    iso = '2005-W22';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2005 from ${iso}`, 2005, date.getFullYear());
    assertEquals(`Got May from ${iso}`, 4, date.getMonth());
    assertEquals(`Got 30th from ${iso}`, 30, date.getDate());

    iso = '2005W22';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2005 from ${iso}`, 2005, date.getFullYear());
    assertEquals(`Got May from ${iso}`, 4, date.getMonth());
    assertEquals(`Got 30th from ${iso}`, 30, date.getDate());

    // 1997-W01-2 or 1997W012
    iso = '2005-W22-4';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2005 from ${iso}`, 2005, date.getFullYear());
    assertEquals(`Got June from ${iso}`, 5, date.getMonth());
    assertEquals(`Got 2nd from ${iso}`, 2, date.getDate());

    iso = '2005W224';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2005 from ${iso}`, 2005, date.getFullYear());
    assertEquals(`Got June from ${iso}`, 5, date.getMonth());
    assertEquals(`Got 2nd from ${iso}`, 2, date.getDate());

    iso = '2004-W53-6';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2005 from ${iso}`, 2005, date.getFullYear());
    assertEquals(`Got January from ${iso}`, 0, date.getMonth());
    assertEquals(`Got 1st from ${iso}`, 1, date.getDate());

    iso = '2004-W53-7';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2005 from ${iso}`, 2005, date.getFullYear());
    assertEquals(`Got January from ${iso}`, 0, date.getMonth());
    assertEquals(`Got 2nd from ${iso}`, 2, date.getDate());

    iso = '2005-W52-6';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2005 from ${iso}`, 2005, date.getFullYear());
    assertEquals(`Got December from ${iso}`, 11, date.getMonth());
    assertEquals(`Got 31st from ${iso}`, 31, date.getDate());

    // both years 2007 start with the same day
    iso = '2007-W01-1';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2007 from ${iso}`, 2007, date.getFullYear());
    assertEquals(`Got January from ${iso}`, 0, date.getMonth());
    assertEquals(`Got 1st from ${iso}`, 1, date.getDate());

    iso = '2007-W52-7';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2007 from ${iso}`, 2007, date.getFullYear());
    assertEquals(`Got December from ${iso}`, 11, date.getMonth());
    assertEquals(`Got 30th from ${iso}`, 30, date.getDate());

    iso = '2008-W01-1';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2007 from ${iso}`, 2007, date.getFullYear());
    assertEquals(`Got December from ${iso}`, 11, date.getMonth());
    assertEquals(`Got 31st from ${iso}`, 31, date.getDate());

    // Gregorian year 2008 is a leap year,
    // ISO year 2008 is 2 days shorter:
    // 1 day longer at the start, 3 days shorter at the end
    iso = '2008-W01-2';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2008 from ${iso}`, 2008, date.getFullYear());
    assertEquals(`Got Jan from ${iso}`, 0, date.getMonth());
    assertEquals(`Got 1st from ${iso}`, 1, date.getDate());

    // ISO year is three days into the previous gregorian year
    iso = '2009-W01-1';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2008 from ${iso}`, 2008, date.getFullYear());
    assertEquals(`Got December from ${iso}`, 11, date.getMonth());
    assertEquals(`Got 29th from ${iso}`, 29, date.getDate());

    iso = '2009-W01-3';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2008 from ${iso}`, 2008, date.getFullYear());
    assertEquals(`Got December from ${iso}`, 11, date.getMonth());
    assertEquals(`Got 31st from ${iso}`, 31, date.getDate());

    iso = '2009-W01-4';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2009 from ${iso}`, 2009, date.getFullYear());
    assertEquals(`Got January from ${iso}`, 0, date.getMonth());
    assertEquals(`Got 1st from ${iso}`, 1, date.getDate());

    // ISO year is three days into the next gregorian year
    iso = '2009-W53-4';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2009 from ${iso}`, 2009, date.getFullYear());
    assertEquals(`Got December from ${iso}`, 11, date.getMonth());
    assertEquals(`Got 31st from ${iso}`, 31, date.getDate());

    iso = '2009-W53-5';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2010 from ${iso}`, 2010, date.getFullYear());
    assertEquals(`Got January from ${iso}`, 0, date.getMonth());
    assertEquals(`Got 1st from ${iso}`, 1, date.getDate());

    iso = '2009-W53-6';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2010 from ${iso}`, 2010, date.getFullYear());
    assertEquals(`Got January from ${iso}`, 0, date.getMonth());
    assertEquals(`Got 2nd from ${iso}`, 2, date.getDate());

    iso = '2009-W53-7';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2010 from ${iso}`, 2010, date.getFullYear());
    assertEquals(`Got January from ${iso}`, 0, date.getMonth());
    assertEquals(`Got 3rd from ${iso}`, 3, date.getDate());

    iso = '2010-W01-1';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2010 from ${iso}`, 2010, date.getFullYear());
    assertEquals(`Got January from ${iso}`, 0, date.getMonth());
    assertEquals(`Got 4th from ${iso}`, 4, date.getDate());

    // Examples where the ISO year is three days into the previous gregorian
    // year

    // 1995-035 or 1995035
    iso = '2005-146';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2005 from ${iso}`, 2005, date.getFullYear());
    assertEquals(`Got May from ${iso}`, 4, date.getMonth());
    assertEquals(`Got 26th from ${iso}`, 26, date.getDate());

    iso = '2005146';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2005 from ${iso}`, 2005, date.getFullYear());
    assertEquals(`Got May from ${iso}`, 4, date.getMonth());
    assertEquals(`Got 26th from ${iso}`, 26, date.getDate());
  },

  testDate_fromIsoString() {
    // YYYY-MM-DD
    let iso = '2005-02-22';
    let date = DateDate.fromIsoString(iso);
    assertEquals(`Got 2005 from ${iso}`, 2005, date.getFullYear());
    assertEquals(`Got February from ${iso}`, 1, date.getMonth());
    assertEquals(`Got 22nd from ${iso}`, 22, date.getDate());

    // YYYY-MM-DDTHH:MM:SS
    iso = '2005-02-22T11:22:33';
    date = DateDate.fromIsoString(iso);
    assertNull(`Got null from ${iso}`, date);
  },

  testDateTime_fromIsoString() {
    // YYYY-MM-DD
    let iso = '2005-02-22';
    let date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2005 from ${iso}`, 2005, date.getFullYear());
    assertEquals(`Got February from ${iso}`, 1, date.getMonth());
    assertEquals(`Got 22nd from ${iso}`, 22, date.getDate());
    assertEquals(`Got 0 hours from ${iso}`, 0, date.getHours());
    assertEquals(`Got 0 minutes from ${iso}`, 0, date.getMinutes());
    assertEquals(`Got 0 seconds from ${iso}`, 0, date.getSeconds());

    // YYYY-MM-DDTHH:MM:SS
    iso = '2005-02-22T11:22:33';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2005 from ${iso}`, 2005, date.getFullYear());
    assertEquals(`Got February from ${iso}`, 1, date.getMonth());
    assertEquals(`Got 22nd from ${iso}`, 22, date.getDate());
    assertEquals(`Got 11 hours from ${iso}`, 11, date.getHours());
    assertEquals(`Got 22 minutes from ${iso}`, 22, date.getMinutes());
    assertEquals(`Got 33 seconds from ${iso}`, 33, date.getSeconds());

    // YYYY-MM-DDTHH:MM:SS+03:00
    iso = '2005-02-22T11:22:33+03:00';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2005 from ${iso}`, 2005, date.getUTCFullYear());
    assertEquals(`Got February from ${iso}`, 1, date.getUTCMonth());
    assertEquals(`Got 22nd from ${iso}`, 22, date.getUTCDate());
    assertEquals(`Got 08 hours from ${iso}`, 8, date.getUTCHours());
    assertEquals(`Got 22 minutes from ${iso}`, 22, date.getUTCMinutes());
    assertEquals(`Got 33 seconds from ${iso}`, 33, date.getUTCSeconds());

    // On a DST boundary, using a UTC timestamp
    iso = '2019-03-10T11:22:33Z';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2019 from ${iso}`, 2019, date.getUTCFullYear());
    assertEquals(`Got March from ${iso}`, 2, date.getUTCMonth());
    assertEquals(`Got 10th from ${iso}`, 10, date.getUTCDate());
    assertEquals(`Got 11 hours from ${iso}`, 11, date.getUTCHours());
    assertEquals(`Got 22 minutes from ${iso}`, 22, date.getUTCMinutes());
    assertEquals(`Got 33 seconds from ${iso}`, 33, date.getUTCSeconds());

    // Before a leap day in year 1 BC
    iso = '0000-01-02T11:22:33Z';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 0 from ${iso}`, 0, date.getUTCFullYear());
    assertEquals(`Got January from ${iso}`, 0, date.getUTCMonth());
    assertEquals(`Got 2nd from ${iso}`, 2, date.getUTCDate());
    assertEquals(`Got 11 hours from ${iso}`, 11, date.getUTCHours());
    assertEquals(`Got 22 minutes from ${iso}`, 22, date.getUTCMinutes());
    assertEquals(`Got 33 seconds from ${iso}`, 33, date.getUTCSeconds());

    // After a leap day in year 4 AD
    iso = '0004-03-04T11:22:33Z';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 4 from ${iso}`, 4, date.getUTCFullYear());
    assertEquals(`Got March from ${iso}`, 2, date.getUTCMonth());
    assertEquals(`Got 4th from ${iso}`, 4, date.getUTCDate());
    assertEquals(`Got 11 hours from ${iso}`, 11, date.getUTCHours());
    assertEquals(`Got 22 minutes from ${iso}`, 22, date.getUTCMinutes());
    assertEquals(`Got 33 seconds from ${iso}`, 33, date.getUTCSeconds());

    // Parsing ISO string in local time zone.
    iso = '2019-04-01T01:00:00';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2019 from ${iso}`, 2019, date.getFullYear());
    assertEquals(`Got April from ${iso}`, 3, date.getMonth());
    assertEquals(`Got 1st from ${iso}`, 1, date.getDate());
    assertEquals(`Got 01 hours from ${iso}`, 1, date.getHours());
    assertEquals(`Got 00 minutes from ${iso}`, 0, date.getMinutes());
    assertEquals(`Got 00 seconds from ${iso}`, 0, date.getSeconds());

    // Parsing ISO string in local time zone.
    iso = '2019-03-31T23:59:59';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2019 from ${iso}`, 2019, date.getFullYear());
    assertEquals(`Got March from ${iso}`, 2, date.getMonth());
    assertEquals(`Got 31st from ${iso}`, 31, date.getDate());
    assertEquals(`Got 23 hours from ${iso}`, 23, date.getHours());
    assertEquals(`Got 59 minutes from ${iso}`, 59, date.getMinutes());
    assertEquals(`Got 59 seconds from ${iso}`, 59, date.getSeconds());

    // Parsing ISO string at month boundary.
    iso = '2019-04-01T00:00:01Z';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2019 from ${iso}`, 2019, date.getUTCFullYear());
    assertEquals(`Got April from ${iso}`, 3, date.getUTCMonth());
    assertEquals(`Got 1st from ${iso}`, 1, date.getUTCDate());
    assertEquals(`Got 00 hours from ${iso}`, 0, date.getUTCHours());
    assertEquals(`Got 00 minutes from ${iso}`, 0, date.getUTCMinutes());
    assertEquals(`Got 01 seconds from ${iso}`, 1, date.getUTCSeconds());

    // Parsing ISO string at month boundary.
    iso = '2019-03-31T23:59:59Z';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2019 from ${iso}`, 2019, date.getUTCFullYear());
    assertEquals(`Got March from ${iso}`, 2, date.getUTCMonth());
    assertEquals(`Got 31st from ${iso}`, 31, date.getUTCDate());
    assertEquals(`Got 23 hours from ${iso}`, 23, date.getUTCHours());
    assertEquals(`Got 59 minutes from ${iso}`, 59, date.getUTCMinutes());
    assertEquals(`Got 59 seconds from ${iso}`, 59, date.getUTCSeconds());

    // Parsing ISO string with differing UTC date.
    iso = '2019-03-31T23:00:00-02:00';
    date = DateTime.fromIsoString(iso);
    assertEquals(`Got 2019 from ${iso}`, 2019, date.getUTCFullYear());
    assertEquals(`Got April from ${iso}`, 3, date.getUTCMonth());
    assertEquals(`Got 1st from ${iso}`, 1, date.getUTCDate());
    assertEquals(`Got 01 hours from ${iso}`, 1, date.getUTCHours());
    assertEquals(`Got 00 minutes from ${iso}`, 0, date.getUTCMinutes());
    assertEquals(`Got 00 seconds from ${iso}`, 0, date.getUTCSeconds());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  test_setIso8601TimeOnly_() {
    // 23:59:59
    let d = new DateTime(0, 0);
    let iso = '18:46:39';
    assertTrue(
        `parsed ${iso}`, googRequiredGoogDate.setIso8601TimeOnly_(d, iso));
    assertEquals(`Got 18 hours from ${iso}`, 18, d.getHours());
    assertEquals(`Got 46 minutes from ${iso}`, 46, d.getMinutes());
    assertEquals(`Got 39 seconds from ${iso}`, 39, d.getSeconds());

    // 235959
    d = new DateTime(0, 0);
    iso = '184639';
    assertTrue(
        `parsed ${iso}`, googRequiredGoogDate.setIso8601TimeOnly_(d, iso));
    assertEquals(`Got 18 hours from ${iso}`, 18, d.getHours());
    assertEquals(`Got 46 minutes from ${iso}`, 46, d.getMinutes());
    assertEquals(`Got 39 seconds from ${iso}`, 39, d.getSeconds());

    // 23:59, 2359, or 23
    d = new DateTime(0, 0);
    iso = '18:46';
    assertTrue(
        `parsed ${iso}`, googRequiredGoogDate.setIso8601TimeOnly_(d, iso));
    assertEquals(`Got 18 hours from ${iso}`, 18, d.getHours());
    assertEquals(`Got 46 minutes from ${iso}`, 46, d.getMinutes());

    d = new DateTime(0, 0);
    iso = '1846';
    assertTrue(
        `parsed ${iso}`, googRequiredGoogDate.setIso8601TimeOnly_(d, iso));
    assertEquals(`Got 18 hours from ${iso}`, 18, d.getHours());
    assertEquals(`Got 46 minutes from ${iso}`, 46, d.getMinutes());

    d = new DateTime(0, 0);
    iso = '18';
    assertTrue(
        `parsed ${iso}`, googRequiredGoogDate.setIso8601TimeOnly_(d, iso));
    assertEquals(`Got 18 hours from ${iso}`, 18, d.getHours());

    d = new DateTime(0, 0);
    iso = '18463';
    assertFalse(
        `failed to parse ${iso}`,
        googRequiredGoogDate.setIso8601TimeOnly_(d, iso));
    assertTrue('date did not change', d.equals(new DateTime(0, 0)));

    // 23:59:59.9942 or 235959.9942
    d = new DateTime(0, 0);
    iso = '18:46:39.9942';
    assertTrue(
        `parsed ${iso}`, googRequiredGoogDate.setIso8601TimeOnly_(d, iso));
    assertEquals(`Got 18 hours from ${iso}`, 18, d.getHours());
    assertEquals(`Got 46 minutes from ${iso}`, 46, d.getMinutes());
    assertEquals(`Got 39 seconds from ${iso}`, 39, d.getSeconds());

    assertEquals(`Got 994 milliseconds from ${iso}`, 994, d.getMilliseconds());

    d = new DateTime(0, 0);
    iso = '184639.9942';
    assertTrue(
        `parsed ${iso}`, googRequiredGoogDate.setIso8601TimeOnly_(d, iso));
    assertEquals(`Got 18 hours from ${iso}`, 18, d.getHours());
    assertEquals(`Got 46 minutes from ${iso}`, 46, d.getMinutes());
    assertEquals(`Got 39 seconds from ${iso}`, 39, d.getSeconds());
    // Other browsers, including WebKit on Windows, return integers.
    assertEquals(`Got 994 milliseconds from ${iso}`, 994, d.getMilliseconds());

    // 1995-02-04 24:00 = 1995-02-05 00:00
    // timezone tests
    const offset = new Date().getTimezoneOffset() / 60;
    d = new DateTime(0, 0);
    iso = '18:46:39+07:00';
    assertTrue(
        `parsed ${iso}`, googRequiredGoogDate.setIso8601TimeOnly_(d, iso));
    assertEquals(`Got an 11-hour GMT offset from ${iso}`, 11, d.getUTCHours());

    d = new DateTime(0, 0);
    iso = '18:46:39+00:00';
    assertTrue(
        `parsed ${iso}`, googRequiredGoogDate.setIso8601TimeOnly_(d, iso));
    assertEquals(`Got an 18-hour GMT offset from ${iso}`, 18, d.getUTCHours());

    d = new DateTime(0, 0);
    iso = '16:46:39-07:00';
    assertTrue(
        `parsed ${iso}`, googRequiredGoogDate.setIso8601TimeOnly_(d, iso));
    assertEquals(`Got a 23-hour GMT offset from ${iso}`, 23, d.getUTCHours());
  },

  testDateIntervalAdd() {
    // -1m, cross year boundary
    let d = new DateDate(2007, month.JAN, 1);
    d.add(new Interval(Interval.MONTHS, -1));
    assertEquals('2007-01-01 - 1m = 2006-12-01', '20061201', d.toIsoString());

    // +1y2m3d
    d = new DateDate(2006, month.JAN, 1);
    d.add(new Interval(1, 2, 3));
    assertEquals(
        '2006-01-01 + 1y2m3d = 2007-03-04', '20070304', d.toIsoString());

    // -1y2m3d (negative interval)
    d = new DateDate(2007, month.MAR, 4);
    d.add(new Interval(-1, -2, -3));
    assertEquals(
        '2007-03-04 - 1y2m3d = 2006-01-01', '20060101', d.toIsoString());

    // 2007-12-30 + 3d (roll over to next year)
    d = new DateDate(2007, month.DEC, 30);
    d.add(new Interval(Interval.DAYS, 3));
    assertEquals('2007-12-30 + 3d = 2008-01-02', '20080102', d.toIsoString());

    // 2004-03-01 - 1d (handle leap year)
    d = new DateDate(2004, month.MAR, 1);
    d.add(new Interval(Interval.DAYS, -1));
    assertEquals('2004-03-01 - 1d = 2004-02-29', '20040229', d.toIsoString());

    // 2004-02-29 + 1y (stays at end of Feb, doesn't roll to Mar)
    d = new DateDate(2004, month.FEB, 29);
    d.add(new Interval(Interval.YEARS, 1));
    assertEquals('2004-02-29 + 1y = 2005-02-28', '20050228', d.toIsoString());

    // 2004-02-29 - 1y (stays at end of Feb, doesn't roll to Mar)
    d = new DateDate(2004, month.FEB, 29);
    d.add(new Interval(Interval.YEARS, -1));
    assertEquals('2004-02-29 - 1y = 2003-02-28', '20030228', d.toIsoString());

    // 2003-02-28 + 1y (stays at 28, doesn't go to leap year end of Feb)
    d = new DateDate(2003, month.FEB, 28);
    d.add(new Interval(Interval.YEARS, 1));
    assertEquals('2003-02-28 + 1y = 2004-02-28', '20040228', d.toIsoString());

    // 2005-02-28 - 1y (stays at 28, doesn't go to leap year end of Feb)
    d = new DateDate(2005, month.FEB, 28);
    d.add(new Interval(Interval.YEARS, -1));
    assertEquals('2005-02-28 - 1y = 2004-02-28', '20040228', d.toIsoString());

    // 2003-01-31 + 1y (stays at end of Jan, standard case)
    d = new DateDate(2003, month.JAN, 31);
    d.add(new Interval(Interval.YEARS, 1));
    assertEquals('2003-01-31 + 1y = 2004-01-31', '20040131', d.toIsoString());

    // 2005-01-31 - 1y (stays at end of Jan, standard case)
    d = new DateDate(2005, month.JAN, 31);
    d.add(new Interval(Interval.YEARS, -1));
    assertEquals('2005-01-31 - 1y = 2004-01-31', '20040131', d.toIsoString());

    // 2006-01-31 + 1m (stays at end of Feb, doesn't roll to Mar, non-leap-year)
    d = new DateDate(2006, month.JAN, 31);
    d.add(new Interval(Interval.MONTHS, 1));
    assertEquals('2006-01-31 + 1m = 2006-02-28', '20060228', d.toIsoString());

    // 2004-02-29 + 1m (stays at 29, standard case)
    d = new DateDate(2004, month.FEB, 29);
    d.add(new Interval(Interval.MONTHS, +1));
    assertEquals('2004-02-29 + 1m = 2004-03-29', '20040329', d.toIsoString());

    // 2004-02-29 - 1m (stays at 29, standard case)
    d = new DateDate(2004, month.FEB, 29);
    d.add(new Interval(Interval.MONTHS, -1));
    assertEquals('2004-02-29 - 1m = 2004-01-29', '20040129', d.toIsoString());

    // 2004-01-30 + 1m (snaps to Feb 29)
    d = new DateDate(2004, month.JAN, 30);
    d.add(new Interval(Interval.MONTHS, 1));
    assertEquals('2004-01-30 + 1m = 2004-02-29', '20040229', d.toIsoString());

    // 2004-03-30 - 1m (snaps to Feb 29)
    d = new DateDate(2004, month.MAR, 30);
    d.add(new Interval(Interval.MONTHS, -1));
    assertEquals('2004-03-30 - 1m = 2004-02-29', '20040229', d.toIsoString());

    // 2004-03-30 + 1m (stays at 30, standard case)
    d = new DateDate(2004, month.MAR, 30);
    d.add(new Interval(Interval.MONTHS, 1));
    assertEquals('2004-03-30 + 1m = 2004-04-30', '20040430', d.toIsoString());

    // 2008-10-30 + 1d (roll over to 31).
    d = new DateDate(2008, month.OCT, 30);
    d.add(new Interval(Interval.DAYS, 1));
    assertEquals('2008-10-30 + 1d = 2008-10-31', '20081031', d.toIsoString());

    // 2008-10-31 + 1d (roll over to November, not December).
    d = new DateDate(2008, month.OCT, 31);
    d.add(new Interval(Interval.DAYS, 1));
    assertEquals('2008-10-31 + 1d = 2008-11-01', '20081101', d.toIsoString());

    // 2008-10-17 + 1d (Brasilia dst).
    d = new DateDate(2008, month.OCT, 17);
    d.add(new Interval(Interval.DAYS, 1));
    assertEquals('2008-10-17 + 1d = 2008-10-18', '20081018', d.toIsoString());

    // 2008-10-18 + 1d (Brasilia dst).
    d = new DateDate(2008, month.OCT, 18);
    d.add(new Interval(Interval.DAYS, 1));
    assertEquals('2008-10-18 + 1d = 2008-10-19', '20081019', d.toIsoString());

    // 2008-10-19 + 1d (Brasilia dst).
    d = new DateDate(2008, month.OCT, 19);
    d.add(new Interval(Interval.DAYS, 1));
    assertEquals('2008-10-19 + 1d = 2008-10-20', '20081020', d.toIsoString());

    // 2008-02-16 + 1d (Brasilia dst).
    d = new DateDate(2008, month.FEB, 16);
    d.add(new Interval(Interval.DAYS, 1));
    assertEquals('2008-02-16 + 1d = 2008-02-17', '20080217', d.toIsoString());

    // 2008-02-17 + 1d (Brasilia dst).
    d = new DateDate(2008, month.FEB, 17);
    d.add(new Interval(Interval.DAYS, 1));
    assertEquals('2008-02-17 + 1d = 2008-02-18', '20080218', d.toIsoString());

    // Javascript Date objects have special behavior for years 0-99
    d = new DateDate(0, month.MAR, 3);
    d.add(new Interval(Interval.DAYS, -1));
    // Note that ISO strings allow dropping leading zeros on the year.
    assertEquals('0000-3-3 - 1d = 0000-03-02', '00302', d.toIsoString());

    // Javascript Date objects have special behavior for years 0-99
    d = new DateDate(99, month.OCT, 31);
    d.add(new Interval(Interval.DAYS, -1));
    // Note that ISO strings allow dropping leading zeros on the year.
    assertEquals('0099-10-31 - 1d = 0099-10-30', '991030', d.toIsoString());

    // Javascript Date objects have special behavior for years 0-99; -1 is just
    // outside that range.
    d = new DateDate(-1, month.JUN, 10);
    d.add(new Interval(Interval.DAYS, 1));
    // Note that ISO strings allow dropping leading zeros on the year.
    assertEquals('-0001-06-10 + 1d = -0001-06-11', '-10611', d.toIsoString());

    // Javascript Date objects have special behavior for years 0-99; 100 is just
    // outside that range.
    d = new DateDate(100, month.JUN, 10);
    d.add(new Interval(Interval.DAYS, 1));
    // Note that ISO strings allow dropping leading zeros on the year.
    assertEquals('0100-06-10 + 1d = 0100-06-11', '1000611', d.toIsoString());

    // Javascript Date objects have special behavior for years 0-99; add an
    // interval that pushes the original date into the range from across the
    // bottom boundary.
    d = new DateDate(-1, month.DEC, 20);
    d.add(new Interval(Interval.DAYS, 12));
    // Note that ISO strings allow dropping leading zeros on the year.
    assertEquals('-0001-12-20 + 12d = 0000-01-01', '00101', d.toIsoString());

    // Javascript Date objects have special behavior for years 0-99; add an
    // interval that pushes the original date into the range from across the top
    // boundary.
    d = new DateDate(100, month.JAN, 3);
    d.add(new Interval(Interval.DAYS, -3));
    // Note that ISO strings allow dropping leading zeros on the year.
    assertEquals('0100-01-03 - 3d = 0099-12-31', '991231', d.toIsoString());

    // Javascript Date objects have special behavior for years 0-99; add an
    // interval that pushes the original, special date outside the range across
    // the bottom boundary.
    d = new DateDate(0, month.JAN, 2);
    d.add(new Interval(Interval.DAYS, -2));
    // Note that ISO strings allow dropping leading zeros on the year.
    assertEquals('0000-01-02 - 2d = -0001-12-31', '-11231', d.toIsoString());

    // Javascript Date objects have special behavior for years 0-99; add an
    // interval that pushes the original, special date outside the range across
    // the top boundary.
    d = new DateDate(99, month.DEC, 30);
    d.add(new Interval(Interval.DAYS, 2));
    // Note that ISO strings allow dropping leading zeros on the year.
    assertEquals('0099-12-30 + 2d = 0100-01-01', '1000101', d.toIsoString());
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testDateEquals() {
    let d1 = new DateDate(2004, month.MAR, 1);
    let d2 = new DateDate(2004, month.MAR, 1);
    assertFalse('d1 != null', d1.equals(null));
    assertFalse('d2 != undefined', d2.equals(undefined));
    assertTrue('d1 == d2', d1.equals(d2));
    assertTrue('d2 == d1', d2.equals(d1));

    d1 = new DateDate(2005, month.MAR, 1);
    d2 = new DateDate(2004, month.MAR, 1);
    assertFalse('different year', d1.equals(d2));

    d1 = new DateDate(2004, month.FEB, 1);
    d2 = new DateDate(2004, month.MAR, 1);
    assertFalse('different month', d1.equals(d2));

    d1 = new DateDate(2004, month.MAR, 2);
    d2 = new DateDate(2004, month.MAR, 1);
    assertFalse('different date', d1.equals(d2));

    // try passing in DateTime, time fields should be ignored
    d1 = new DateDate(2004, month.MAR, 1);
    d2 = new DateTime(2004, month.MAR, 1, 12, 30, 30);
    assertTrue('using goog.date.DateTime, same date', d1.equals(d2));
  },

  testDateTimeConstructor() {
    let date = new DateTime(2001, 2, 3, 4, 5, 6, 7);
    assertEquals(2001, date.getFullYear());
    assertEquals(2, date.getMonth());
    assertEquals(3, date.getDate());
    assertEquals(4, date.getHours());
    assertEquals(5, date.getMinutes());
    assertEquals(6, date.getSeconds());
    assertEquals(7, date.getMilliseconds());
    assertEquals(new Date(2001, 2, 3, 4, 5, 6, 7).getTime(), date.getTime());

    date = new DateTime(2001);
    assertEquals(2001, date.getFullYear());
    assertEquals(0, date.getMonth());
    assertEquals(1, date.getDate());
    assertEquals(0, date.getHours());
    assertEquals(0, date.getMinutes());
    assertEquals(0, date.getSeconds());
    assertEquals(0, date.getMilliseconds());

    date = new DateTime(new Date(2001, 2, 3, 4, 5, 6, 7));
    assertEquals(2001, date.getFullYear());
    assertEquals(2, date.getMonth());
    assertEquals(3, date.getDate());
    assertEquals(4, date.getHours());
    assertEquals(5, date.getMinutes());
    assertEquals(6, date.getSeconds());
    assertEquals(7, date.getMilliseconds());
    assertEquals(new Date(2001, 2, 3, 4, 5, 6, 7).getTime(), date.getTime());

    goog.now = () => new Date(2001, 2, 3, 4).getTime();
    date = new DateTime();
    assertEquals(2001, date.getFullYear());
    assertEquals(2, date.getMonth());
    assertEquals(3, date.getDate());
    assertEquals(4, date.getHours());
    assertEquals(0, date.getMinutes());
    assertEquals(0, date.getSeconds());
    assertEquals(0, date.getMilliseconds());
    assertEquals(new Date(2001, 2, 3, 4).getTime(), date.getTime());

    date = new DateTime(new Date('October 2, 2002 8:00:00'));
    assertEquals(2002, date.getFullYear());
    assertEquals(9, date.getMonth());
    assertEquals(2, date.getDate());
    assertEquals(8, date.getHours());
    assertEquals(0, date.getMinutes());
    assertEquals(0, date.getSeconds());
    assertEquals(0, date.getMilliseconds());
    assertEquals(new Date(2002, 9, 2, 8).getTime(), date.getTime());
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testDateTimeEquals() {
    let d1 = new DateTime(2004, month.MAR, 1, 12, 30, 30);
    let d2 = new DateTime(2004, month.MAR, 1, 12, 30, 30);
    assertTrue('d1 == d2', d1.equals(d2));
    assertTrue('d2 == d1', d2.equals(d1));

    d1 = new DateTime(2007, month.JAN, 1);
    d2 = new DateTime();  // today
    assertFalse('different date', d1.equals(d2));

    d1 = new DateTime(2004, month.MAR, 1);
    d2 = new DateTime(2004, month.MAR, 1, 12);
    assertFalse('different hours', d1.equals(d2));

    d1 = new DateTime(2004, month.MAR, 1, 12, 29, 30);
    d2 = new DateTime(2004, month.MAR, 1, 12, 30, 30);
    assertFalse('different minutes', d1.equals(d2));

    d1 = new DateTime(2004, month.MAR, 1, 12, 30, 29);
    d2 = new DateTime(2004, month.MAR, 1, 12, 30, 30);
    assertFalse('different seconds', d1.equals(d2));

    d1 = new DateTime(2004, month.MAR, 1, 12, 30, 30, 500);
    d2 = new DateTime(2004, month.MAR, 1, 12, 30, 30, 500);
    assertTrue('same milliseconds', d1.equals(d2));

    d1 = new DateTime(2004, month.MAR, 1, 12, 30, 30, 499);
    d2 = new DateTime(2004, month.MAR, 1, 12, 30, 30, 500);
    assertFalse('different milliseconds', d1.equals(d2));

    // try milliseconds again, this time comparing against a native Date
    d1 = new DateTime(2004, month.MAR, 1, 12, 30, 30);
    d2 = new Date(2004, 2, 1, 12, 30, 30, 999);
    assertFalse('different milliseconds, native Date', d1.equals(d2));

    // pass in a goog.date.Date rather than a goog.date.DateTime
    d1 = new DateTime(2004, month.MAR, 1, 12, 30, 30);
    d2 = new DateDate(2004, month.MAR, 1);
    assertFalse('using goog.date.Date, different times', d1.equals(d2));

    d1 = new DateTime(2004, month.MAR, 1, 0, 0, 0);
    d2 = new DateDate(2004, month.MAR, 1);
    assertTrue('using goog.date.Date, same time (midnight)', d1.equals(d2));
  },

  testIntervalIsZero() {
    assertTrue('zero interval', new Interval().isZero());
    const i = new Interval(0, 0, 1, -24, 0, 0);
    assertFalse('1 day minus 24 hours is not considered zero', i.isZero());
  },

  testIntervalGetInverse() {
    let i1 = new Interval(Interval.DAYS, -1);
    let i2 = i1.getInverse();

    let d = new DateDate(2004, month.MAR, 1);
    d.add(i1);
    d.add(i2);
    let label = '2004-03-01 - 1d + 1d = 2004-03-01';
    assertEquals(label, d.toIsoString(), '20040301');

    i1 = new Interval(1, 2, 3);
    i2 = i1.getInverse();

    d = new DateDate(2004, month.MAR, 1);
    d.add(i1);
    d.add(i2);
    label = '2004-03-01 - 1y2m3d + 1y2m3d = 2004-03-01';
    assertEquals(label, d.toIsoString(), '20040301');
  },

  testIntervalTimes() {
    const i = new Interval(1, 2, 3, 4, 5, 6);
    const expected = new Interval(2, 4, 6, 8, 10, 12);
    assertTrue('double interval', expected.equals(i.times(2)));
  },

  testIntervalEquals() {
    let i1 = new Interval(Interval.DAYS, -1);
    let i2 = new Interval(Interval.DAYS, -1);
    assertTrue('-1d == -1d, aka i1 == i2', i1.equals(i2));
    assertTrue('-1d == -1d, aka i2 == i1', i2.equals(i1));

    i1 = new Interval(Interval.DAYS, -1);
    i2 = new Interval(Interval.DAYS, 1);
    assertFalse('-1d != +1d, aka i1 == i2', i1.equals(i2));
    assertFalse('-1d != +1d, aka i2 == i1', i2.equals(i1));

    i1 = new Interval(0, 3);  // Three months
    i2 = new Interval(Interval.MONTHS, 3);
    assertTrue('3m == 3m, aka i1 == i2', i1.equals(i2));
    assertTrue('3m == 3m, aka i2 == i1', i2.equals(i1));

    // 1 year, 6 months, 15 days, 12 hours, 30 minutes, 30 seconds
    i1 = new Interval(1, 6, 15, 12, 30, 30);
    i2 = new Interval(1, 6, 15, 12, 30, 30);
    assertTrue('1y6m15d12h30M30s == 1y6m15d12h30M30s', i1.equals(i2));
    assertTrue('1y6m15d12h30M30s == 1y6m15d12h30M30s', i2.equals(i1));
  },

  testIntervalIntervalAdd() {
    let i1 = new Interval(1, 6, 15, 12, 30, 30);
    let i2 = new Interval(0, 3, 20, 10, 50, -25);
    i1.add(i2);
    assertTrue('i1 + i2', i1.equals(new Interval(1, 9, 35, 22, 80, 5)));

    i1 = new Interval(1, 6, 15, 12, 30, 30);
    i2 = new Interval(0, 3, 20, 10, 50, -25);
    i2.add(i1);
    assertTrue('i2 + i1', i2.equals(new Interval(1, 9, 35, 22, 80, 5)));

    i1 = new Interval(1, 6, 15, 12, 30, 30);
    i2 = i1.getInverse();
    i1.add(i2);
    assertTrue('i1 + (-i1)', i1.equals(new Interval()));

    i1 = new Interval(Interval.DAYS, 1);
    i2 = new Interval(0, -2, -2);
    i1.add(i2);
    assertTrue('1d + (-2m-2d)', i1.equals(new Interval(0, -2, -1)));
  },

  testIsoDuration() {
    const interval1 = new Interval(123, 456, 678, 11, 12, 455.5);
    const duration1 = 'P123Y456M678DT11H12M455.5S';
    assertTrue(
        'parse full duration',
        interval1.equals(Interval.fromIsoString(duration1)));
    assertEquals('create full duration', duration1, interval1.toIsoString());

    const interval2 = new Interval(123);
    const duration2 = 'P123Y';
    const duration2v = 'P123Y0M0DT0H0M0S';
    assertTrue(
        'parse year', interval2.equals(Interval.fromIsoString(duration2)));
    assertEquals('create year', duration2, interval2.toIsoString());
    assertEquals(
        'create year, verbose', duration2v, interval2.toIsoString(true));

    const interval3 = new Interval(0, 0, 0, 11, 12, 40);
    const duration3 = 'PT11H12M40S';
    const duration3v = 'P0Y0M0DT11H12M40S';
    assertTrue(
        'parse time duration',
        interval3.equals(Interval.fromIsoString(duration3)));
    assertEquals('create time duration', duration3, interval3.toIsoString());
    assertEquals(
        'create time duration, verbove', duration3v,
        interval3.toIsoString(true));

    const interval4 = new Interval(7, 8, 9, 1, 2, 4);
    const duration4 = 'P7Y8M9DT1H2M4S';
    assertTrue(
        'parse one-digit duration',
        interval4.equals(Interval.fromIsoString(duration4)));
    assertEquals(
        'create one-digit duration', duration4, interval4.toIsoString());

    const interval5 = new Interval(-123, -456, -678, -11, -12, -455.5);
    const duration5 = '-P123Y456M678DT11H12M455.5S';
    assertTrue(
        'parse full negative duration',
        interval5.equals(Interval.fromIsoString(duration5)));
    assertEquals(
        'create full negative duration', duration5, interval5.toIsoString());

    const interval6 = new Interval(0, 0, -1);
    const duration6 = '-P1D';
    const duration6v = '-P0Y0M1DT0H0M0S';
    assertTrue(
        'parse partial negative duration',
        interval6.equals(Interval.fromIsoString(duration6)));
    assertEquals(
        'create partial negative duration', duration6, interval6.toIsoString());
    assertEquals(
        'create partial negative duration, verbose', duration6v,
        interval6.toIsoString(true));

    const interval7 = new Interval(0, 0, 9, 0, 0, 4);
    const duration7 = 'P9DT4S';
    const duration7v = 'P0Y0M9DT0H0M4S';
    assertTrue(
        'parse partial one-digit duration',
        interval7.equals(Interval.fromIsoString(duration7)));
    assertTrue(
        'parse partial one-digit duration, verbose',
        interval7.equals(Interval.fromIsoString(duration7v)));
    assertEquals(
        'create partial one-digit duration', duration7,
        interval7.toIsoString());
    assertEquals(
        'create partial one-digit duration, verbose', duration7v,
        interval7.toIsoString(true));

    const interval8 = new Interval(1, -1, 1, -1, 1, -1);
    assertNull('create mixed sign duration', interval8.toIsoString());

    const duration9 = '1Y1M1DT1H1M1S';
    assertNull('missing P', Interval.fromIsoString(duration9));

    const duration10 = 'P1Y1M1D1H1M1S';
    assertNull('missing T', Interval.fromIsoString(duration10));

    const duration11 = 'P1Y1M1DT';
    assertNull('extra T', Interval.fromIsoString(duration11));

    const duration12 = 'PT.5S';
    assertNull(
        'invalid seconds, missing integer part',
        Interval.fromIsoString(duration12));

    const duration13 = 'PT1.S';
    assertNull(
        'invalid seconds, missing fractional part',
        Interval.fromIsoString(duration13));
  },

  testGetTotalSeconds() {
    const duration = new Interval(0, 0, 2, 3, 4, 5);
    assertEquals(
        'seconds in 2d3h4m5s', 2 * 86400 + 3 * 3600 + 4 * 60 + 5,
        duration.getTotalSeconds());
  },

  testIsDateLikeWithGoogDateTime() {
    const jsDate = new Date();
    const googDate = new DateTime();
    const string = 'foo';
    const number = 1;
    const nullVar = null;
    let notDefined;

    assertTrue('js Date should be date-like', goog.isDateLike(jsDate));
    assertTrue('goog Date should be date-like', goog.isDateLike(googDate));
    assertFalse('string should not be date-like', goog.isDateLike(string));
    assertFalse('number should not be date-like', goog.isDateLike(number));
    assertFalse('nullVar should not be date-like', goog.isDateLike(nullVar));
    assertFalse(
        'undefined should not be date-like', goog.isDateLike(notDefined));
  },

  testToUTCRfc3339String() {
    let date = DateTime.fromIsoString('19850412T232050Z');
    date.setUTCMilliseconds(52);
    assertEquals('1985-04-12T23:20:50.052Z', date.toUTCRfc3339String());
    assertNotEquals(
        'Diverges from ISO 8601', date.toUTCRfc3339String(),
        date.toUTCIsoString(true, true));

    date = DateTime.fromIsoString('19901231T235959Z');
    assertEquals('1990-12-31T23:59:59Z', date.toUTCRfc3339String());

    date = DateTime.fromIsoString('19370101T120027Z');
    date.setUTCMilliseconds(87);
    assertEquals('1937-01-01T12:00:27.087Z', date.toUTCRfc3339String());
  },

  testDateTimezone() {
    const d = new DateTime(2006, 1, 1, 12, 0, 0);
    d.add(new Interval(Interval.MINUTES, d.getTimezoneOffset()));
    const d2 = new DateTime(2006, 1, 1, 12, 0, 0);
    assertEquals(
        'Compensate for timezone and compare with UTC date/time',
        d.toIsoString(true), d2.toUTCIsoString(true));
  },

  testToUsTimeString() {
    const doPad = true;
    const doShowPm = true;
    const dontPad = false;
    const dontShowPm = false;

    // 12am
    let d = new DateTime(2007, 1, 14);
    assertEquals('12am test 1', '12:00 AM', d.toUsTimeString());
    assertEquals('12am test 2', '12:00 AM', d.toUsTimeString(doPad));
    assertEquals('12am test 3', '12:00 AM', d.toUsTimeString(dontPad));
    assertEquals('12am test 4', '12:00 AM', d.toUsTimeString(doPad, doShowPm));
    assertEquals('12am test 5', '00:00', d.toUsTimeString(doPad, dontShowPm));
    assertEquals(
        '12am test 6', '12:00 AM', d.toUsTimeString(dontPad, doShowPm));
    assertEquals('12am test 7', '0:00', d.toUsTimeString(dontPad, dontShowPm));

    // 9am
    d = new DateTime(2007, 1, 14, 9);
    assertEquals('9am test 1', '9:00 AM', d.toUsTimeString());
    assertEquals('9am test 2', '09:00 AM', d.toUsTimeString(doPad));
    assertEquals('9am test 3', '9:00 AM', d.toUsTimeString(dontPad));
    assertEquals('9am test 4', '09:00 AM', d.toUsTimeString(doPad, doShowPm));
    assertEquals('9am test 5', '09:00', d.toUsTimeString(doPad, dontShowPm));
    assertEquals('9am test 6', '9:00 AM', d.toUsTimeString(dontPad, doShowPm));
    assertEquals('9am test 7', '9:00', d.toUsTimeString(dontPad, dontShowPm));

    // 12pm
    d = new DateTime(2007, 1, 14, 12);
    assertEquals('12pm test 1', '12:00 PM', d.toUsTimeString());
    assertEquals('12pm test 2', '12:00 PM', d.toUsTimeString(doPad));
    assertEquals('12pm test 3', '12:00 PM', d.toUsTimeString(dontPad));
    assertEquals('12pm test 4', '12:00 PM', d.toUsTimeString(doPad, doShowPm));
    assertEquals('12pm test 5', '12:00', d.toUsTimeString(doPad, dontShowPm));
    assertEquals(
        '12pm test 6', '12:00 PM', d.toUsTimeString(dontPad, doShowPm));
    assertEquals('12pm test 7', '12:00', d.toUsTimeString(dontPad, dontShowPm));

    // 6 PM
    d = new DateTime(2007, 1, 14, 18);
    assertEquals('6 PM test 1', '6:00 PM', d.toUsTimeString());
    assertEquals('6 PM test 2', '06:00 PM', d.toUsTimeString(doPad));
    assertEquals('6 PM test 3', '6:00 PM', d.toUsTimeString(dontPad));
    assertEquals('6 PM test 4', '06:00 PM', d.toUsTimeString(doPad, doShowPm));
    assertEquals('6 PM test 5', '06:00', d.toUsTimeString(doPad, dontShowPm));
    assertEquals('6 PM test 6', '6:00 PM', d.toUsTimeString(dontPad, doShowPm));
    assertEquals('6 PM test 7', '6:00', d.toUsTimeString(dontPad, dontShowPm));

    // 6:01 PM
    d = new DateTime(2007, 1, 14, 18, 1);
    assertEquals('6:01 PM test 1', '6:01 PM', d.toUsTimeString());
    assertEquals('6:01 PM test 2', '06:01 PM', d.toUsTimeString(doPad));
    assertEquals('6:01 PM test 3', '6:01 PM', d.toUsTimeString(dontPad));
    assertEquals(
        '6:01 PM test 4', '06:01 PM', d.toUsTimeString(doPad, doShowPm));
    assertEquals(
        '6:01 PM test 5', '06:01', d.toUsTimeString(doPad, dontShowPm));
    assertEquals(
        '6:01 PM test 6', '6:01 PM', d.toUsTimeString(dontPad, doShowPm));
    assertEquals(
        '6:01 PM test 7', '6:01', d.toUsTimeString(dontPad, dontShowPm));

    // 6:35 PM
    d = new DateTime(2007, 1, 14, 18, 35);
    assertEquals('6:35 PM test 1', '6:35 PM', d.toUsTimeString());
    assertEquals('6:35 PM test 2', '06:35 PM', d.toUsTimeString(doPad));
    assertEquals('6:35 PM test 3', '6:35 PM', d.toUsTimeString(dontPad));
    assertEquals(
        '6:35 PM test 4', '06:35 PM', d.toUsTimeString(doPad, doShowPm));
    assertEquals(
        '6:35 PM test 5', '06:35', d.toUsTimeString(doPad, dontShowPm));
    assertEquals(
        '6:35 PM test 6', '6:35 PM', d.toUsTimeString(dontPad, doShowPm));
    assertEquals(
        '6:35 PM test 7', '6:35', d.toUsTimeString(dontPad, dontShowPm));

    // omit zero minutes
    d = new DateTime(2007, 1, 14, 18);
    assertEquals(
        'omit zero 1', '6:00 PM', d.toUsTimeString(dontPad, doShowPm, false));
    assertEquals(
        'omit zero 2', '6 PM', d.toUsTimeString(dontPad, doShowPm, true));

    // but don't omit zero minutes if not actually zero minutes
    d = new DateTime(2007, 1, 14, 18, 1);
    assertEquals(
        'omit zero 3', '6:01 PM', d.toUsTimeString(dontPad, doShowPm, false));
    assertEquals(
        'omit zero 4', '6:01 PM', d.toUsTimeString(dontPad, doShowPm, true));
  },

  testToIsoTimeString() {
    // 00:00
    let d = new DateTime(2007, 1, 14);
    assertEquals('00:00', '00:00:00', d.toIsoTimeString());

    // 09:00
    d = new DateTime(2007, 1, 14, 9);
    assertEquals('09:00', '09:00:00', d.toIsoTimeString());

    // 12:00
    d = new DateTime(2007, 1, 14, 12);
    assertEquals('12:00', '12:00:00', d.toIsoTimeString());

    // 18:00
    d = new DateTime(2007, 1, 14, 18);
    assertEquals('18:00', '18:00:00', d.toIsoTimeString());

    // 18:01
    d = new DateTime(2007, 1, 14, 18, 1);
    assertEquals('18:01', '18:01:00', d.toIsoTimeString());

    // 18:35
    d = new DateTime(2007, 1, 14, 18, 35);
    assertEquals('18:35', '18:35:00', d.toIsoTimeString());

    // 18:35:01
    d = new DateTime(2007, 1, 14, 18, 35, 1);
    assertEquals('18:35:01', '18:35:01', d.toIsoTimeString());

    // 18:35:11
    d = new DateTime(2007, 1, 14, 18, 35, 11);
    assertEquals('18:35:11', '18:35:11', d.toIsoTimeString());

    // 18:35:11 >> 18:35
    d = new DateTime(2007, 1, 14, 18, 35, 11);
    assertEquals('18:35:11 no secs', '18:35', d.toIsoTimeString(false));
  },

  testToXmlDateTimeString() {
    let d = new DateTime(2007, 1, 14);
    assertEquals('2007-02-14', '2007-02-14T00:00:00', d.toXmlDateTime());

    d = new DateTime(2007, 1, 14, 18, 35, 1);
    assertEquals(
        '2007-02-14, 8:35:01, timezone==undefined', '2007-02-14T18:35:01',
        d.toXmlDateTime());

    d = new DateTime(2007, 1, 14, 18, 35, 1);
    assertEquals(
        '2007-02-14, 8:35:01, timezone==false', '2007-02-14T18:35:01',
        d.toXmlDateTime(false));

    d = new DateTime(2007, 1, 14, 18, 35, 1);
    assertEquals(
        '2007-02-14, 8:35:01, timezone==true',
        '2007-02-14T18:35:01' + d.getTimezoneOffsetString(),
        d.toXmlDateTime(true));
  },

  testClone() {
    let d = new DateTime(2007, 1, 14, 18, 35, 1);
    let d2 = d.clone();
    assertTrue('datetimes equal', d.equals(d2));

    d = new DateTime(2007, 1, 14, 18, 35, 1, 310);
    d2 = d.clone();
    assertTrue('datetimes with milliseconds equal', d.equals(d2));

    d = new DateDate(2007, 1, 14);
    d2 = d.clone();
    assertTrue('dates equal', d.equals(d2));

    // 1 year, 6 months, 15 days, 12 hours, 30 minutes, 30 seconds
    let i = new Interval(1, 6, 15, 12, 30, 30);
    let i2 = i.clone();
    assertTrue('intervals equal', i.equals(i2));

    i = new Interval(Interval.DAYS, -1);
    i2 = i.clone();
    assertTrue('day intervals equal', i.equals(i2));

    // Brasilia dst
    d = new DateDate(2008, month.OCT, 18);
    d.add(new Interval(Interval.DAYS, 1));
    d2 = d.clone();
    assertTrue('dates equal', d.equals(d2));
  },

  testValueOf() {
    const date1 = new DateTime(2008, 11, 26, 15, 40, 0);
    const date2 = new DateDate(2008, 11, 27);
    const date3 = new DateTime(2008, 11, 26, 15, 40, 1);
    const nativeDate = new Date();
    nativeDate.setFullYear(2008, 11, 26);
    nativeDate.setHours(15, 40, 0, 0);
    assertEquals(date1.valueOf(), nativeDate.valueOf());
    assertFalse(date1 < date1);
    assertTrue(date1 <= date1);
    assertTrue(date1 < date2);
    assertTrue(date2 > date3);
  },

  testDateCompare() {
    // May 16th, 2011, 3:17:36.500
    const date1 = new DateTime(2011, month.MAY, 16, 15, 17, 36, 500);

    // May 16th, 2011, 3:17:36.501
    const date2 = new DateTime(2011, month.MAY, 16, 15, 17, 36, 501);

    // May 16th, 2011, 3:17:36.501
    const date3 = new DateTime(2011, month.MAY, 16, 15, 17, 36, 502);

    assertEquals(0, DateDate.compare(date1.clone(), date1.clone()));
    assertEquals(-1, DateDate.compare(date1, date2));
    assertEquals(1, DateDate.compare(date2, date1));

    const dates = [date2, date3, date1];
    dates.sort(DateDate.compare);
    assertArrayEquals(
        'Dates should be sorted in time.', [date1, date2, date3], dates);

    // Assert a known millisecond difference between two points in time.
    assertEquals(
        -19129478,
        DateDate.compare(
            new DateTime(1982, month.MAR, 12, 6, 48, 32, 354),
            new DateTime(1982, month.MAR, 12, 12, 7, 21, 832)));

    // Test dates before the year 0.  Dates are Talk Like a Pirate Day, and
    // Towel Day, 300 B.C. (and before pirates).

    const pirateDay = new DateDate(-300, month.SEP, 2);
    const towelDay = new DateDate(-300, month.MAY, 12);

    assertEquals(
        'Dates should be 113 days apart.', 113 * 24 * 60 * 60 * 1000,
        DateDate.compare(pirateDay, towelDay));
  },

  testDateCompareDateLikes() {
    const nativeDate = new Date(2011, 4, 16, 15, 17, 36, 500);
    const closureDate = new DateTime(2011, month.MAY, 16, 15, 17, 36, 500);

    assertEquals(0, DateDate.compare(nativeDate, closureDate));

    nativeDate.setMilliseconds(499);
    assertEquals(-1, DateDate.compare(nativeDate, closureDate));

    nativeDate.setMilliseconds(501);
    assertEquals(1, DateDate.compare(nativeDate, closureDate));
  },

  testIsMidnight() {
    assertTrue(new DateTime(2013, 0, 1).isMidnight());
    assertFalse(new DateTime(2013, 0, 1, 1).isMidnight());
    assertFalse(new DateTime(2013, 0, 1, 0, 1).isMidnight());
    assertFalse(new DateTime(2013, 0, 1, 0, 0, 1).isMidnight());
    assertFalse(new DateTime(2013, 0, 1, 0, 0, 0, 1).isMidnight());
  },

  testMinMax() {
    // Comparison of two goog.date.DateTimes
    const dateTime1 = new DateTime(2000, 0, 1);
    const dateTime2 = new DateTime(2000, 0, 1, 0, 0, 0, 1);
    assertEquals(dateTime1, googRequiredGoogDate.min(dateTime1, dateTime2));
    assertEquals(dateTime1, googRequiredGoogDate.min(dateTime2, dateTime1));
    assertEquals(dateTime2, googRequiredGoogDate.max(dateTime1, dateTime2));
    assertEquals(dateTime2, googRequiredGoogDate.max(dateTime2, dateTime1));

    // Comparison of two goog.date.Dates
    const date1 = new DateDate(2000, 0, 1);
    const date2 = new DateDate(2000, 0, 2);
    assertEquals(date1, googRequiredGoogDate.min(date1, date2));

    // Comparison of native Dates.
    const jsDate1 = new Date(2000, 0, 1);
    const jsDate2 = new Date(2000, 0, 2);
    assertEquals(jsDate1, googRequiredGoogDate.min(jsDate1, jsDate2));
    assertEquals(jsDate2, googRequiredGoogDate.max(jsDate1, jsDate2));

    // Comparison of different types.
    assertEquals(date1, googRequiredGoogDate.min(date1, dateTime2));
    assertEquals(dateTime2, googRequiredGoogDate.min(date2, dateTime2));
    assertEquals(date1, googRequiredGoogDate.min(date1, jsDate2));
    assertEquals(jsDate2, googRequiredGoogDate.max(dateTime1, jsDate2));
  },

  testDateTimeIntervalAdd() {
    // Add hours
    let d = new DateTime(2007, month.JAN, 1, 10, 20, 30);
    d.add(new Interval(Interval.HOURS, 10));
    assertEquals(20, d.getHours());

    // Add negative hours
    d.add(new Interval(Interval.HOURS, -5));
    assertEquals(15, d.getHours());

    // Add hours to the next day
    d.add(new Interval(Interval.HOURS, 10));
    assertEquals(2, d.getDay());
    assertEquals(1, d.getHours());

    // Add minutes
    d = new DateTime(2007, month.JAN, 1, 22, 20, 30);
    d.add(new Interval(Interval.MINUTES, 10));
    assertEquals(30, d.getMinutes());

    // Add negative minutes
    d.add(new Interval(Interval.MINUTES, -5));
    assertEquals(25, d.getMinutes());

    // Add minutes to the next day
    d.add(new Interval(Interval.MINUTES, 130));
    assertEquals(2, d.getDay());
    assertEquals(0, d.getHours());
    assertEquals(35, d.getMinutes());

    // Add seconds
    d = new DateTime(2007, month.JAN, 1, 23, 45, 30);
    d.add(new Interval(Interval.SECONDS, 10));
    assertEquals(40, d.getSeconds());

    // Add negative seconds
    d.add(new Interval(Interval.SECONDS, -5));
    assertEquals(35, d.getSeconds());

    // Add seconds to the next day
    d.add(new Interval(Interval.SECONDS, 1200));
    assertEquals(2, d.getDay());
    assertEquals(0, d.getHours());
    assertEquals(5, d.getMinutes());
    assertEquals(35, d.getSeconds());

    // Test daylight savings day 2015-11-1
    d = new DateTime(2015, month.NOV, 1, 0, 50, 30);
    d.add(new Interval(Interval.MINUTES, 15));
    assertEquals(1, d.getHours());
    assertEquals(5, d.getMinutes());

    d.add(new Interval(Interval.HOURS, 1));
    assertEquals(1, d.getHours());

    // Test daylight savings day 2015-3-8
    d = new DateTime(2015, month.MAR, 8, 0, 50, 30);
    d.add(new Interval(Interval.MINUTES, 15));
    assertEquals(1, d.getHours());
    assertEquals(5, d.getMinutes());

    d.add(new Interval(Interval.HOURS, 1));
    assertEquals(3, d.getHours());
  },
});
