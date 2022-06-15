// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Recent date bucket definition and util functions.
 */

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

/**
 * Given a date and now date, return the date bucket it belongs to.
 *
 * @param {!Date|undefined} date
 * @param {!Date} now
 * @return {!chrome.fileManagerPrivate.RecentDateBucket}
 */
export function getRecentDateBucket(date, now) {
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
  const localeBasedWeekStart = loadTimeData.getInteger('WEEK_START_FROM') || 0;
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
