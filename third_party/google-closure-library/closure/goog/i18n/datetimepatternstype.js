/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Date/Time patterns data type.
 */

goog.module('goog.i18n.DateTimePatternsType');

/**
 * The type definition for date/time patterns.
 * @record
 */
class DateTimePatternsType {
  constructor() {
    /** @type {string} */
    this.YEAR_FULL;

    /** @type {string} */
    this.YEAR_FULL_WITH_ERA;

    /** @type {string} */
    this.YEAR_MONTH_ABBR;

    /** @type {string} */
    this.YEAR_MONTH_FULL;

    /** @type {string} */
    this.YEAR_MONTH_SHORT;

    /** @type {string} */
    this.MONTH_DAY_ABBR;

    /** @type {string} */
    this.MONTH_DAY_FULL;

    /** @type {string} */
    this.MONTH_DAY_SHORT;

    /** @type {string} */
    this.MONTH_DAY_MEDIUM;

    /** @type {string} */
    this.MONTH_DAY_YEAR_MEDIUM;

    /** @type {string} */
    this.WEEKDAY_MONTH_DAY_MEDIUM;

    /** @type {string} */
    this.WEEKDAY_MONTH_DAY_YEAR_MEDIUM;

    /** @type {string} */
    this.DAY_ABBR;

    /** @type {string} */
    this.MONTH_DAY_TIME_ZONE_SHORT;
  }
}

exports = DateTimePatternsType;
