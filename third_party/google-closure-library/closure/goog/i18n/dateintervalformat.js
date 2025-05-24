/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview DateIntervalFormat provides methods to format a date interval
 * into a string in a user friendly way and a locale sensitive manner.
 *
 * Similar to the ICU4J class com/ibm/icu/text/DateIntervalFormat:
 *  http://icu-project.org/apiref/icu4j/com/ibm/icu/text/DateIntervalFormat.html
 *
 * Example usage:
 * const DateIntervalFormat = goog.require('goog.i18n.DateIntervalFormat');
 * const DateRange = goog.require('goog.date.DateRange');
 * const DateTime = goog.require('goog.date.DateTime');
 * const DateTimeFormat = goog.require('goog.i18n.DateTimeFormat');
 * const GDate = goog.require('goog.date.Date');
 * const Interval = goog.require('goog.date.Interval');
 *
 * This also uses ECMAScript native implementation if supported by the browser.
 * https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Intl/DateTimeFormat
 * using the formatRange() option.
 *
 * // Formatter.
 * const dtIntFmt = new DateIntervalFormat(DateTimeFormat.Format.MEDIUM_DATE);
 *
 * // Format a date range.
 * const dt1 = new GDate(2016, 8, 23);
 * const dt2 = new GDate(2016, 8, 24);
 * const dtRng = new DateRange(dt1, dt2);
 * dtIntFmt.formatRange(dtRng); // --> 'Sep 23 – 24, 2016'
 *
 * // Format two dates.
 * const dt3 = new DateTime(2016, 8, 23, 14, 53, 0);
 * const dt4 = new DateTime(2016, 8, 23, 14, 54, 0);
 * dtIntFmt.format(dt3, dt4); // --> 'Sep 23, 2016'
 *
 * // Format a date and an interval.
 * const dt5 = new DateTime(2016, 8, 23, 14, 53, 0);
 * const itv = new Interval(0, 1); // One month.
 * dtIntFmt.format(dt5, itv); // --> 'Sep 23 – Oct 23, 2016'
 */

goog.module('goog.i18n.DateIntervalFormat');

const DateLike = goog.require('goog.date.DateLike');
const DateRange = goog.require('goog.date.DateRange');
const DateTime = goog.require('goog.date.DateTime');
const DateTimeFormat = goog.require('goog.i18n.DateTimeFormat');
const DateTimeSymbols = goog.require('goog.i18n.DateTimeSymbols');
const DateTimeSymbolsType = goog.require('goog.i18n.DateTimeSymbolsType');
const Interval = goog.require('goog.date.Interval');
const LocaleFeature = goog.require('goog.i18n.LocaleFeature');
const TimeZone = goog.require('goog.i18n.TimeZone');
const array = goog.require('goog.array');
const asserts = goog.require('goog.asserts');
const dateIntervalSymbols = goog.require('goog.i18n.dateIntervalSymbols');
const object = goog.require('goog.object');

/**
 * Constructs a DateIntervalFormat object based on the current locale.
 *
 * @param {number|!dateIntervalSymbols.DateIntervalPatternMap} pattern Pattern
 *     specification or pattern object.
 * @param {!dateIntervalSymbols.DateIntervalSymbols=} opt_dateIntervalSymbols
 *     Optional DateIntervalSymbols to use for this instance rather than the
 *     global symbols.
 * @param {!DateTimeSymbolsType=} opt_dateTimeSymbols Optional DateTimeSymbols
 *     to use for this instance rather than the global symbols.
 * @constructor
 * @struct
 * @final
 */
