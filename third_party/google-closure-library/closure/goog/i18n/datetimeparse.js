/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Date/Time parsing library with locale support.
 */


/**
 * Namespace for locale date/time parsing functions
 */
goog.provide('goog.i18n.DateTimeParse');

goog.require('goog.asserts');
goog.require('goog.date');
goog.require('goog.i18n.DateTimeFormat.Format');
goog.require('goog.i18n.DateTimeSymbols');
goog.require('goog.i18n.DayPeriods');
goog.require('goog.object');
goog.requireType('goog.i18n.DateTimeSymbolsType');

goog.scope(function() {
// For referencing modules
const DayPeriods = goog.module.get('goog.i18n.DayPeriods');

/**
 * DateTimeParse is for parsing date in a locale-sensitive manner. It allows
 * user to use any customized patterns to parse date-time string under certain
 * locale. Things varies across locales like month name, weekname, field
 * order, etc.
 *
 * This module is the counter-part of DateTimeFormat. They use the same
 * date/time pattern specification, which is borrowed from ICU/JDK.
 *
 * This implementation could parse partial date/time.
 *
 * Time Format Syntax: To specify the time format use a time pattern string.
 * In this pattern, the following letters are reserved as pattern letters, which
 * are defined as follows:
 *
 * ```
 * Symbol   Meaning                 Presentation        Example
 * ------   -------                 ------------        -------
 * G?       era designator          (Text)              AD
 * y#       year                    (Number)            1996
 * M        month in year           (Text & Number)     July & 07
 * L        standalone month in year (Text & Number)    July & 07
 * d?       day in month            (Number)            10
 * h?       hour in am/pm (1~12)    (Number)            12
 * H?       hour in day (0~23)      (Number)            0
 * m?       minute in hour          (Number)            30
 * s?       second in minute        (Number)            55
 * S?       fractional second       (Number)            978
 * E        day of week             (Text)              Tuesday
 * D*       day in year             (Number)            189
 * a?       am/pm marker            (Text)              PM
 * b?#      am/pm/noon/midnight     (Text)              Noon
 * B?#      flexible day periods    (Text)              de l’après-midi'
 * k        hour in day (1~24)      (Number)            24
 * K        hour in am/pm (0~11)    (Number)            0
 * z?#      time zone               (Text)              Pacific Standard Time
 * Z?       time zone (RFC 822)     (Number) -0800
 * v?#      time zone (generic)     (Text)              Pacific Time
 * '        escape for text         (Delimiter)         'Date='
 * ''       single quote            (Literal)           'o''clock'
 * ```
 *
 * - Items marked with '*' are not supported yet.
 * - Items marked with '#' work different from java (i.e. may not support
 *   parsing more verbose formats/examples, such as "Noon" or "Pacific Time").
 * - Items marked with '?' can be omitted when at the end of a non-empty format
 *   string. Optional text items can also be omitted when abutting immediately
 *   before or after a numeric item (i.e. "Gy" and "yG" will both accept "1").
 *
 * The letter count and presentation together determine the accepted formats:
 * - **Text**: both full and abbreviated forms are allowed, with the long form
 *   being preferred.
 * - **Number**: the count indicates the maximum number of characters parsed in
 *   case of abutting numbers (the count is otherwise ignored, except for the
 *   special case of 'yy', discussed below).
 * - **Text & Number**: count of 3 or more requires the text form, otherwise
 *   allows either number or text.
 *
 * Any letters not in the above table should be treated as _reserved_: any
 * alphanumeric characters that must be matched literally should be wrapped in
 * single quotes to ensure any newly added format characters don't change the
 * meaning.  Punctuation (such as ':', '.', ' ', '#' and '@') may be left
 * unquoted and will still be treated as literals.
 *
 * Examples Using the US Locale:
 *
 * ```
 * Format Pattern                         Result
 * --------------                         -------
 * "yyyy.MM.dd G 'at' HH:mm:ss vvvv" ->>  1996.07.10 AD at 15:08:56 Pacific Time
 * "EEE, MMM d, ''yy"                ->>  Wed, July 10, '96
 * "h:mm a"                          ->>  12:08 PM
 * "hh 'o''clock' a, zzzz"           ->>  12 o'clock PM, Pacific Daylight Time
 * "K:mm a, vvv"                     ->>  0:00 PM, PT
 * "yyyyy.MMMMM.dd GGG hh:mm aaa"    ->>  01996.July.10 AD 12:08 PM
 * ```
 *
 * <p> When parsing a date string using the abbreviated year pattern ("yy"),
 * DateTimeParse must interpret the abbreviated year relative to some
 * century. It does this by adjusting dates to be within 80 years before and 20
 * years after the time the parse function is called. For example, using a
 * pattern of "MM/dd/yy" and a DateTimeParse instance created on Jan 1, 1997,
 * the string "01/11/12" would be interpreted as Jan 11, 2012 while the string
 * "05/04/64" would be interpreted as May 4, 1964. During parsing, only
 * strings consisting of exactly two digits, as defined by {@link
 * java.lang.Character#isDigit(char)}, will be parsed into the default
 * century. Any other numeric string, such as a one digit string, a three or
 * more digit string will be interpreted as its face value.
 *
 * <p> If the year pattern does not have exactly two 'y' characters, the year is
 * interpreted literally, regardless of the number of digits. So when using the
 * patterns "MM/dd/yyyy" or "M/D/y", "01/11/12" parses to Jan 11, 12 A.D.
 *
 * <p> When numeric fields abut one another directly, with no intervening
 * delimiter characters, they constitute a run of abutting numeric fields. Such
 * runs are parsed specially. For example, the format "HHmmss" parses the input
 * text "123456" to 12:34:56, parses the input text "12345" to 1:23:45, and
 * fails to parse "1234". In other words, the leftmost field of the run is
 * flexible (i.e. it may be shorter), while the others keep a fixed width. If
 * the parse fails anywhere in the run, then the leftmost field is shortened by
 * one character, and the entire run is parsed again. This is repeated until
 * either the parse succeeds or the leftmost field is one character in length.
 * If the parse still fails at that point, the parse of the run fails.
 *
 * <p> Now timezone parsing only support GMT:hhmm, GMT:+hhmm, GMT:-hhmm
 */

/**
 * Components of patternParts_ object.
 * @private @typedef {{
 *    text: (string|undefined),
 *    count: (number),
 *    numeric: (boolean|undefined),
 *    abutStart: (boolean|undefined),
 * }}
 */
goog.i18n.DateTimeParse.PatternPart;

// To match one white horizontal space character at start of string, including
// non-ASCII.
const horizontalWhiteSpacePrefixRegex =
    /^[ \t\xA0\u1680\u180e\u2000-\u200a\u202f\u205f\u3000]/;
// Match one or more white space, including those covered by JavaScript's \s
// match.
const skipWhiteSpacePrefixRegex =
    /^[\s\xA0\u1680\u180e\u2000-\u200a\u202f\u205f\u3000]+/;

/**
 * Construct a DateTimeParse based on current locale.
 * @param {string|number} pattern pattern specification or pattern type.
 * @param {!Object=} opt_dateTimeSymbols Optional symbols to use for this
 *     instance rather than the global symbols.
 * @constructor
 * @final
 */
goog.i18n.DateTimeParse = function(pattern, opt_dateTimeSymbols) {
  'use strict';
  goog.asserts.assert(
      opt_dateTimeSymbols !== undefined ||
          goog.i18n.DateTimeSymbols !== undefined,
      'goog.i18n.DateTimeSymbols or explicit symbols must be defined');

  /** @const @private {!Array<!goog.i18n.DateTimeParse.PatternPart>}; */
  this.patternParts_ = [];

  /**
   * Data structure with all the locale info needed for date formatting.
   * (day/month names, most common patterns, rules for week-end, etc.)
   * @const @private {!goog.i18n.DateTimeSymbolsType}
   */
  this.dateTimeSymbols_ = /** @type {!goog.i18n.DateTimeSymbolsType} */ (
      opt_dateTimeSymbols || goog.i18n.DateTimeSymbols);
  if (typeof pattern == 'number') {
    this.applyStandardPattern_(pattern);
  } else {
    this.applyPattern_(pattern);
  }
};


/**
 * Number of years prior to now that the century used to
 * disambiguate two digit years will begin
 *
 * @type {number}
 */
goog.i18n.DateTimeParse.ambiguousYearCenturyStart = 80;


/**
 * All acceptable pattern characters.
 * @private
 * @const
 */
goog.i18n.DateTimeParse.PATTERN_CHARS_ = 'GyMdkHmsSEDabBhKzZvQL';


/**
 * Pattern characters that specify numerical field.
 * @private
 * @const
 */
goog.i18n.DateTimeParse.NUMERIC_FORMAT_CHARS_ = 'MydhHmsSDkK';


/**
 * Pattern characters supported by predictive parsing.
 * @private
 * @const
 */
goog.i18n.DateTimeParse.PREDICTIVE_FORMAT_CHARS_ = 'abBhHkKm';


/**
 * Apply a pattern to this Parser. The pattern string will be parsed and saved
 * in "compiled" form.
 * Note: this method is somewhat similar to the pattern parsing method in
 *       datetimeformat. If you see something wrong here, you might want
 *       to check the other.
 * @param {string} pattern It describes the format of date string that need to
 *     be parsed.
 * @private
 */
goog.i18n.DateTimeParse.prototype.applyPattern_ = function(pattern) {
  'use strict';
  let inQuote = false;
  let buf = '';

  for (let i = 0; i < pattern.length; i++) {
    const ch = pattern.charAt(i);

    // handle whitespace, add literal part (if exist), and add space part
    if (horizontalWhiteSpacePrefixRegex.test(ch)) {
      if (buf.length > 0) {
        this.patternParts_.push(
            {text: buf, count: 0, abutStart: false, numeric: false});
        buf = '';
      }
      this.patternParts_.push(
          {text: ' ', count: 0, abutStart: false, numeric: false});
      while (i < pattern.length - 1 && pattern.charAt(i + 1) == ' ') {
        i++;
      }
    } else if (inQuote) {
      // inside quote, except '', just copy or exit
      if (ch == '\'') {
        if (i + 1 < pattern.length && pattern.charAt(i + 1) == '\'') {
          // quote appeared twice continuously, interpret as one quote.
          buf += '\'';
          i++;
        } else {
          // exit quote
          inQuote = false;
        }
      } else {
        // literal
        buf += ch;
      }
    } else if (goog.i18n.DateTimeParse.PATTERN_CHARS_.indexOf(ch) >= 0) {
      // outside quote, it is a pattern char
      if (buf.length > 0) {
        this.patternParts_.push(
            {text: buf, count: 0, abutStart: false, numeric: false});
        buf = '';
      }
      const count = this.getNextCharCount_(pattern, i);
      const numeric = this.isNumericField_(ch, count);
      this.patternParts_.push(
          {text: ch, count: count, abutStart: false, numeric: numeric});
      i += count - 1;
    } else if (ch == '\'') {
      // Two consecutive quotes is a quote literal, inside or outside of quotes.
      if (i + 1 < pattern.length && pattern.charAt(i + 1) == '\'') {
        buf += '\'';
        i++;
      } else {
        inQuote = true;
      }
    } else {
      buf += ch;
    }
  }

  if (buf.length > 0) {
    this.patternParts_.push(
        {text: buf, count: 0, abutStart: false, numeric: false});
  }

  this.markAbutStart_();
};


/**
 * Apply a predefined pattern to this Parser.
 * @param {number} formatType A constant used to identified the predefined
 *     pattern string stored in locale repository.
 * @private
 */
goog.i18n.DateTimeParse.prototype.applyStandardPattern_ = function(formatType) {
  'use strict';
  let pattern;
  // formatType constants are in consecutive numbers. So it can be used to
  // index array in following way.

  // if type is out of range, default to medium date/time format.
  if (formatType > goog.i18n.DateTimeFormat.Format.SHORT_DATETIME) {
    formatType = goog.i18n.DateTimeFormat.Format.MEDIUM_DATETIME;
  }

  if (formatType < 4) {
    pattern = this.dateTimeSymbols_.DATEFORMATS[formatType];
  } else if (formatType < 8) {
    pattern = this.dateTimeSymbols_.TIMEFORMATS[formatType - 4];
  } else {
    pattern = this.dateTimeSymbols_.DATETIMEFORMATS[formatType - 8];
    pattern = pattern.replace(
        '{1}', this.dateTimeSymbols_.DATEFORMATS[formatType - 8]);
    pattern = pattern.replace(
        '{0}', this.dateTimeSymbols_.TIMEFORMATS[formatType - 8]);
  }
  this.applyPattern_(pattern);
};


/**
 * Calculate character repeat count in pattern.
 *
 * @param {string} pattern It describes the format of date string that need to
 *     be parsed.
 * @param {number} start The position of pattern character.
 *
 * @return {number} Repeat count.
 * @private
 */
goog.i18n.DateTimeParse.prototype.getNextCharCount_ = function(pattern, start) {
  'use strict';
  const ch = pattern.charAt(start);
  let next = start + 1;
  while (next < pattern.length && pattern.charAt(next) == ch) {
    next++;
  }
  return next - start;
};


/**
 * Identify the start of an abutting numeric fields' run. Taking pattern
 * "HHmmss" as an example. It will try to parse 2/2/2 characters of the input
 * text, then if that fails, 1/2/2. We only adjust the width of the leftmost
 * field; the others remain fixed. This allows "123456" => 12:34:56, but
 * "12345" => 1:23:45. Likewise, for the pattern "yyyyMMdd" we try 4/2/2,
 * 3/2/2, 2/2/2, and finally 1/2/2. The first field of connected numeric
 * fields will be marked as abutStart, its width can be reduced to accommodate
 * others.
 *
 * @private
 */
goog.i18n.DateTimeParse.prototype.markAbutStart_ = function() {
  'use strict';
  // abut parts are continuous numeric parts. abutStart is the switch
  // point from non-abut to abut
  let abut = false;

  for (let i = 0; i < this.patternParts_.length; i++) {
    if (this.patternParts_[i].numeric) {
      // if next part is not following abut sequence, and is numeric
      if (!abut && i + 1 < this.patternParts_.length &&
          this.patternParts_[i + 1].numeric) {
        abut = true;
        this.patternParts_[i].abutStart = true;
      }
    } else {
      abut = false;
    }
  }
};


/**
 * Check if the pattern part is a numeric field.
 *
 * @param {string} ch pattern character
 * @param {number} count pattern character count
 *
 * @return {boolean} true if the pattern part is numeric field.
 * @private
 */
goog.i18n.DateTimeParse.prototype.isNumericField_ = function(ch, count) {
  'use strict';
  if (count <= 0) {
    return false;
  }
  const i = goog.i18n.DateTimeParse.NUMERIC_FORMAT_CHARS_.indexOf(ch);
  return i > 0 || i == 0 && count < 3;
};


/**
 * Assert this object's pattern supports predictive parsing.
 * @private
 */
goog.i18n.DateTimeParse.prototype.assertPatternSupportsPredictive_ =
    function() {
  'use strict';
  for (let i = 0; i < this.patternParts_.length; i++) {
    const part = this.patternParts_[i];
    if (part.count > 0 &&
        (goog.i18n.DateTimeParse.PREDICTIVE_FORMAT_CHARS_.indexOf(
             part.text.charAt(0)) < 0 ||
         part.count > 2 || part.abutStart)) {
      throw new Error(
          `'predictive' parsing is not supported for symbol ` +
          `'${part.text.charAt(0)}'.`);
    }
  }
};


/**
 * Parse the given string and fill parsed values into date object.
 *
 * The existing values of any temporal fields of `date` not parsed from `text`
 * are not directly changed, but may be affected by overflow. E.g. if a minutes
 * value of 70 is parsed, the implementation of `date` may increment the value
 * of its hours field by 1 while setting its minutes value to 10.
 *
 * @param {string} text The string being parsed.
 * @param {?goog.date.DateLike} date The Date object to hold the parsed date.
 * @param {!goog.i18n.DateTimeParse.ParseOptions=} options The options object.
 * @return {number} How many characters parser advanced.
 */
goog.i18n.DateTimeParse.prototype.parse = function(text, date, options) {
  'use strict';
  let predictive = false;
  let validate = false;
  if (options) {
    predictive = options.predictive || false;
    validate = options.validate || false;
  }

  if (predictive) {
    this.assertPatternSupportsPredictive_();
  }

  const cal = new goog.i18n.DateTimeParse.MyDate_();
  const parsePos = [0];
  cal.dayPeriodIndex = -1;

  for (let i = 0; i < this.patternParts_.length; i++) {
    if (predictive && parsePos[0] >= text.length) {
      break;
    }

    if (this.patternParts_[i].count == 0) {
      // Handle literal pattern characters.
      if (this.subParseLiteral_(
              text, parsePos, this.patternParts_[i], predictive)) {
        continue;
      }
      return 0;
    }

    if (this.patternParts_[i].abutStart) {
      // Handle fields within a run of abutting numeric fields.
      const partsParsed = this.subParseAbut_(text, parsePos, i, cal);
      if (partsParsed <= 0) {
        return 0;
      }
      i += partsParsed - 1;
      continue;
    }

    // Handle non-numeric fields and non-abutting numeric fields.
    if (!this.subParse_(
            text, parsePos, this.patternParts_[i], 0, cal, predictive)) {
      return 0;
    }
  }

  // return progress
  return cal.calcDate_(date, validate) ? parsePos[0] : 0;
};

/**
 * Parse a run of abutting numeric pattern parts. Take the pattern "HHmmss" as
 * an example. We will try to parse 2/2/2 characters of the input text, then if
 * that fails, 1/2/2. We only adjust the width of the leftmost field; the others
 * remain fixed. This allows "123456" => 12:34:56, but "12345" => 1:23:45.
 * Likewise, for the pattern "yyyyMMdd" we try 4/2/2, 3/2/2, 2/2/2, and finally
 * 1/2/2.
 *
 * @param {string} text the text to be parsed.
 * @param {!Array<number>} pos parse position
 * @param {number} abutStart the index of the pattern part starting the run
 * @param {!goog.i18n.DateTimeParse.MyDate_} cal object that holds parsed value
 *
 * @return {number} how many pattern parts the parser advanced
 * @private
 */
goog.i18n.DateTimeParse.prototype.subParseAbut_ = function(
    text, pos, abutStart, cal) {
  'use strict';
  const start = pos[0];
  let abutPass = 0;

  let i;
  for (i = abutStart; i < this.patternParts_.length; i++) {
    const part = this.patternParts_[i];
    let count = part.count;
    if (count === 0) {
      // Literal pattern parts end abutting runs of numeric fields. Non-numeric
      // fields do not (possibly a bug or undocumented constraint).
      break;
    }
    // If we are at the start of a run of abutting fields, then
    // shorten this field in each pass. If we can't shorten
    // this field any more, then the parse of this set of
    // abutting numeric fields has failed.
    if (i == abutStart) {
      count -= abutPass;
      abutPass++;
      if (count == 0) {
        // tried all possible width, fail now
        return 0;
      }
    }

    // Predictive parsing is not supported for abutting runs of numbers.  We
    // also enforce that subsequent parts must parse the full number of chars.
    const requireLength = i > abutStart && part.numeric;
    const partStart = pos[0];
    const subParsed = this.subParse_(
        text, pos, part, count, cal,
        /* predictive= */ false);
    if (!subParsed || (requireLength && pos[0] - partStart < count)) {
      // If the parse fails anywhere in the run, back up to the
      // start of the run and retry.
      i = abutStart - 1;
      pos[0] = start;
      continue;
    }
  }

  return i - abutStart;
};


/**
 * Protected method that converts one field of the input string into a
 * numeric field value.
 *
 * @param {string} text the text to be parsed.
 * @param {!Array<number>} pos Parse position.
 * @param {!goog.i18n.DateTimeParse.PatternPart} part the pattern part for this
 *     field.
 * @param {number} digitCount when > 0, numeric parsing must obey the count.
 * @param {!goog.i18n.DateTimeParse.MyDate_} cal object that holds parsed value.
 * @param {boolean} predictive whether to apply predictive parsing rules.
 *
 * @return {boolean} True if it parses successfully.
 * @private
 */
goog.i18n.DateTimeParse.prototype.subParse_ = function(
    text, pos, part, digitCount, cal, predictive) {
  'use strict';
  this.skipSpace_(text, pos);

  if (part.numeric && digitCount > 0) {
    if ((pos[0] + digitCount) > text.length) {
      return false;
    }
  }

  switch (part.text.charAt(0)) {
    case 'G':  // ERA
      this.subParseString_(
          text, pos, [this.dateTimeSymbols_.ERAS], value => cal.era = value);
      return true;
    case 'M':  // MONTH
    case 'L':  // STANDALONEMONTH
      return this.subParseMonth_(text, pos, digitCount, part, cal);
    case 'E':  // DAY_OF_WEEK
      // Handle both short and long forms. Try count == 4 first.
      const weekdays =
          [this.dateTimeSymbols_.WEEKDAYS, this.dateTimeSymbols_.SHORTWEEKDAYS];
      return this.subParseString_(
          text, pos, weekdays, value => cal.dayOfWeek = value);
    case 'B':  // Flexible day peridos
    case 'b':  // Day periods of 'noon', 'midnight', 'am', or 'pm'
      // TODO b/206042104: update to handle parsing day periods with 'b' and 'B'
      // Get the strings for the day periods for adjusting the time to AM/PM.
      // Also includes AM/PM data for fallback.
      /** {?goog.i18n.DayPeriods} */
      const localePeriods = DayPeriods.getDayPeriods();
      // Standard names such as 'noon', 'morning1', 'night2', etc.
      let periodNames = [];
      // The localized terms for the period names.
      let expectedValues = [];
      if (localePeriods) {
        // Get the formatNames values to check
        for (const name of goog.object.getKeys(localePeriods)) {
          periodNames.push(localePeriods[name].periodName);
          expectedValues.push(localePeriods[name].formatNames[0]);
        }
      }
      // Add strings for AM & PM, in addition to flexible periods
      const periodsData = [expectedValues.concat(this.dateTimeSymbols_.AMPMS)];
      // Include possible outputs of am/pm as day periods.
      periodNames.push('isAm');
      periodNames.push('isPm');

      // Record string matching this day period
      const foundPeriod = this.subParseString_(
          text, pos, periodsData, value => cal.dayPeriodIndex = value,
          predictive);
      cal.dayPeriodName = periodNames[cal.dayPeriodIndex];

      return predictive ? foundPeriod : true;
    case 'a':  // AM_PM
      const success = this.subParseString_(
          text, pos, [this.dateTimeSymbols_.AMPMS], value => cal.ampm = value,
          predictive);
      return predictive ? success : true;
    case 'y':  // YEAR
      return this.subParseYear_(text, pos, part, digitCount, cal);
    case 'Q':  // QUARTER
      // Handle both short and long forms. Try count == 4 first.
      const quarters =
          [this.dateTimeSymbols_.QUARTERS, this.dateTimeSymbols_.SHORTQUARTERS];
      return this.subParseString_(text, pos, quarters, function(value) {
        cal.month = value * 3;  // First month of quarter.
        cal.day = 1;
      });
    case 'd':  // DATE
      this.subParseInt_(text, pos, part, digitCount, function(value) {
        cal.day = value;
      });
      return true;
    case 'S':  // FRACTIONAL_SECOND
      return this.subParseFractionalSeconds_(text, pos, digitCount, cal);
    case 'h':  // HOUR (1..12)
    case 'K':  // HOUR (0..11)
    case 'H':  // HOUR_OF_DAY (0..23)
    case 'k':  // HOUR_OF_DAY (1..24)
    {
      const success =
          this.subParseInt_(text, pos, part, digitCount, function(value) {
            cal.hours =
                (part.text.charAt(0) === 'h' && value === 12) ? 0 : value;
          }, predictive);
      return predictive ? success : true;
    }
    case 'm':  // MINUTE
    {
      const success =
          this.subParseInt_(text, pos, part, digitCount, function(value) {
            cal.minutes = value;
          }, predictive);
      return predictive ? success : true;
    }
    case 's':  // SECOND
      this.subParseInt_(text, pos, part, digitCount, function(value) {
        cal.seconds = value;
      });
      return true;
    case 'z':  // ZONE_OFFSET
    case 'Z':  // TIMEZONE_RFC
    case 'v':  // TIMEZONE_GENERIC
      return this.subParseTimeZoneInGMT_(text, pos, cal);
    default:
      return false;
  }
};


/**
 * Parse year field. Year field is special because
 * 1) two digit year need to be resolved.
 * 2) we allow year to take a sign.
 * 3) year field participate in abut processing.
 *
 * @param {string} text the text to be parsed.
 * @param {!Array<number>} pos Parse position.
 * @param {!goog.i18n.DateTimeParse.PatternPart} part the pattern part for this
 *     field.
 * @param {number} digitCount when > 0, numeric parsing must obey the count.
 * @param {!goog.i18n.DateTimeParse.MyDate_} cal object to hold parsed value.
 *
 * @return {boolean} True if successful.
 * @private
 */
goog.i18n.DateTimeParse.prototype.subParseYear_ = function(
    text, pos, part, digitCount, cal) {
  'use strict';
  const start = pos[0];
  // This awkward implementation preserves an existing behavioral quirk.
  // digitCount (for abutting patterns) is ignored for signed years.
  let value = this.parseInt_(text, pos, digitCount);
  if (value === null) {
    value = this.parseInt_(text, pos, 0, /* allowSigned= */ true);
  }
  if (value === null) {
    return false;
  }

  // only if 2 digit was actually parsed, and pattern say it has 2 digit.
  if (value >= 0 && pos[0] - start == 2 && part.count == 2) {
    cal.setTwoDigitYear_(value);
  } else {
    cal.year = value;
  }
  return true;
};


/**
 * Parse Month field.
 *
 * @param {string} text the text to be parsed.
 * @param {!Array<number>} pos Parse position.
 * @param {number} digitCount when > 0, numeric parsing must obey the count.
 * @param {!goog.i18n.DateTimeParse.PatternPart} part the pattern part
 * @param {!goog.i18n.DateTimeParse.MyDate_} cal object to hold parsed value.
 *
 * @return {boolean} True if parsing successful.
 * @private
 */
goog.i18n.DateTimeParse.prototype.subParseMonth_ = function(
    text, pos, digitCount, part, cal) {
  'use strict';
  if (part.numeric &&
      this.subParseInt_(text, pos, part, digitCount, function(value) {
        cal.month = value - 1;
      })) {
    return true;
  }

  // month is symbols, i.e., MMM, MMMM, LLL or LLLL
  // Handle both short and long forms. Try count == 4 first.
  const months = [
    this.dateTimeSymbols_.MONTHS, this.dateTimeSymbols_.STANDALONEMONTHS,
    this.dateTimeSymbols_.SHORTMONTHS,
    this.dateTimeSymbols_.STANDALONESHORTMONTHS
  ];
  return this.subParseString_(text, pos, months, function(value) {
    cal.month = value;
  });
};


/**
 * Parse fractional seconds field.
 *
 * @param {string} text the text to be parsed.
 * @param {!Array<number>} pos current parse position.
 * @param {number} digitCount when > 0, numeric parsing must obey the count.
 * @param {!goog.i18n.DateTimeParse.MyDate_} cal object to hold parsed value.
 *
 * @return {boolean} True if successful.
 * @private
 */
goog.i18n.DateTimeParse.prototype.subParseFractionalSeconds_ = function(
    text, pos, digitCount, cal) {
  'use strict';
  const start = pos[0];
  const value = this.parseInt_(text, pos, digitCount);
  if (value === null) {
    return false;
  }
  // Fractional seconds left-justify
  const len = pos[0] - start;
  cal.milliseconds = len < 3 ? value * Math.pow(10, 3 - len) :
                               Math.round(value / Math.pow(10, len - 3));
  return true;
};


/**
 * Parse GMT type timezone.
 *
 * @param {string} text the text to be parsed.
 * @param {!Array<number>} pos Parse position.
 * @param {!goog.i18n.DateTimeParse.MyDate_} cal object to hold parsed value.
 *
 * @return {boolean} True if successful.
 * @private
 */
goog.i18n.DateTimeParse.prototype.subParseTimeZoneInGMT_ = function(
    text, pos, cal) {
  'use strict';
  // First try to parse generic forms such as GMT-07:00. Do this first
  // in case localized DateFormatZoneData contains the string "GMT"
  // for a zone; in that case, we don't want to match the first three
  // characters of GMT+/-HH:MM etc.

  // For time zones that have no known names, look for strings
  // of the form:
  //    GMT[+-]hours:minutes or
  //    GMT[+-]hhmm or
  //    GMT.
  if (text.indexOf('GMT', pos[0]) == pos[0]) {
    pos[0] += 3;  // 3 is the length of GMT
  }

  // TODO(user): check for named time zones by looking through the locale
  // data from the DateFormatZoneData strings. Should parse both short and long
  // forms.
  // subParseZoneString(text, start, cal);

  // As a last resort, look for numeric timezones of the form
  // [+-]hhmm as specified by RFC 822.  This code is actually
  // a little more permissive than RFC 822.  It will try to do
  // its best with numbers that aren't strictly 4 digits long.

  if (pos[0] >= text.length) {
    cal.tzOffset = 0;
    return true;
  }

  // Look for hours:minutes or hhmm.
  const start = pos[0];
  let value =
      this.parseInt_(text, pos, /* digitCount= */ 0, /* allowSigned= */ true);
  if (value === null) {
    return false;
  }

  let offset;
  if (pos[0] < text.length && text.charAt(pos[0]) == ':') {
    // This is the hours:minutes case
    offset = value * 60;
    pos[0]++;
    value = this.parseInt_(text, pos, /* digitCount= */ 0);
    if (value === null) {
      return false;
    }
    offset += value;
  } else {
    // This is the hhmm case.
    offset = value;
    // Assume "-23".."+23" refers to hours.
    if (offset < 24 && (pos[0] - start) <= 3) {
      offset *= 60;
    } else {
      // todo: this looks questionable, should have more error checking
      offset = offset % 100 + offset / 100 * 60;
    }
  }

  cal.tzOffset = -offset;
  return true;
};


/**
 * Parse unsigned integer pattern characters. These are symbols such as 'd' for
 * date.
 *
 * @param {string} text the text to be parsed.
 * @param {!Array<number>} pos parse position
 * @param {!goog.i18n.DateTimeParse.PatternPart} part the pattern part for this
 *     field.
 * @param {number} maxChars when > 0, at most this many characters are parsed.
 * @param {function(number)} callback function to record the parsed value.
 * @param {boolean=} predictive whether to apply predictive parsing rules.
 *     defaults to false
 *
 * @return {boolean} True if it parses successfully.
 * @private
 */
goog.i18n.DateTimeParse.prototype.subParseInt_ = function(
    text, pos, part, maxChars, callback, predictive) {
  'use strict';
  predictive = predictive || false;
  const start = pos[0];
  let value = this.parseInt_(text, pos, maxChars);
  if (value === null) {
    return false;
  } else if (predictive && pos[0] - start < part.count) {
    if (pos[0] < text.length) {
      return false;
    }
    // Infer trailing 0s to match pattern length.
    value *= Math.pow(10, part.count - (pos[0] - start));
  }
  callback(value);
  return true;
};


/**
 * Parse string pattern characters. These are symbols matching a set of strings
 * such as 'E' for day of week.
 *
 * @param {string} text the text to be parsed.
 * @param {!Array<number>} pos parse position
 * @param {!Array<!Array<string>>} data Arrays of strings to match against,
 *     sequentially.
 * @param {function(number)} callback function to record the parsed value.
 * @param {boolean=} predictive whether to apply predictive parsing rules.
 *     defaults to false
 *
 * @return {boolean} True iff the input matches any of the strings in the data
 *     arrays.
 * @private
 */
goog.i18n.DateTimeParse.prototype.subParseString_ = function(
    text, pos, data, callback, predictive) {
  'use strict';
  predictive = predictive || false;
  let value = null;
  for (let i = 0; i < data.length; i++) {
    value = this.matchString_(text, pos, data[i], predictive);
    if (value !== null) {
      callback(value);
      return true;
    }
  }
  return false;
};


/**
 * Parse literal pattern characters. These are any quoted characters and non-
 * alphabetic unquoted characters.
 *
 * @param {string} text the text to be parsed.
 * @param {!Array<number>} pos parse position
 * @param {!goog.i18n.DateTimeParse.PatternPart} part the pattern part
 * @param {boolean} predictive whether to apply predictive parsing rules.
 *
 * @return {boolean} True if it parses successfully.
 * @private
 */
goog.i18n.DateTimeParse.prototype.subParseLiteral_ = function(
    text, pos, part, predictive) {
  'use strict';
  // A run of white space in the pattern matches a run
  // of white space in the input text.
  const white_space_match = part.text.match(horizontalWhiteSpacePrefixRegex);
  if (white_space_match != null) {
    const start = pos[0];
    this.skipSpace_(text, pos);

    // Must see at least one white space char in input
    if (pos[0] > start) {
      return true;
    }
  } else if (text.indexOf(part.text, pos[0]) == pos[0]) {
    pos[0] += part.text.length;
    return true;
  } else if (predictive && part.text.indexOf(text.substring(pos[0])) == 0) {
    // For predictive matching, if the remaining text is a prefix of the
    // string literal pattern part, accept it as a match.
    pos[0] += text.length - pos[0];
    return true;
  }
  // We fall through to this point if the match fails
  return false;
};


/**
 * Skip space in the string.
 *
 * @param {string} text input string.
 * @param {!Array<number>} pos where skip start, and return back where the skip
 *     stops.
 * @private
 */
goog.i18n.DateTimeParse.prototype.skipSpace_ = function(text, pos) {
  'use strict';
  // Skips Unicode spaces in addition to ASCII space.
  const m = text.substring(pos[0]).match(skipWhiteSpacePrefixRegex);
  if (m) {
    pos[0] += m[0].length;
  }
};


/**
 * Parse an integer string and return integer value.
 *
 * @param {string} text string being parsed.
 * @param {!Array<number>} pos parse position.
 * @param {number} maxChars when > 0, at most this many characters are parsed.
 * @param {boolean=} allowSigned if true allows a single leading sign character
 *     (+|-) in the input. defaults to false
 *
 * @return {?number} integer value, or null if the integer cannot be parsed
 * @private
 */
goog.i18n.DateTimeParse.prototype.parseInt_ = function(
    text, pos, maxChars, allowSigned) {
  'use strict';
  text = maxChars > 0 ? text.substring(0, pos[0] + maxChars) : text;
  allowSigned = allowSigned || false;
  // Delocalizes the string containing native digits specified by the locale,
  // replaces the native digits with ASCII digits. Leaves other characters.
  // This is the reverse operation of localizeNumbers_ in datetimeformat.js.
  if (this.dateTimeSymbols_.ZERODIGIT) {
    const parts = [];
    for (let i = pos[0]; i < text.length; i++) {
      const c = text.charCodeAt(i) - this.dateTimeSymbols_.ZERODIGIT;
      parts.push(
          (0 <= c && c <= 9) ? String.fromCharCode(c + 0x30) : text.charAt(i));
    }
    text = parts.join('');
  } else {
    text = text.substring(pos[0]);
  }

  const signRe = allowSigned ? '[+-]?' : '';
  const re = new RegExp(`^${signRe}\\d+`);
  const m = text.match(re);
  if (!m) {
    return null;
  }
  pos[0] += m[0].length;
  return parseInt(m[0], 10);
};


/**
 * Attempt to match the text at a given position against an array of strings.
 * Since multiple strings in the array may match (for example, if the array
 * contains "a", "ab", and "abc", all will match the input string "abcd") the
 * longest match is returned.
 *
 * @param {string} text The string to match to.
 * @param {!Array<number>} pos parsing position.
 * @param {!Array<string>} data The string array of matching patterns.
 * @param {boolean} predictive whether to apply predictive parsing rules.
 *
 * @return {?number} the index of the best match in the array, or null
 *     indicating matching failure.
 * @private
 */
goog.i18n.DateTimeParse.prototype.matchString_ = function(
    text, pos, data, predictive) {
  'use strict';
  // There may be multiple strings in the data[] array which begin with
  // the same prefix (e.g., Cerven and Cervenec (June and July) in Czech).
  // We keep track of the longest match, and return that. Note that this
  // unfortunately requires us to test all array elements.
  let bestMatchLength = 0;
  let bestMatchIndex = null;
  const lowerText = text.substring(pos[0]).toLowerCase();
  for (let i = 0; i < data.length; i++) {
    const lowerData = data[i].toLowerCase();
    // For predictive parsing the first data string matching the remainder of
    // the text is considered a match.
    if (predictive && lowerData.indexOf(lowerText) == 0) {
      bestMatchLength = lowerText.length;
      bestMatchIndex = i;
      break;
    }
    // Always compare if we have no match yet; otherwise only compare
    // against potentially better matches (longer strings).
    if (data[i].length > bestMatchLength && lowerText.indexOf(lowerData) == 0) {
      bestMatchLength = data[i].length;
      bestMatchIndex = i;
    }
  }
  if (bestMatchIndex !== null) {
    pos[0] += bestMatchLength;
  }
  return bestMatchIndex;
};


/**
 * Options object for calls to DateTimeParse.prototype.parse.
 * @record
 */
goog.i18n.DateTimeParse.ParseOptions = function() {
  'use strict';
  /**
   * Whether to use predictive parsing. Predictive parsing generates successful
   * parse results for non-empty partial inputs that have valid completions for
   * the parse pattern.
   *
   * If the end of the input is reached while parsing, numeric patterns parts
   * such as "HH" will infer values by padding with trailing 0s, and string
   * pattern parts such as "a" will attempt to prefix match valid values. E.g.
   * for the pattern "HH:mm" the input "12:3" will parse to a value equivalent
   * to "12:30" and for the pattern "h:mm a" the input "12:34 p" will parse to
   * a value equivalent to "12:30 PM".
   *
   * Predictive parsing currently only supports the pattern symbols "ahHkKm"
   * and is not compatible with abutting number patterns. Attempting to parse
   * using the predictive option with unsupported patterns will result in an
   * error being thrown.
   *
   * Defaults to false.
   *
   * @type {boolean|undefined}
   */
  this.predictive;
  /**
   * Whether the parsed date/time value should be validated.
   *
   * When true, parsing will fail if any of the parsed fields overflow, e.g.
   * minutes > 60.
   *
   * Note that setting `validate` to `false` does _not_ mean that parsing will
   * not fail: if the parser encounters an unknown month or day-of-week name
   * (for instance) then parsing will still fail. This only checks for overflow.
   *
   * Defaults to false.
   *
   * @type {boolean|undefined}
   */
  this.validate;
};


/**
 * This class hold the intermediate parsing result. After all fields are
 * consumed, final result will be resolved from this class.
 * @constructor
 * @private
 */
goog.i18n.DateTimeParse.MyDate_ = function() {};


/**
 * The date's era.
 * @type {?number}
 */
goog.i18n.DateTimeParse.MyDate_.prototype.era;


/**
 * The date's year.
 * @type {?number}
 */
goog.i18n.DateTimeParse.MyDate_.prototype.year;


/**
 * The date's month.
 * @type {?number}
 */
goog.i18n.DateTimeParse.MyDate_.prototype.month;


/**
 * The date's day of month.
 * @type {?number}
 */
goog.i18n.DateTimeParse.MyDate_.prototype.day;


/**
 * The date's hour.
 * @type {?number}
 */
goog.i18n.DateTimeParse.MyDate_.prototype.hours;


/**
 * The date's before/afternoon denominator.
 * @type {?number}
 */
goog.i18n.DateTimeParse.MyDate_.prototype.ampm;

/**
 * Index of the time's flexible day period in data object.
 * @type {number}
 */
goog.i18n.DateTimeParse.MyDate_.prototype.dayPeriodIndex;

/**
 * The name for flexible time of day.
 * @type {?string}
 */
goog.i18n.DateTimeParse.MyDate_.prototype.dayPeriodName;

/**
 * The date's minutes.
 * @type {?number}
 */
goog.i18n.DateTimeParse.MyDate_.prototype.minutes;


/**
 * The date's seconds.
 * @type {?number}
 */
goog.i18n.DateTimeParse.MyDate_.prototype.seconds;


/**
 * The date's milliseconds.
 * @type {?number}
 */
goog.i18n.DateTimeParse.MyDate_.prototype.milliseconds;


/**
 * The date's timezone offset.
 * @type {?number}
 */
goog.i18n.DateTimeParse.MyDate_.prototype.tzOffset;


/**
 * The date's day of week. Sunday is 0, Saturday is 6.
 * @type {?number}
 */
goog.i18n.DateTimeParse.MyDate_.prototype.dayOfWeek;


/**
 * Flag indicating if a provided two digit year needs to
 * be disambiguated
 *
 * @type {boolean}
 */
goog.i18n.DateTimeParse.MyDate_.prototype.ambiguousYear = false;


/**
 * 2 digit year special handling. Assuming for example that the
 * defaultCenturyStart is 6/18/1903. This means that two-digit years will be
 * forced into the range 6/18/1903 to 6/17/2003. As a result, years 00, 01, and
 * 02 correspond to 2000, 2001, and 2002. Years 04, 05, etc. correspond
 * to 1904, 1905, etc. If the year is 03, then it is 2003 if the
 * other fields specify a date before 6/18, or 1903 if they specify a
 * date afterwards. As a result, 03 is an ambiguous year. All other
 * two-digit years are unambiguous.
 *
 * @param {number} year 2 digit year value before adjustment.
 * @return {number} disambiguated year.
 * @private
 */
goog.i18n.DateTimeParse.MyDate_.prototype.setTwoDigitYear_ = function(year) {
  'use strict';
  const now = new Date();
  const defaultCenturyStartYear =
      now.getFullYear() - goog.i18n.DateTimeParse.ambiguousYearCenturyStart;
  const ambiguousTwoDigitYear = defaultCenturyStartYear % 100;
  this.ambiguousYear = (year == ambiguousTwoDigitYear);
  year += Math.floor(defaultCenturyStartYear / 100) * 100 +
      (year < ambiguousTwoDigitYear ? 100 : 0);
  return this.year = year;
};


/**
 * Based on the fields set, fill a Date object. For those fields that not
 * set, use the passed in date object's value.
 *
 * @param {?goog.date.DateLike} date Date object to be filled.
 * @param {boolean} validation If true, input date will be checked to make
 *     sure it is valid.
 *
 * @return {boolean} false if fields specify a invalid date.
 * @private
 * @suppress {strictMissingProperties} Added to tighten compiler checks
 */
goog.i18n.DateTimeParse.MyDate_.prototype.calcDate_ = function(
    date, validation) {
  'use strict';
  // Throw exception if date is null or undefined.
  if (date == null) {
    throw new Error('Parameter \'date\' should not be null.');
  }

  // year 0 is 1 BC, and so on.
  if (this.era != undefined && this.year != undefined && this.era == 0 &&
      this.year > 0) {
    this.year = -(this.year - 1);
  }

  if (this.year != undefined) {
    date.setFullYear(this.year);
  }

  // The setMonth and setDate logic is a little tricky. We need to make sure
  // day of month is smaller enough so that it won't cause a month switch when
  // setting month. For example, if data in date is Nov 30, when month is set
  // to Feb, because there is no Feb 30, JS adjust it to Mar 2. So Feb 12 will
  // become Mar 12.
  const orgDate = date.getDate();

  // Every month has a 1st day, this can actually be anything less than 29.
  date.setDate(1);

  if (this.month != undefined) {
    date.setMonth(this.month);
  }

  if (this.day != undefined) {
    date.setDate(this.day);
  } else {
    const maxDate =
        goog.date.getNumberOfDaysInMonth(date.getFullYear(), date.getMonth());
    date.setDate(orgDate > maxDate ? maxDate : orgDate);
  }

  if (typeof date.setHours === 'function') {
    if (this.hours == undefined) {
      /**
       * @suppress {strictMissingProperties} Added to tighten compiler checks
       */
      this.hours = date.getHours();
    }
    // adjust with am/pm to 24-hour time.
    if (this.hours < 12) {
      if (this.ampm != undefined && this.ampm > 0) {
        // AM or PM explictly set.
        this.hours += 12;
      } else {
        // Handle flexible time of day for PM times from parsed day period.
        // Noon, afternoons, evenings, are always PM.
        // Night could be AM or PM, depending on the hour value.
        //    6 in the evening --> 18:00
        //   10 at night --> 22:00
        //    3 at night --> 3:00
        if (this.dayPeriodName !== undefined) {
          const pmPeriods = [
            'isPm', 'noon', 'afternoon1', 'afternoon2', 'evening1', 'evening2'
          ];
          if (pmPeriods.includes(this.dayPeriodName) ||
              (['night1', 'night2'].includes(this.dayPeriodName) &&
               this.hours >= 6)) {
            // Adjust to be PM for this day period.
            this.hours += 12;
          }
        }
      }
    }
    date.setHours(this.hours);
  }

  if (typeof date.setMinutes === 'function' && this.minutes != undefined) {
    date.setMinutes(this.minutes);
  }

  if (typeof date.setSeconds === 'function' && this.seconds != undefined) {
    date.setSeconds(this.seconds);
  }

  if (typeof date.setMilliseconds === 'function' &&
      this.milliseconds != undefined) {
    date.setMilliseconds(this.milliseconds);
  }

  // If validation is needed, verify that the uncalculated date fields
  // match the calculated date fields.  We do this before we set the
  // timezone offset, which will skew all of the dates.
  //
  // Don't need to check the day of week as it is guaranteed to be
  // correct or return false below.
  if (validation &&
      (this.year && this.year != date.getFullYear() ||
       this.month && this.month != date.getMonth() ||
       this.day && this.day != date.getDate() ||
       this.hours && this.hours >= 24 || this.minutes && this.minutes >= 60 ||
       this.seconds && this.seconds >= 60 ||
       this.milliseconds && this.milliseconds >= 1000)) {
    return false;
  }

  // adjust time zone
  if (this.tzOffset != undefined) {
    const offset = date.getTimezoneOffset();
    date.setTime(date.getTime() + (this.tzOffset - offset) * 60 * 1000);
  }

  // resolve ambiguous year if needed
  if (this.ambiguousYear) {  // the two-digit year == the default start year
    const defaultCenturyStart = new Date();
    defaultCenturyStart.setFullYear(
        defaultCenturyStart.getFullYear() -
        goog.i18n.DateTimeParse.ambiguousYearCenturyStart);
    if (date.getTime() < defaultCenturyStart.getTime()) {
      date.setFullYear(defaultCenturyStart.getFullYear() + 100);
    }
  }

  // dayOfWeek, validation only
  if (this.dayOfWeek != undefined) {
    if (this.day == undefined) {
      // adjust to the nearest day of the week
      let adjustment = (7 + this.dayOfWeek - date.getDay()) % 7;
      if (adjustment > 3) {
        adjustment -= 7;
      }
      const orgMonth = date.getMonth();
      date.setDate(date.getDate() + adjustment);

      // don't let it switch month
      if (date.getMonth() != orgMonth) {
        date.setDate(date.getDate() + (adjustment > 0 ? -7 : 7));
      }
    } else if (this.dayOfWeek != date.getDay()) {
      return false;
    }
  }
  return true;
};
});  // End of scope for module data
