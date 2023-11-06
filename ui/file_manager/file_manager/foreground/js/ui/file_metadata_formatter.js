// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dispatchSimpleEvent} from 'chrome://resources/ash/common/cr_deprecated.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import {bytesToString, getCurrentLocaleOrDefault, strf} from '../../../common/js/translations.js';

/**
 * Formatter class for file metadatas.
 */
export class FileMetadataFormatter extends EventTarget {
  constructor() {
    super();

    /** @private @type {?Intl.DateTimeFormat} */
    this.timeFormatter_;

    /** @private @type {?Intl.DateTimeFormat} */
    this.dateFormatter_;
  }

  /**
   * Sets date and time format.
   * @param {boolean} use12hourClock True if 12 hours clock, False if 24 hours.
   */
  setDateTimeFormat(use12hourClock) {
    const locale = getCurrentLocaleOrDefault();
    const options = {
      hour: 'numeric',
      minute: 'numeric',
    };
    if (use12hourClock) {
      // @ts-ignore: error TS2551: Property 'hour12' does not exist on type '{
      // hour: string; minute: string; }'. Did you mean 'hour'?
      options['hour12'] = true;
    } else {
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type '"hourCycle"' can't be used to index type '{ hour:
      // string; minute: string; }'.
      options['hourCycle'] = 'h23';
    }
    // @ts-ignore: error TS2345: Argument of type '{ hour: string; minute:
    // string; }' is not assignable to parameter of type
    // 'DateTimeFormatOptions'.
    this.timeFormatter_ = new Intl.DateTimeFormat(locale, options);
    const dateOptions = {
      year: 'numeric',
      month: 'short',
      day: 'numeric',
      hour: 'numeric',
      minute: 'numeric',
    };
    if (use12hourClock) {
      // @ts-ignore: error TS2551: Property 'hour12' does not exist on type '{
      // year: string; month: string; day: string; hour: string; minute: string;
      // }'. Did you mean 'hour'?
      dateOptions['hour12'] = true;
    } else {
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type '"hourCycle"' can't be used to index type '{ year:
      // string; month: string; day: string; hour: string; minute: string; }'.
      dateOptions['hourCycle'] = 'h23';
    }

    // @ts-ignore: error TS2345: Argument of type '{ year: string; month:
    // string; day: string; hour: string; minute: string; }' is not assignable
    // to parameter of type 'DateTimeFormatOptions'.
    this.dateFormatter_ = new Intl.DateTimeFormat(locale, dateOptions);
    dispatchSimpleEvent(this, 'date-time-format-changed');
  }

  /**
   * Generates a formatted modification time text.
   * @param {Date=} modTime
   * @return {string} A string that represents modification time.
   */
  formatModDate(modTime) {
    if (!modTime) {
      return '--';
    }

    if (!(this.timeFormatter_ && this.dateFormatter_)) {
      this.setDateTimeFormat(true);
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
        // @ts-ignore: error TS2365: Operator '<' cannot be applied to types
        // 'Date' and 'number'.
        modTime >= today && modTime < today.getTime() + MILLISECONDS_IN_DAY) {
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      return strf('TIME_TODAY', this.timeFormatter_.format(modTime));
      // @ts-ignore: error TS2362: The left-hand side of an arithmetic operation
      // must be of type 'any', 'number', 'bigint' or an enum type.
    } else if (modTime >= today - MILLISECONDS_IN_DAY && modTime < today) {
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      return strf('TIME_YESTERDAY', this.timeFormatter_.format(modTime));
    } else {
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
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
      return bytesToString(size, addPrecision ? 1 : 0);
    }
  }
}