let DateIntervalFormat = function(
    pattern, opt_dateIntervalSymbols, opt_dateTimeSymbols) {
  asserts.assert(pattern !== undefined, 'Pattern must be defined.');

  /**
   * A reference to a native Intl DateTimeFormat object, only used when
   * USE_ECMASCRIPT_I18N_DATEINTERVALFORMAT is true.
   * @private {?goog.global.Intl.DateTimeFormat}
   */
  this.intlFormatter_ = null;
  // TimeZone information of non-integral hour offset.
  /** @private {number} */
  this.timeZoneMillisecOffset_ = 0;
  /** @private {string} */
  this.tzId_ = '';

  /**
   * Used in place of resolvedOptions timeStyle as workaround.
   * Records timeStyle setting.
   * TODO(user): replace this with resolvedOptions.timeStyle
   * @private {string|undefined}
   */
  this.timeStyle;

  // Note: the ECMAScript implementation cannot parse an arbitrary pattern.
  if ((typeof pattern === 'number') &&
      LocaleFeature.USE_ECMASCRIPT_I18N_DATEINTERVALFORMAT) {
    // Implement using ECMAScript Intl object.
    /**
     * @type {undefined|!DateTimeFormat.IntlOptions}
     * @private @const
     */
    const options = this.getIntlOptions_(pattern);
    this.intlFormatter_ = new Intl.DateTimeFormat(goog.LOCALE, options);
    this.timeStyle = options.timeStyle;
  } else {
    asserts.assert(
        opt_dateIntervalSymbols !== undefined ||
            dateIntervalSymbols.getDateIntervalSymbols() !== undefined,
        'goog.i18n.DateIntervalSymbols or explicit symbols must be defined');
    asserts.assert(
        opt_dateTimeSymbols !== undefined || DateTimeSymbols !== undefined,
        'goog.i18n.DateTimeSymbols or explicit symbols must be defined');
    /**
     * DateIntervalSymbols object that contains locale data required by the
     * formatter.
     * @private @const {!dateIntervalSymbols.DateIntervalSymbols}
     */
    this.dateIntervalSymbols_ =
        opt_dateIntervalSymbols || dateIntervalSymbols.getDateIntervalSymbols();

    /**
     * DateTimeSymbols object that contains locale data required by the
     * formatter's implementation with JavaScript, requiring data and code.
     * @private @const {!DateTimeSymbolsType}
     */
    this.dateTimeSymbols_ = opt_dateTimeSymbols || DateTimeSymbols;

    /**
     * Date interval pattern to use.
     * @private @const {!dateIntervalSymbols.DateIntervalPatternMap}
     */
    this.intervalPattern_ = this.getIntervalPattern_(pattern);

    /**
     * Keys of the available date interval patterns. Used to lookup the key that
     * contains a specific pattern letter (e.g. for ['Myd', 'hms'], the key that
     * contains 'y' is 'Myd').
     * @private @const {!Array<string>}
     */
    this.intervalPatternKeys_ = object.getKeys(this.intervalPattern_);

    // Remove the default pattern's key ('_') from intervalPatternKeys_. Is not
    // necesary when looking up for a key: when no key is found it will always
    // default to the default pattern.
    array.remove(this.intervalPatternKeys_, DEFAULT_PATTERN_KEY_);

    /**
     * Default fallback pattern to use.
     * @private @const {string}
     */
    this.fallbackPattern_ =
        this.dateIntervalSymbols_.FALLBACK || DEFAULT_FALLBACK_PATTERN_;

    // Determine which date should be used with each part of the interval
    // pattern.
    const indexOfFirstDate =
        this.fallbackPattern_.indexOf(FIRST_DATE_PLACEHOLDER_);
    const indexOfSecondDate =
        this.fallbackPattern_.indexOf(SECOND_DATE_PLACEHOLDER_);
    if (indexOfFirstDate < 0 || indexOfSecondDate < 0) {
      throw new Error('Malformed fallback interval pattern');
    }

    /**
     * True if the first date provided should be formatted with the first
     * pattern of the interval pattern.
     * @private @const {boolean}
     */
    this.useFirstDateOnFirstPattern_ = indexOfFirstDate <= indexOfSecondDate;

    /**
     * Map that stores a Formatter_ object per calendar field. Formatters will
     * be instanced on demand and stored on this map until required again.
     * @private @const {!Object<string, !Formatter_>}
     */
    this.formatterMap_ = {};
  }
};

/**
 * Default fallback interval pattern.
 * @private @const {string}
 */
const DEFAULT_FALLBACK_PATTERN_ = '{0} – {1}';

