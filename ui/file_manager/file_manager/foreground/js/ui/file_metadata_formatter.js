// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Formatter class for file metadatas.
 */
class FileMetadataFormatter extends cr.EventTarget {
  constructor() {
    super();
    this.setDateTimeFormat(true);

    /** @private {?Intl.DateTimeFormat} */
    this.timeFormatter_;

    /** @private {?Intl.DateTimeFormat} */
    this.dateFormatter_;
  }

  /**
   * Sets date and time format.
   * @param {boolean} use12hourClock True if 12 hours clock, False if 24 hours.
   */
  setDateTimeFormat(use12hourClock) {
    this.timeFormatter_ = new Intl.DateTimeFormat(
        [] /* default locale */,
        {hour: 'numeric', minute: 'numeric', hour12: use12hourClock});
    this.dateFormatter_ = new Intl.DateTimeFormat([] /* default locale */, {
      year: 'numeric',
      month: 'short',
      day: 'numeric',
      hour: 'numeric',
      minute: 'numeric',
      hour12: use12hourClock
    });
    cr.dispatchSimpleEvent(this, 'date-time-format-changed');
  }

  /**
   * Generates a formatted modification time text.
   * @param {Date} modTime
   * @return {string} A string that represents modification time.
   */
  formatModDate(modTime) {
    if (!modTime) {
      return '...';
    }
    const today = new Date();
    today.setHours(0);
    today.setMinutes(0);
    today.setSeconds(0);
    today.setMilliseconds(0);

    /**
     * Number of milliseconds in a day.
     */
    const MILLISECONDS_IN_DAY = 24 * 60 * 60 * 1000;

    if (isNaN(modTime.getTime())) {
      // In case of 'Invalid Date'.
      return '--';
    } else if (
        modTime >= today && modTime < today.getTime() + MILLISECONDS_IN_DAY) {
      return strf('TIME_TODAY', this.timeFormatter_.format(modTime));
    } else if (modTime >= today - MILLISECONDS_IN_DAY && modTime < today) {
      return strf('TIME_YESTERDAY', this.timeFormatter_.format(modTime));
    } else {
      return this.dateFormatter_.format(modTime);
    }
  }

  /**
   * Generates a formatted filesize text.
   * @param {number=} size
   * @param {boolean=} hosted
   * @param {boolean=} addPrecision addPrecision used to optionally add more
   *     precision digits to the formatted filesize text.
   * @return {string} A string that represents a file size.
   */
  formatSize(size, hosted, addPrecision = false) {
    if (size === null || size === undefined) {
      return '...';
    } else if (size === -1) {
      return '--';
    } else if (size === 0 && hosted) {
      return '--';
    } else {
      return util.bytesToString(size, addPrecision ? 1 : 0);
    }
  }
}
