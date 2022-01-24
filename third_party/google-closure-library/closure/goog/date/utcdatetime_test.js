/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.date.UtcDateTimeTest');
goog.setTestOnly();

const Interval = goog.require('goog.date.Interval');
const UtcDateTime = goog.require('goog.date.UtcDateTime');
const month = goog.require('goog.date.month');
const testSuite = goog.require('goog.testing.testSuite');
const weekDay = goog.require('goog.date.weekDay');

testSuite({
  /** @suppress {checkTypes} suppression added to enable type checking */
  testConstructor() {
    goog.now = () => new Date(2001, 2, 3, 4).getTime();

    let d = new UtcDateTime();
    assertTrue('default constructor', d.equals(new Date(goog.now())));

    d = new UtcDateTime(2001);
    assertTrue('year only', d.equals(new Date(Date.UTC(2001, 0, 1, 0, 0, 0))));

    d = new UtcDateTime(2001, 2, 3, 4, 5, 6, 7);
    assertTrue(
        'full date/time', d.equals(new Date(Date.UTC(2001, 2, 3, 4, 5, 6, 7))));

    d = new UtcDateTime(new Date(0));
    assertTrue(
        'copy constructor', d.equals(new Date(Date.UTC(1970, 0, 1, 0, 0, 0))));
  },

  testClone() {
    const d = new UtcDateTime(2001, 2, 3, 4, 5, 6, 7);
    assertTrue('clone of UtcDateTime', d.equals(d.clone()));
  },

  testAdd() {
    let date = new UtcDateTime(2007, month.OCT, 5);
    date.add(new Interval(-1, 2));
    let expected = new UtcDateTime(2006, month.DEC, 5);
    assertTrue('UTC date + years + months', expected.equals(date));

    date = new UtcDateTime(2007, month.OCT, 1);
    date.add(new Interval(0, 0, 60));
    expected = new UtcDateTime(2007, month.NOV, 30);
    assertTrue('UTC date + days', expected.equals(date));

    date = new UtcDateTime(2007, month.OCT, 1);
    date.add(new Interval(0, 0, 0, 60 * 24 - 12, -30, -30.5));
    expected = new UtcDateTime(2007, month.NOV, 29, 11, 29, 29, 500);
    assertTrue(
        'UTC date + time, daylight saving ignored', expected.equals(date));
  },

  testGetYear() {
    let date = new UtcDateTime(2000, month.JAN, 1);
    assertEquals('year of 2000-01-01 00:00:00', 2000, date.getYear());

    date = new UtcDateTime(1999, month.DEC, 31, 23, 59);
    assertEquals('year of 1999-12-31 23:59:00', 1999, date.getYear());
  },

  testGetDay() {
    let date = new UtcDateTime(2000, month.JAN, 1);
    assertEquals(
        '2000-01-01 00:00:00 is Saturday (UTC + ISO)', weekDay.SAT,
        date.getUTCIsoWeekday());
    assertEquals(
        '2000-01-01 00:00:00 is Saturday (ISO)', weekDay.SAT,
        date.getIsoWeekday());
    assertEquals('2000-01-01 00:00:00 is Saturday (UTC)', 6, date.getUTCDay());
    assertEquals('2000-01-01 00:00:00 is Saturday', 6, date.getDay());

    date = new UtcDateTime(2000, month.JAN, 1, 23, 59);
    assertEquals(
        '2000-01-01 23:59:00 is Saturday (UTC + ISO)', weekDay.SAT,
        date.getUTCIsoWeekday());
    assertEquals(
        '2000-01-01 23:59:00 is Saturday (ISO)', weekDay.SAT,
        date.getIsoWeekday());
    assertEquals('2000-01-01 23:59:00 is Saturday (UTC)', 6, date.getUTCDay());
    assertEquals('2000-01-01 23:59:00 is Saturday', 6, date.getDay());
  },

  testFromIsoString() {
    const dateString = '2000-01-02';
    const date = UtcDateTime.fromIsoString(dateString);
    let exp = new UtcDateTime(2000, month.JAN, 2);
    assertTrue('parsed ISO date', exp.equals(date));

    const dateTimeString = '2000-01-02 03:04:05';
    const dateTime = UtcDateTime.fromIsoString(dateTimeString);
    exp = new UtcDateTime(2000, month.JAN, 2, 3, 4, 5);
    assertTrue('parsed ISO date/time', exp.equals(dateTime));

    const dateTimeZoneString = '2089-01-02 03:04:05Z';
    const dateTimeZone = UtcDateTime.fromIsoString(dateTimeZoneString);
    exp = new UtcDateTime(2089, month.JAN, 2, 3, 4, 5);
    assertTrue('parsed ISO date/time', exp.equals(dateTimeZone));

    const dateTimeZoneString2 = '0089-01-02 03:04:05Z';
    const dateTimeZone2 = UtcDateTime.fromIsoString(dateTimeZoneString2);
    exp = new UtcDateTime(489, month.JAN, 2, 3, 4, 5);
    exp.setUTCFullYear(89);
    assertTrue('parsed ISO date/time', exp.equals(dateTimeZone2));
  },

  testToIsoString() {
    const date = new UtcDateTime(2000, month.JAN, 2, 3, 4, 5);
    assertEquals(
        'serialize date/time', '2000-01-02T03:04:05', date.toIsoString(true));
    assertEquals('serialize time only', '03:04:05', date.toIsoTimeString(true));
    assertEquals(
        'serialize date/time to XML', '2000-01-02T03:04:05',
        date.toXmlDateTime());
  },

  testIsMidnight() {
    assertTrue(new UtcDateTime(2000, 0, 1).isMidnight());
    assertFalse(new UtcDateTime(2000, 0, 1, 0, 0, 0, 1).isMidnight());
  },

  testFromTimestamp() {
    assertEquals(0, UtcDateTime.fromTimestamp(0).getTime());
    assertEquals(1234, UtcDateTime.fromTimestamp(1234).getTime());
  },
});