/**
 * Interval pattern placeholder for the first date.
 * @private @const {string}
 */
const FIRST_DATE_PLACEHOLDER_ = '{0}';

/**
 * Interval pattern placeholder for the second date.
 * @private @const {string}
 */
const SECOND_DATE_PLACEHOLDER_ = '{1}';

/**
 * Key used by the default datetime pattern.
 * @private @const {string}
 */
const DEFAULT_PATTERN_KEY_ = '_';

/**
 * Gregorian calendar Eras.
 * @private @const @enum {number}
 */
const Era_ = {
  BC: 0,
  AD: 1
};

/**
 * Am Pm markers.
 * @private @const @enum {number}
 */
const AmPm_ = {
  AM: 0,
  PM: 1
};

/**
 * String of all pattern letters representing the relevant calendar fields.
 * Sorted according to the length of the datetime unit they represent.
 * @private @const {string}
 */
const RELEVANT_CALENDAR_FIELDS_ = 'GyMdahms';

/**
 * Regex that matches all possible pattern letters.
 * @private @const {!RegExp}
 */
const ALL_PATTERN_LETTERS_ = /[a-zA-Z]/;

/**
 * Returns the interval pattern from a pattern specification or from the pattern
 * object.
 * @param {number|!dateIntervalSymbols.DateIntervalPatternMap} pattern Pattern
 *     specification or pattern object.
 * @return {!dateIntervalSymbols.DateIntervalPatternMap}
 * @private
 */
DateIntervalFormat.prototype.getIntervalPattern_ = function(pattern) {
  if (typeof pattern === 'number') {
    switch (pattern) {
      case DateTimeFormat.Format.FULL_DATE:
        return this.dateIntervalSymbols_.FULL_DATE;
      case DateTimeFormat.Format.LONG_DATE:
        return this.dateIntervalSymbols_.LONG_DATE;
      case DateTimeFormat.Format.MEDIUM_DATE:
        return this.dateIntervalSymbols_.MEDIUM_DATE;
      case DateTimeFormat.Format.SHORT_DATE:
        return this.dateIntervalSymbols_.SHORT_DATE;
      case DateTimeFormat.Format.FULL_TIME:
        return this.dateIntervalSymbols_.FULL_TIME;
      case DateTimeFormat.Format.LONG_TIME:
        return this.dateIntervalSymbols_.LONG_TIME;
      case DateTimeFormat.Format.MEDIUM_TIME:
        return this.dateIntervalSymbols_.MEDIUM_TIME;
      case DateTimeFormat.Format.SHORT_TIME:
        return this.dateIntervalSymbols_.SHORT_TIME;
      case DateTimeFormat.Format.FULL_DATETIME:
        return this.dateIntervalSymbols_.FULL_DATETIME;
      case DateTimeFormat.Format.LONG_DATETIME:
        return this.dateIntervalSymbols_.LONG_DATETIME;
      case DateTimeFormat.Format.MEDIUM_DATETIME:
        return this.dateIntervalSymbols_.MEDIUM_DATETIME;
      case DateTimeFormat.Format.SHORT_DATETIME:
        return this.dateIntervalSymbols_.SHORT_DATETIME;
      default:
        return this.dateIntervalSymbols_.MEDIUM_DATETIME;
    }
  } else {
    return pattern;
  }
};

/**
 * Returns the Intl DateTimeFormat options from a pattern specification
 * or from the pattern object.
 * @param {number|!dateIntervalSymbols.DateIntervalPatternMap} pattern Pattern
 *     specification or pattern object.
 * @return {undefined|!DateTimeFormat.IntlOptions} Options bag for Intl
 *     constructor
 * @private
 */
