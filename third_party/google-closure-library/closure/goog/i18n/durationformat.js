/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview DurationFormat provides methods to format duration time
 * into a human-readable, locale-sensitive string in a user friendly way and a
 * locale sensitive manner.
 */
goog.module('goog.i18n.DurationFormat');

const DurationSymbols = goog.require('goog.i18n.DurationSymbols');
const MessageFormat = goog.require('goog.i18n.MessageFormat');
const {DurationSymbols: DurationSymbolsTypes, DurationSymbolsFormatStyles} = goog.require('goog.i18n.DurationSymbolTypes');
const {ListFormat, ListFormatStyle, ListFormatType} = goog.require('goog.i18n.listFormat');
const {assert, assertNumber, assertObject} = goog.require('goog.asserts');

/**
 * Choices for options bag 'type' in DurationFormat's constructor.
 * @enum {number} DurationFormatStyle
 */
const DurationFormatStyle = {
  SHORT: 0,
  LONG: 1,
  NARROW: 2,
};
exports.DurationFormatStyle = DurationFormatStyle;

/**
 * Available keys for the input object of public method format.
 * @enum {string} DurationFormatUnit
 */
const DurationFormatUnit = {
  YEAR: 'years',
  MONTH: 'months',
  WEEK: 'weeks',
  DAY: 'days',
  HOUR: 'hours',
  MINUTE: 'minutes',
  SECOND: 'seconds'
};
exports.DurationFormatUnit = DurationFormatUnit;

/**
 * Collection of duration unit and time for a locale.
 * @typedef {{
 *   days: (number|undefined),
 *   hours: (number|undefined),
 *   minutes: (number|undefined),
 *   months: (number|undefined),
 *   seconds: (number|undefined),
 *   weeks: (number|undefined),
 *   years: (number|undefined)
 * }}
 */
let DurationLike; /* The data for the locale */

/** @typedef {!DurationLike} */
exports.DurationLike;

/**
 * Collection of duration display style.
 * @typedef {{
 *   style: DurationFormatStyle!
 * }}
 */
let DurationFormatOptions; /* The data for the display style */

/** @typedef {!DurationFormatOptions} */
exports.DurationFormatOptions;

class DurationFormat {
  /**
   * Returns the durationformatter for the locale given by goog.LOCALE.
   * specified, a durationformatter for the user's locale will be returned.
   * @param {!DurationFormatOptions=} opt_options
   *     This optional value determines the style of the duration time output.
   *     A s part of the resulting formatted string, values include LONG, SHORT,
   *     NARROW. Default is SHORT to keep consistency with ECMAScript.
   * @param {!DurationSymbolsTypes=} opt_durationSymbols
   *     This optional value can be used to override the duration format symbols
   *     selected using goog.LOCALE. This does not override the symbols used by
   *     the underlying list formatter, so users overriding this should prefer
   *     to use format each unit and do list formatting on the results.
   * @final
   */
  constructor(opt_options, opt_durationSymbols) {
    /** @private {!DurationFormatStyle} */
    const style = opt_options?.style || DurationFormatStyle.SHORT;
    assert(
        style >= DurationFormatStyle.SHORT &&
            style <= DurationFormatStyle.NARROW,
        'Style must be LONG, SHORT, NARROW');
    /** @private @const {!DurationFormatStyle} */
    this.style_ = style;

    /**
     * DurationSymbols object for locale data required by the formatter.
     * @private @const {!DurationSymbolsTypes}
     */
    const durationSymbols =
        opt_durationSymbols || DurationSymbols.getDurationSymbols();
    assert(durationSymbols !== null, 'Duration symbols cannot be null');
    this.durationSymbols_ = durationSymbols;
  }

  /**
   * From the data, return the information for the given unit and style.
   * @param {string} durationUnit
   * @return {string}
   * @private
   */
  getIcuFormattingPattern_(durationUnit) {
    const unitInfo = this.getIcuFormattingPatternsForUnit_(durationUnit);
    assertObject(unitInfo);
    return this.getIcuFormattingPatternForStyle_(unitInfo);
  };

  /**
   * From the data, check if the duration unit is legal.
   * @param {string} durationUnit
   * @return {boolean}
   * @private
   */
  checkIfLegalUnit_(durationUnit) {
    return durationUnit === DurationFormatUnit.YEAR ||
        durationUnit === DurationFormatUnit.MONTH ||
        durationUnit === DurationFormatUnit.WEEK ||
        durationUnit === DurationFormatUnit.DAY ||
        durationUnit === DurationFormatUnit.HOUR ||
        durationUnit === DurationFormatUnit.MINUTE ||
        durationUnit === DurationFormatUnit.SECOND;
  }

