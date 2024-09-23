// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Recent date bucket definition and util functions.
 */

import {SearchRecency} from '../../state/state.js';

import {getLocaleBasedWeekStart} from './translations.js';

/**
 * Given a date and now date, return the date bucket it belongs to.
 */
export function getRecentDateBucket(date: Date|undefined, now: Date):
    chrome.fileManagerPrivate.RecentDateBucket {
  if (!date) {
    return chrome.fileManagerPrivate.RecentDateBucket.OLDER;
  }
  const startOfToday = new Date(now);
  startOfToday.setHours(0, 0, 0);
  if (date >= startOfToday) {
    return chrome.fileManagerPrivate.RecentDateBucket.TODAY;
  }
  const startOfYesterday = new Date(startOfToday);
  startOfYesterday.setDate(startOfToday.getDate() - 1);
  if (date >= startOfYesterday) {
    return chrome.fileManagerPrivate.RecentDateBucket.YESTERDAY;
  }
  const startOfThisWeek = new Date(startOfToday);
  const localeBasedWeekStart = getLocaleBasedWeekStart();
  const daysDiff = (startOfToday.getDay() - localeBasedWeekStart + 7) % 7;
  startOfThisWeek.setDate(startOfToday.getDate() - daysDiff);
  if (date >= startOfThisWeek) {
    return chrome.fileManagerPrivate.RecentDateBucket.EARLIER_THIS_WEEK;
  }
  const startOfThisMonth = new Date(now.getFullYear(), now.getMonth(), 1);
  if (date >= startOfThisMonth) {
    return chrome.fileManagerPrivate.RecentDateBucket.EARLIER_THIS_MONTH;
  }
  const startOfThisYear = new Date(now.getFullYear(), 0, 1);
  if (date >= startOfThisYear) {
    return chrome.fileManagerPrivate.RecentDateBucket.EARLIER_THIS_YEAR;
  }
  return chrome.fileManagerPrivate.RecentDateBucket.OLDER;
}

export function getTranslationKeyForDateBucket(
    dateBucket: chrome.fileManagerPrivate.RecentDateBucket): string {
  const DATE_BUCKET_TO_TRANSLATION_KEY_MAP = new Map([
    [
      chrome.fileManagerPrivate.RecentDateBucket.TODAY,
      'RECENT_TIME_HEADING_TODAY',
    ],
    [
      chrome.fileManagerPrivate.RecentDateBucket.YESTERDAY,
      'RECENT_TIME_HEADING_YESTERDAY',
    ],
    [
      chrome.fileManagerPrivate.RecentDateBucket.EARLIER_THIS_WEEK,
      'RECENT_TIME_HEADING_THIS_WEEK',
    ],
    [
      chrome.fileManagerPrivate.RecentDateBucket.EARLIER_THIS_MONTH,
      'RECENT_TIME_HEADING_THIS_MONTH',
    ],
    [
      chrome.fileManagerPrivate.RecentDateBucket.EARLIER_THIS_YEAR,
      'RECENT_TIME_HEADING_THIS_YEAR',
    ],
    [
      chrome.fileManagerPrivate.RecentDateBucket.OLDER,
      'RECENT_TIME_HEADING_OLDER',
    ],
  ]);
  return DATE_BUCKET_TO_TRANSLATION_KEY_MAP.get(dateBucket)!;
}

/**
 * Computes the timestamp based on options. If the options ask for today's
 * results, it uses the time in ms from midnight. For yesterday, it goes back
 * by one day from midnight. For week, it goes back by 6 days from midnight.
 * For a month, it goes back by 30 days since midnight, regardless of how
 * many days are in the current month. For a year, it goes back by 365 days
 * since midnight, regardless if the current year is a leap year or not.
 *
 * @return The earliest timestamp for the given recency option.
 */
export function getEarliestTimestamp(
    recency: SearchRecency, now: Date): number {
  const midnight = new Date(now.getFullYear(), now.getMonth(), now.getDate());
  const midnightMs = midnight.getTime();
  const dayMs = 24 * 60 * 60 * 1000;

  switch (recency) {
    case SearchRecency.TODAY:
      return midnightMs;
    case SearchRecency.YESTERDAY:
      return midnightMs - 1 * dayMs;
    case SearchRecency.LAST_WEEK:
      return midnightMs - 6 * dayMs;
    case SearchRecency.LAST_MONTH:
      return midnightMs - 30 * dayMs;
    case SearchRecency.LAST_YEAR:
      return midnightMs - 365 * dayMs;
    default:
      return 0;
  }
}