DateIntervalFormat.prototype.getIntlOptions_ = function(pattern) {
  if (typeof pattern === 'number') {
    switch (pattern) {
      case DateTimeFormat.Format.FULL_DATE:
        return {dateStyle: 'full'};
      case DateTimeFormat.Format.LONG_DATE:
        return {dateStyle: 'long'};
      case DateTimeFormat.Format.MEDIUM_DATE:
        return {dateStyle: 'medium'};
      case DateTimeFormat.Format.SHORT_DATE:
        return {dateStyle: 'short'};
      case DateTimeFormat.Format.FULL_TIME:
        return {timeStyle: 'full'};
      case DateTimeFormat.Format.LONG_TIME:
        return {timeStyle: 'long'};
      case DateTimeFormat.Format.MEDIUM_TIME:
        return {timeStyle: 'medium'};
      case DateTimeFormat.Format.SHORT_TIME:
        return {timeStyle: 'short'};
      case DateTimeFormat.Format.FULL_DATETIME:
        return {dateStyle: 'full', timeStyle: 'full'};
      case DateTimeFormat.Format.LONG_DATETIME:
        return {dateStyle: 'long', timeStyle: 'long'};
      case DateTimeFormat.Format.MEDIUM_DATETIME:
        return {dateStyle: 'medium', timeStyle: 'medium'};
      case DateTimeFormat.Format.SHORT_DATETIME:
      default:
        return {dateStyle: 'short', timeStyle: 'short'};
    }
  } else {
    /* Custom pattern strings are not supported when using the ECMAScript
     * DateTimeFormat. Please use one of the formats in DateTimeFormat.Format
     */
    throw new Error('Pattern string not supported in ECMAScript.');
  }
};

/**
 * Formats the given date or date interval objects according to the present
 * pattern and current locale.
 *
 * Parameter combinations:
 *  * StartDate: {@link goog.date.DateLike}, EndDate: {@link goog.date.DateLike}
 *  * StartDate: {@link goog.date.DateLike}, Interval: {@link goog.date.Interval}
 *
 * @param {!DateLike} startDate Start date of the date range.
 * @param {!DateLike|!Interval} endDate End date of the date range or an
 *     interval object.
 * @param {!TimeZone=} opt_timeZone Timezone to be used in the target
 *     representation.
 * @return {string} Formatted date interval.
 */
DateIntervalFormat.prototype.format = function(
    startDate, endDate, opt_timeZone) {
  asserts.assert(
      startDate != null,
      'The startDate parameter should be defined and not-null.');
  asserts.assert(
      endDate != null, 'The endDate parameter should be defined and not-null.');

  // Convert input to DateLike.
  let endDt;
  if (goog.isDateLike(endDate)) {
    endDt = /** @type {!DateLike} */ (endDate);
  } else {
    // This is an increment to be added to the startDate.
    asserts.assertInstanceof(
        endDate, Interval,
        'endDate parameter should be a goog.date.DateLike or ' +
            'goog.date.Interval');
    endDt = new DateTime(startDate);
    endDt.add(endDate);
  }

  if (this.intlFormatter_) {
    return this.formatWithIntl_(startDate, endDt, opt_timeZone);
  } else {
    // Apply pure Javascript formatting
    // Obtain the largest different calendar field between the two dates.
    const largestDifferentCalendarField =
        DateIntervalFormat.getLargestDifferentCalendarField_(
            startDate, endDt, opt_timeZone);

    // Get the Formatter_ required to format the specified calendar field and
    // use it to format the dates.
    const formatter =
        this.getFormatterForCalendarField_(largestDifferentCalendarField);
    return formatter.format(
        startDate, endDt, largestDifferentCalendarField, opt_timeZone);
  }
};

/**
 * Use ECMAScript Intl formatter to create result string.
 * @param {!DateLike} startDate Start date of the date range.
 * @param {!DateLike} endDt End date of the date range or an
 *     interval object.
 * @param {!TimeZone=} opt_timeZone Timezone to be used in the target
 *     representation.
 * @return {string} Formatted date interval.
 * @private
 */