  /**
   * Use public unit symbol to retrieve data for that unit.
   * @param {string} unit
   * @return {!DurationSymbolsFormatStyles}
   * @private
   */
  getIcuFormattingPatternsForUnit_(unit) {
    assert(
        this.checkIfLegalUnit_(unit),
        'Unit must be years, months, weeks, days, hours, minutes or seconds');
    switch (unit) {
      default:
      case DurationFormatUnit.YEAR:
        return this.durationSymbols_.YEAR;
      case DurationFormatUnit.MONTH:
        return this.durationSymbols_.MONTH;
      case DurationFormatUnit.WEEK:
        return this.durationSymbols_.WEEK;
      case DurationFormatUnit.DAY:
        return this.durationSymbols_.DAY;
      case DurationFormatUnit.HOUR:
        return this.durationSymbols_.HOUR;
      case DurationFormatUnit.MINUTE:
        return this.durationSymbols_.MINUTE;
      case DurationFormatUnit.SECOND:
        return this.durationSymbols_.SECOND;
    }
  };

  /**
   * Use unit symbol to retrieve data for that unit, given the style.
   * @param{!DurationSymbolsFormatStyles} unitInfo
   * @return {string}
   * @private
   */
  getIcuFormattingPatternForStyle_(unitInfo) {
    // Fall back from LONG to NARROW to SHORT as needed.
    switch (this.style_) {
      case DurationFormatStyle.LONG:
        if (unitInfo.LONG != undefined) {
          return unitInfo.LONG;
        }
      case DurationFormatStyle.NARROW:
        if (unitInfo.NARROW != undefined) {
          return unitInfo.NARROW;
        }
      case DurationFormatStyle.SHORT:
      default:
        return unitInfo.SHORT;
    }
  };

  /**
   * Format using pure JavaScript
   * @param {number} quantity Duration time value.
   * @param {string} durationUnit  string such as hours, years,
   *     months.
   * @return {string} The formatted result. May be empty string for an
   *   unsupported locale.
   * @private
   */
  formatPolyfill_(quantity, durationUnit) {
    /**
     * Find the right data based on unit, quantity, and plural.
     */
    const unitStyleString = this.getIcuFormattingPattern_(durationUnit);
    if (!unitStyleString) return '';

    /**
     * Formatter for the messages requiring units. Plural formatting needed.
     * @type {?MessageFormat}
     */
    // Take basic message and wrap with plural message type.
    const msgFormatter =
        new MessageFormat('{DURATION_VALUE,plural,' + unitStyleString + '}');
    return msgFormatter.format({'DURATION_VALUE': quantity});
  };

  /**
   * Formats a string with the amount and one unit.
   * @param {number} quantity  A value for the duration time.
   * @param {string} durationUnit  string such as hours, years,
   *     months.
   * @return {string} The formatted result.
   * @private
   */
  formatWithUnit_(quantity, durationUnit) {
    assertNumber(quantity, 'Quantity must be a number');
    assert(quantity >= 0, 'Duration value should not be less than zero.');

    // TODO(user): Add formatNative_ method when available.
    return this.formatPolyfill_(quantity, durationUnit);
  };

  /**
   * Formats a string with the amount and correspondent unit.
   * @param {!DurationLike} durationLike  An object for the duration time whose
   *     keys can be one of DurationFormatUnit value.
   * @return {string} The formatted result.
   */
  format(durationLike) {
    const formattedDurationList = [];
    const years = durationLike[DurationFormatUnit.YEAR];
    const months = durationLike[DurationFormatUnit.MONTH];
    const weeks = durationLike[DurationFormatUnit.WEEK];
    const days = durationLike[DurationFormatUnit.DAY];
    const hours = durationLike[DurationFormatUnit.HOUR];
    const minutes = durationLike[DurationFormatUnit.MINUTE];
    const seconds = durationLike[DurationFormatUnit.SECOND];
    if (years != null) {
      formattedDurationList.push(
          this.formatWithUnit_(years, DurationFormatUnit.YEAR));
    }

    if (months != null) {
      formattedDurationList.push(
          this.formatWithUnit_(months, DurationFormatUnit.MONTH));
    }

    if (weeks != null) {
      formattedDurationList.push(
          this.formatWithUnit_(weeks, DurationFormatUnit.WEEK));
    }

    if (days != null) {
      formattedDurationList.push(
          this.formatWithUnit_(days, DurationFormatUnit.DAY));
    }

    if (hours != null) {
      formattedDurationList.push(
          this.formatWithUnit_(hours, DurationFormatUnit.HOUR));
    }

    if (minutes != null) {
      formattedDurationList.push(
          this.formatWithUnit_(minutes, DurationFormatUnit.MINUTE));
    }

    if (seconds != null) {
      formattedDurationList.push(
          this.formatWithUnit_(seconds, DurationFormatUnit.SECOND));
    }

    // TODO(user): Add method for STYLE.DIGIT when available.
    const newListFormater = new ListFormat(
        {type: ListFormatType.UNIT, style: ListFormatStyle.NARROW});
    return newListFormater.format(formattedDurationList);
  };
}

exports.DurationFormat = DurationFormat;