/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.date.DateRangeTest');
goog.setTestOnly();

const DateDate = goog.require('goog.date.Date');
const DateRange = goog.require('goog.date.DateRange');
const DateTimeSymbols = goog.require('goog.i18n.DateTimeSymbols');
const Interval = goog.require('goog.date.Interval');
const testSuite = goog.require('goog.testing.testSuite');

function assertStartEnd(name, start, end, actual) {
  assertTrue(
      `${name} start should be ${start} but was ` + actual.getStartDate(),
      start.equals(actual.getStartDate()));
  assertTrue(
      `${name} end should be ${end} but was ` + actual.getEndDate(),
      end.equals(actual.getEndDate()));
  // see http://b/23820818
  assertFalse(
      `${name} start date was the same object as the end date`,
      actual.getStartDate() === actual.getEndDate());
}
testSuite({
  testDateRange() {
    const date1 = new DateDate(2000, 0, 1);
    const date2 = new DateDate(2000, 1, 1);

    const range = new DateRange(date1, date2);
    assertTrue('startDate matches', date1.equals(range.getStartDate()));
    assertTrue('endDate matches', date2.equals(range.getEndDate()));
  },

  testDateRangeEquals() {
    const date1 = new DateDate(2000, 0, 1);
    const date2 = new DateDate(2000, 1, 1);

    const range1 = new DateRange(date1, date2);
    const range2 = new DateRange(date1, date2);
    assertTrue('equals', DateRange.equals(range1, range2));
  },

  testDateRangeNotEquals() {
    const date1 = new DateDate(2000, 0, 1);
    const date2 = new DateDate(2000, 1, 1);

    const range1 = new DateRange(date1, date2);
    const range2 = new DateRange(date2, date1);
    assertFalse('not equals', DateRange.equals(range1, range2));
  },

  testOffsetInDays() {
    const d = new DateDate(2000, 0, 1);
    /** @suppress {visibility} suppression added to enable type checking */
    const f = DateRange.offsetInDays_;

    assertTrue('same day', d.equals(f(d, 0)));
    assertTrue('next day', new DateDate(2000, 0, 2).equals(f(d, 1)));
    assertTrue('last day', new DateDate(1999, 11, 31).equals(f(d, -1)));
  },

  testOffsetInMonths() {
    const d = new DateDate(2008, 9, 13);
    /** @suppress {visibility} suppression added to enable type checking */
    const f = DateRange.offsetInMonths_;

    assertTrue('this month', new DateDate(2008, 9, 1).equals(f(d, 0)));
    assertTrue('last month', new DateDate(2008, 8, 1).equals(f(d, -1)));
    assertTrue('next month', new DateDate(2008, 10, 1).equals(f(d, 1)));
    assertTrue('next year', new DateDate(2009, 9, 1).equals(f(d, 12)));
    assertTrue('last year', new DateDate(2007, 9, 1).equals(f(d, -12)));
  },

  testYesterday() {
    const d = new DateDate(2008, 9, 13);
    const s = new DateDate(2008, 9, 12);
    const e = new DateDate(2008, 9, 12);
    assertStartEnd('yesterday', s, e, DateRange.yesterday(d));
  },

  testToday() {
    const d = new DateDate(2008, 9, 13);
    assertStartEnd('today', d, d, DateRange.today(d));
  },

  testLast7Days() {
    const d = new DateDate(2008, 9, 13);
    const s = new DateDate(2008, 9, 6);
    const e = new DateDate(2008, 9, 12);
    assertStartEnd('last7Days', s, e, DateRange.last7Days(d));
    assertStartEnd(
        'last7Days by key', s, e,
        DateRange.standardDateRange(
            DateRange.StandardDateRangeKeys.LAST_7_DAYS, d));
  },

  testThisMonth() {
    const d = new DateDate(2008, 9, 13);
    const s = new DateDate(2008, 9, 1);
    const e = new DateDate(2008, 9, 31);
    assertStartEnd('thisMonth', s, e, DateRange.thisMonth(d));
    assertStartEnd(
        'thisMonth by key', s, e,
        DateRange.standardDateRange(
            DateRange.StandardDateRangeKeys.THIS_MONTH, d));
  },

  testLastMonth() {
    const d = new DateDate(2008, 9, 13);
    const s = new DateDate(2008, 8, 1);
    const e = new DateDate(2008, 8, 30);
    assertStartEnd('lastMonth', s, e, DateRange.lastMonth(d));
    assertStartEnd(
        'lastMonth by key', s, e,
        DateRange.standardDateRange(
            DateRange.StandardDateRangeKeys.LAST_MONTH, d));
  },

  testThisWeek() {
    const startDates = [
      new DateDate(2011, 2, 28),
      new DateDate(2011, 2, 29),
      new DateDate(2011, 2, 30),
      new DateDate(2011, 2, 31),
      new DateDate(2011, 3, 1),
      new DateDate(2011, 2, 26),
      new DateDate(2011, 2, 27),
    ];

    const endDates = [
      new DateDate(2011, 3, 3),
      new DateDate(2011, 3, 4),
      new DateDate(2011, 3, 5),
      new DateDate(2011, 3, 6),
      new DateDate(2011, 3, 7),
      new DateDate(2011, 3, 1),
      new DateDate(2011, 3, 2),
    ];

    // 0 - is Monday, 6 is Sunday.
    for (let i = 0; i < 7; i++) {
      const date = new DateDate(2011, 3, 1);
      date.setFirstDayOfWeek(i);
      assertStartEnd(
          `thisWeek, ${i}`, startDates[i], endDates[i],
          DateRange.thisWeek(date));
    }

    assertStartEnd(
        'thisWeek by key ', startDates[DateTimeSymbols.FIRSTDAYOFWEEK],
        endDates[DateTimeSymbols.FIRSTDAYOFWEEK],
        DateRange.standardDateRange(
            DateRange.StandardDateRangeKeys.THIS_WEEK,
            new DateDate(2011, 3, 1)));
  },

  testLastWeek() {
    const startDates = [
      new DateDate(2011, 2, 21),
      new DateDate(2011, 2, 22),
      new DateDate(2011, 2, 23),
      new DateDate(2011, 2, 24),
      new DateDate(2011, 2, 25),
      new DateDate(2011, 2, 19),
      new DateDate(2011, 2, 20),
    ];

    const endDates = [
      new DateDate(2011, 2, 27),
      new DateDate(2011, 2, 28),
      new DateDate(2011, 2, 29),
      new DateDate(2011, 2, 30),
      new DateDate(2011, 2, 31),
      new DateDate(2011, 2, 25),
      new DateDate(2011, 2, 26),
    ];

    // 0 - is Monday, 6 is Sunday.
    for (let i = 0; i < 7; i++) {
      const date = new DateDate(2011, 3, 1);
      date.setFirstDayOfWeek(i);
      assertStartEnd(
          `lastWeek, ${i}`, startDates[i], endDates[i],
          DateRange.lastWeek(date));
    }

    assertStartEnd(
        'lastWeek by key', startDates[DateTimeSymbols.FIRSTDAYOFWEEK],
        endDates[DateTimeSymbols.FIRSTDAYOFWEEK],
        DateRange.standardDateRange(
            DateRange.StandardDateRangeKeys.LAST_WEEK,
            new DateDate(2011, 3, 1)));
  },

  testLastBusinessWeek() {
    const d = new DateDate(2008, 9, 13);
    const s = new DateDate(2008, 9, 6);
    const e = new DateDate(2008, 9, 10);
    assertStartEnd('lastBusinessWeek', s, e, DateRange.lastBusinessWeek(d));
    assertStartEnd(
        'lastBusinessWeek by key', s, e,
        DateRange.standardDateRange(
            DateRange.StandardDateRangeKeys.LAST_BUSINESS_WEEK, d));
  },

  testAllTime() {
    const s = new DateDate(0, 0, 1);
    const e = new DateDate(9999, 11, 31);
    assertStartEnd('allTime', s, e, DateRange.allTime());
    assertStartEnd(
        'allTime by key', s, e,
        DateRange.standardDateRange(DateRange.StandardDateRangeKeys.ALL_TIME));
  },

  testIterator() {
    const s = new DateDate(2008, 9, 1);
    const e = new DateDate(2008, 9, 10);
    const i = new DateRange(s, e).iterator();
    assertTrue('day 0', new DateDate(2008, 9, 1).equals(i.nextValueOrThrow()));
    assertTrue('day 1', new DateDate(2008, 9, 2).equals(i.nextValueOrThrow()));
    assertTrue('day 2', new DateDate(2008, 9, 3).equals(i.nextValueOrThrow()));
    assertTrue('day 3', new DateDate(2008, 9, 4).equals(i.nextValueOrThrow()));
    assertTrue('day 4', new DateDate(2008, 9, 5).equals(i.nextValueOrThrow()));
    assertTrue('day 5', new DateDate(2008, 9, 6).equals(i.nextValueOrThrow()));
    assertTrue('day 6', new DateDate(2008, 9, 7).equals(i.nextValueOrThrow()));
    assertTrue('day 7', new DateDate(2008, 9, 8).equals(i.nextValueOrThrow()));
    assertTrue('day 8', new DateDate(2008, 9, 9).equals(i.nextValueOrThrow()));
    assertTrue('day 9', new DateDate(2008, 9, 10).equals(i.nextValueOrThrow()));
    assertThrows('day 10', goog.bind(i.nextValueOrThrow, i));
  },

  testContains() {
    const r =
        new DateRange(new DateDate(2008, 9, 10), new DateDate(2008, 9, 12));
    assertFalse('min date', r.contains(DateRange.MINIMUM_DATE));
    assertFalse('9/10/2007', r.contains(new DateDate(2007, 9, 10)));
    assertFalse('9/9/2008', r.contains(new DateDate(2008, 9, 9)));
    assertTrue('9/10/2008', r.contains(new DateDate(2008, 9, 10)));
    assertTrue('9/11/2008', r.contains(new DateDate(2008, 9, 11)));
    assertTrue('9/12/2008', r.contains(new DateDate(2008, 9, 12)));
    assertFalse('9/13/2008', r.contains(new DateDate(2008, 9, 13)));
    assertFalse('max date', r.contains(DateRange.MAXIMUM_DATE));
  },

  testSeparateDateObjects() {
    // see http://b/23820818
    const dr = DateRange.today();
    const endDate = dr.getEndDate();
    endDate.add(new Interval(Interval.DAYS, 1));
    const startDate = dr.getStartDate();
    assertFalse(startDate.equals(endDate));
  },
});