DateIntervalFormat.prototype.formatWithIntl_ = function(
    startDate, endDt, opt_timeZone) {
  if (opt_timeZone) {
    // Remember the original TZ string and the offset in milliseconds.
    this.tzId_ = opt_timeZone.getShortName(startDate);
    const rawOffset =
        opt_timeZone.getTimeZoneData()['std_offset'];  // In minutes
    // Find whole number of hours
    const timeZoneRoundedHourOffset_ = Math.floor(rawOffset / 60);
    // Milliseconds offset from the integral hour
    this.timeZoneMillisecOffset_ =
        60000 * (rawOffset - timeZoneRoundedHourOffset_ * 60);

    // Create timezone from the rounded hour and set in the options.
    const tz = TimeZone.createTimeZone(-timeZoneRoundedHourOffset_ * 60);

    // Check if this is the same timezone as the current formatter.
    // If not, rebuild the formatter.
    const newTzId = tz.getTimeZoneId();
    const resolvedOptions = this.intlFormatter_.resolvedOptions();
    if (resolvedOptions && newTzId != resolvedOptions.timeZone) {
      // Build with only these options from existing formatter.
      let newOptions = {
        timeStyle: this.timeStyle,  // Workaround
        dateStyle: resolvedOptions.dateStyle,
        timeZone: newTzId
      };
      this.intlFormatter_ =
          new Intl.DateTimeFormat(resolvedOptions.locale, newOptions);
    }
  }
  /**
   * @type {!Date|number|undefined} realStartDate type match for Closure
   */
  let realStartDate = startDate ? new Date(startDate.valueOf()) : undefined;
  /**
   * @type {!Date|number|undefined} realEndDate type match for Closure
   */
  let realEndDate = endDt ? new Date(endDt.valueOf()) : undefined;

  if (this.timeZoneMillisecOffset_ === 0) {
    return this.intlFormatter_.formatRange(realStartDate, realEndDate);
  } else {
    // Handle timezones with non-integral hour offset.

    // Adjust start and end times by the timezone's minute offset
    realStartDate =
        new Date(startDate.getTime() + this.timeZoneMillisecOffset_);
    realEndDate = new Date(endDt.getTime() + this.timeZoneMillisecOffset_);

    // Finally, adjust the timezone's offset in the string by updating
    // the timezone name in the formatted parts.
    const parts =
        this.intlFormatter_.formatRangeToParts(realStartDate, realEndDate);
    return parts
        .map(({type, value}) => {
          switch (type) {
            case 'timeZoneName':
              return this.tzId_;
            default:
              return value;
          }
        })
        .join('');  // Rebuild the string from parts.
  }
};

/**
 * Formats the given date range object according to the present pattern
 * and current locale.
 *
 * @param {!DateRange} dateRange
 * @param {!TimeZone=} opt_timeZone Timezone to be used in the target
 *     representation.
 * @return {string} Formatted date interval.
 */
DateIntervalFormat.prototype.formatRange = function(dateRange, opt_timeZone) {
  asserts.assert(
      dateRange != null,
      'The dateRange parameter should be defined and non-null.');
  const startDate = dateRange.getStartDate();
  const endDate = dateRange.getEndDate();
  if (startDate == null) {
    throw new Error(
        'The dateRange\'s startDate should be defined and non-null.');
  }
  if (endDate == null) {
    throw new Error('The dateRange\'s endDate should be defined and non-null.');
  }
  if (this.intlFormatter_) {
    /**
     * @type {!Date|number|undefined} realStartDate type match for Closure
     */
    const realStartDate = startDate ? new Date(startDate.valueOf()) : undefined;
    /**
     * @type {!Date|number|undefined} realEndDate type match for Closure
     */
    const realEndDate = endDate ? new Date(endDate.valueOf()) : undefined;
    return this.intlFormatter_.formatRange(realStartDate, realEndDate);
  } else {
    // Javascript implementation
    return this.format(startDate, endDate, opt_timeZone);
  }
};

/**
 * Returns the Formatter_ to be used to format two dates for the given calendar
 * field.
 * @param {string} calendarField Pattern letter representing the calendar field.
 * @return {!Formatter_}
 * @private
 */
DateIntervalFormat.prototype.getFormatterForCalendarField_ = function(
    calendarField) {
  if (calendarField != '') {
    for (let i = 0; i < this.intervalPatternKeys_.length; i++) {
      if (this.intervalPatternKeys_[i].indexOf(calendarField) >= 0) {
        return this.getOrCreateFormatterForKey_(this.intervalPatternKeys_[i]);
      }
    }
  }
  return this.getOrCreateFormatterForKey_(DEFAULT_PATTERN_KEY_);
};

/**
 * Returns and creates (if necessary) a formatter for the specified key.
 * @param {string} key
 * @return {!Formatter_}
 * @private
 */
DateIntervalFormat.prototype.getOrCreateFormatterForKey_ = function(key) {
  const fmt = this;
  return object.setWithReturnValueIfNotSet(this.formatterMap_, key, function() {
    const patternParts =
        DateIntervalFormat.divideIntervalPattern_(fmt.intervalPattern_[key]);
    if (patternParts === null) {
      return new DateTimeFormatter_(
          fmt.intervalPattern_[key], fmt.fallbackPattern_,
          fmt.dateTimeSymbols_);
    }
    return new IntervalFormatter_(
        patternParts.firstPart, patternParts.secondPart, fmt.dateTimeSymbols_,
        fmt.useFirstDateOnFirstPattern_);
  });
};

/**
 * Divides the interval pattern string into its two parts. Will return null if
 * the pattern can't be divided (e.g. it's a datetime pattern).
 * @param {string} intervalPattern
 * @return {?{firstPart:string, secondPart:string}} Record containing the two
 *     parts of the interval pattern. Null if the pattern can't be divided.
 * @private
 */
DateIntervalFormat.divideIntervalPattern_ = function(intervalPattern) {
  let foundKeys = {};
  let patternParts = null;
  // Iterate over the pattern until a repeated calendar field is found.
  DateIntervalFormat.executeForEveryCalendarField_(
      intervalPattern, function(char, index) {
        if (object.containsKey(foundKeys, char)) {
          patternParts = {
            firstPart: intervalPattern.substring(0, index),
            secondPart: intervalPattern.substring(index)
          };
          return false;
        }
        object.set(foundKeys, char, true);
        return true;
      });

  return patternParts;
};

/**
 * Iterates over a pattern string and executes a function for every
 * calendar field. The function will be executed once, independent of the width
 * of the calendar field (number of repeated pattern letters). It will ignore
 * all literal text (enclosed by quotes).
 *
 * For example, on: "H 'h' mm – H 'h' mm" it will call the function for:
 * H (pos:0), m (pos:6), H (pos:11), m (pos:17).
 *
 * @param {string} pattern
 * @param {function(string, number):boolean} func Function which accepts as
 *     parameters the current calendar field and the index of its first pattern
 *     letter; and returns a boolean which indicates if the iteration should
 *     continue.
 * @private
 */
DateIntervalFormat.executeForEveryCalendarField_ = function(pattern, func) {
  let inQuote = false;
  let previousChar = '';
  for (let i = 0; i < pattern.length; i++) {
    const char = pattern.charAt(i);
    if (inQuote) {
      if (char == '\'') {
        if (i + 1 < pattern.length && pattern.charAt(i + 1) == '\'') {
          i++;  // Literal quotation mark: ignore and advance.
        } else {
          inQuote = false;
        }
      }
    } else {
      if (char == '\'') {
        inQuote = true;
      } else if (char != previousChar && ALL_PATTERN_LETTERS_.test(char)) {
        if (!func(char, i)) {
          break;
        }
      }
    }
    previousChar = char;
  }
};

/**
 * Returns a pattern letter representing the largest different calendar field
 * between the two dates. This is calculated using the timezone used in the
 * target representation.
 * @param {!DateLike} startDate Start date of the date range.
 * @param {!DateLike} endDate End date of the date range.
 * @param {!TimeZone=} opt_timeZone Timezone to be used in the target
 *     representation.
 * @return {string} Pattern letter representing the largest different calendar
 *     field or an empty string if all relevant fields for these dates are equal.
 * @private
 */
DateIntervalFormat.getLargestDifferentCalendarField_ = function(
    startDate, endDate, opt_timeZone) {
  // Before comparing them, dates have to be adjusted by the target timezone's
  // offset.
  let startDiff = 0;
  let endDiff = 0;
  if (opt_timeZone != null) {
    startDiff =
        (startDate.getTimezoneOffset() - opt_timeZone.getOffset(startDate)) *
        60000;
    endDiff =
        (endDate.getTimezoneOffset() - opt_timeZone.getOffset(endDate)) * 60000;
  }
  const startDt = new Date(startDate.getTime() + startDiff);
  const endDt = new Date(endDate.getTime() + endDiff);

  if (DateIntervalFormat.getEra_(startDt) !=
      DateIntervalFormat.getEra_(endDt)) {
    return 'G';
  } else if (startDt.getFullYear() != endDt.getFullYear()) {
    return 'y';
  } else if (startDt.getMonth() != endDt.getMonth()) {
    return 'M';
  } else if (startDt.getDate() != endDt.getDate()) {
    return 'd';
  } else if (
      DateIntervalFormat.getAmPm_(startDt) !=
      DateIntervalFormat.getAmPm_(endDt)) {
    return 'a';
  } else if (startDt.getHours() != endDt.getHours()) {
    return 'h';
  } else if (startDt.getMinutes() != endDt.getMinutes()) {
    return 'm';
  } else if (startDt.getSeconds() != endDt.getSeconds()) {
    return 's';
  }
  return '';
};

/**
 * Returns the Era of a given DateLike object.
 * @param {!Date} date
 * @return {number}
 * @private
 */
DateIntervalFormat.getEra_ = function(date) {
  return date.getFullYear() > 0 ? Era_.AD : Era_.BC;
};

/**
 * Returns if the given date is in AM or PM.
 * @param {!Date} date
 * @return {number}
 * @private
 */
DateIntervalFormat.getAmPm_ = function(date) {
  const hours = date.getHours();
  return (12 <= hours && hours < 24) ? AmPm_.PM : AmPm_.AM;
};

/**
 * Returns true if the calendar field field1 is a larger or equal than field2.
 * Assumes that both string parameters have just one character. Field1 has to
 * be part of the relevant calendar fields set.
 * @param {string} field1
 * @param {string} field2
 * @return {boolean}
 * @private
 */
DateIntervalFormat.isCalendarFieldLargerOrEqualThan_ = function(
    field1, field2) {
  return RELEVANT_CALENDAR_FIELDS_.indexOf(field1) <=
      RELEVANT_CALENDAR_FIELDS_.indexOf(field2);
};

/**
 * Interface implemented by internal date interval formatters.
 * @interface
 * @private
 */
let Formatter_ = function() {};

/**
 * Formats two dates with the two parts of the date interval and returns the
 * formatted string.
 * @param {!DateLike} firstDate
 * @param {!DateLike} secondDate
 * @param {string} largestDifferentCalendarField
 * @param {!TimeZone=} opt_timeZone Target timezone in which to format the
 *     dates.
 * @return {string} String with the formatted date interval.
 */
Formatter_.prototype.format = function(
    firstDate, secondDate, largestDifferentCalendarField, opt_timeZone) {};

/**
 * Constructs an IntervalFormatter_ object which implements the Formatter_
 * interface.
 *
 * Internal object to construct and store a goog.i18n.DateTimeFormat for each
 * part of the date interval pattern.
 *
 * @param {string} firstPattern First part of the date interval pattern.
 * @param {string} secondPattern Second part of the date interval pattern.
 * @param {!DateTimeSymbolsType} dateTimeSymbols Symbols to use with the
 *     datetime formatters.
 * @param {boolean} useFirstDateOnFirstPattern Indicates if the first or the
 *     second date should be formatted with the first or second part of the date
 *     interval pattern.
 * @constructor
 * @implements {Formatter_}
 * @private
 */
let IntervalFormatter_ = function(
    firstPattern, secondPattern, dateTimeSymbols, useFirstDateOnFirstPattern) {
  /**
   * Formatter_ to format the first part of the date interval.
   * @private @const {!DateTimeFormat}
   */
  this.firstPartFormatter_ = new DateTimeFormat(firstPattern, dateTimeSymbols);

  /**
   * Formatter_ to format the second part of the date interval.
   * @private @const {!DateTimeFormat}
   */
  this.secondPartFormatter_ =
      new DateTimeFormat(secondPattern, dateTimeSymbols);

  /**
   * Specifies if the first or the second date should be formatted by the
   * formatter of the first or second part of the date interval.
   * @private {boolean}
   */
  this.useFirstDateOnFirstPattern_ = useFirstDateOnFirstPattern;
};

/**
 * @param {!DateLike} firstDate Start date of the date range.
 * @param {!DateLike} secondDate end date of the date range or an
 *     interval object.
 * @param {string} largestDifferentCalendarField represents biggest field
 * @param {!TimeZone=} opt_timeZone Timezone to be used in the target
 *     representation.
 * @return {string} Formatted date interval
 * @override
 */
IntervalFormatter_.prototype.format = function(
    firstDate, secondDate, largestDifferentCalendarField, opt_timeZone) {
  if (this.useFirstDateOnFirstPattern_) {
    return this.firstPartFormatter_.format(firstDate, opt_timeZone) +
        this.secondPartFormatter_.format(secondDate, opt_timeZone);
  } else {
    return this.firstPartFormatter_.format(secondDate, opt_timeZone) +
        this.secondPartFormatter_.format(firstDate, opt_timeZone);
  }
};

/**
 * Constructs a DateTimeFormatter_ object which implements the Formatter_
 * interface.
 *
 * Internal object to construct and store a goog.i18n.DateTimeFormat for the
 * a datetime pattern and formats dates using the fallback interval pattern
 * (e.g. '{0} – {1}').
 *
 * @param {string} dateTimePattern Datetime pattern used to format the dates.
 * @param {string} fallbackPattern Fallback interval pattern to be used with the
 *     datetime pattern.
 * @param {!DateTimeSymbolsType} dateTimeSymbols Symbols to use with
 *     the datetime format.
 * @constructor
 * @implements {Formatter_}
 * @private
 */
let DateTimeFormatter_ = function(
    dateTimePattern, fallbackPattern, dateTimeSymbols) {
  /**
   * Date time pattern used to format the dates.
   * @private @const {string}
   */
  this.dateTimePattern_ = dateTimePattern;

  /**
   * Date time formatter used to format the dates.
   * @private @const {!DateTimeFormat}
   */
  this.dateTimeFormatter_ =
      new DateTimeFormat(dateTimePattern, dateTimeSymbols);

  /**
   * Fallback interval pattern.
   * @private {string}
   */
  this.fallbackPattern_ = fallbackPattern;
};

/**
 * @param {!DateLike} firstDate Start date of the date range.
 * @param {!DateLike} secondDate end date of the date range or an
 *     interval object.
 * @param {string} largestDifferentCalendarField represents biggest field
 * @param {!TimeZone=} opt_timeZone Timezone to be used in the target
 *     representation.
 * @return {string} Formatted date interval
 * @override
 */
DateTimeFormatter_.prototype.format = function(
    firstDate, secondDate, largestDifferentCalendarField, opt_timeZone) {
  // Check if the largest different calendar field between the two dates is
  // larger or equal than any calendar field in the datetime pattern. If true,
  // format the string using the datetime pattern and the fallback interval
  // pattern.
  let shouldFormatWithFallbackPattern = false;
  if (largestDifferentCalendarField != '') {
    DateIntervalFormat.executeForEveryCalendarField_(
        this.dateTimePattern_, function(char, index) {
          if (DateIntervalFormat.isCalendarFieldLargerOrEqualThan_(
                  largestDifferentCalendarField, char)) {
            shouldFormatWithFallbackPattern = true;
            return false;
          }
          return true;
        });
  }

  if (shouldFormatWithFallbackPattern) {
    return this.fallbackPattern_
        .replace(
            FIRST_DATE_PLACEHOLDER_,
            this.dateTimeFormatter_.format(firstDate, opt_timeZone))
        .replace(
            SECOND_DATE_PLACEHOLDER_,
            this.dateTimeFormatter_.format(secondDate, opt_timeZone));
  }
  // If not, format the first date using the datetime pattern.
  return this.dateTimeFormatter_.format(firstDate, opt_timeZone);
};

exports = DateIntervalFormat;
