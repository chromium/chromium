/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Provides a parser that turns a string of well-formed CSV data
 * into an array of objects or an array of arrays. All values are returned as
 * strings; the user has to convert data into numbers or Dates as required.
 * Empty fields (adjacent commas) are returned as empty strings.
 *
 * This parser uses http://tools.ietf.org/html/rfc4180 as the definition of CSV.
 */

// TODO(user): We're trying to migrate all ES5 subclasses of Closure
// Library to ES6. In ES6 this cannot be referenced before super is called. This
// file has at least one this before a super call (in ES5) and cannot be
// automatically upgraded to ES6 as a result. Please fix this if you have a
// chance. Note: This can sometimes be caused by not calling the super
// constructor at all. You can run the conversion tool yourself to see what it
// does on this file: blaze run //javascript/refactoring/es6_classes:convert.

goog.provide('goog.labs.format.csv');
goog.provide('goog.labs.format.csv.ParseError');
goog.provide('goog.labs.format.csv.Token');

goog.require('goog.asserts');
goog.require('goog.debug.Error');
goog.require('goog.object');
goog.require('goog.string');
goog.require('goog.string.newlines');


/**
 * @define {boolean} Enable verbose debugging. This is a flag so it can be
 * enabled in production if necessary post-compilation.  Otherwise, debug
 * information will be stripped to minimize final code size.
 */
goog.labs.format.csv.ENABLE_VERBOSE_DEBUGGING = goog.DEBUG;



/**
 * Error thrown when parsing fails.
 *
 * @param {string} text The CSV source text being parsed.
 * @param {number} index The index, in the string, of the position of the
 *      error.
 * @param {string=} opt_message A description of the violated parse expectation.
 * @constructor
 * @extends {goog.debug.Error}
 * @final
 */
goog.labs.format.csv.ParseError = function(text, index, opt_message) {
  'use strict';
  let message;

  /**
   * @type {?{line: number, column: number}} The line and column of the parse
   *     error.
   */
  this.position = null;

  if (goog.labs.format.csv.ENABLE_VERBOSE_DEBUGGING) {
    message = opt_message || '';

    const info = goog.labs.format.csv.ParseError.findLineInfo_(text, index);
    if (info) {
      const lineNumber = info.lineIndex + 1;
      const columnNumber = index - info.line.startLineIndex + 1;

      this.position = {line: lineNumber, column: columnNumber};

      message +=
          goog.string.subs(' at line %s column %s', lineNumber, columnNumber);
      message += '\n' +
          goog.labs.format.csv.ParseError.getLineDebugString_(
              info.line.getContent(), columnNumber);
    }
  }

  goog.labs.format.csv.ParseError.base(this, 'constructor', message);
};
goog.inherits(goog.labs.format.csv.ParseError, goog.debug.Error);


/** @inheritDoc */
goog.labs.format.csv.ParseError.prototype.name = 'ParseError';


/**
 * Calculate the line and column for an index in a string.
 * TODO(nnaze): Consider moving to goog.string.newlines.
 * @param {string} str A string.
 * @param {number} index An index into the string.
 * @return {?{line: !goog.string.newlines.Line, lineIndex: number}} The line
 *     and index of the line.
 * @private
 */
goog.labs.format.csv.ParseError.findLineInfo_ = function(str, index) {
  'use strict';
  const lines = goog.string.newlines.getLines(str);
  const lineIndex = lines.findIndex(function(line) {
    'use strict';
    return line.startLineIndex <= index && line.endLineIndex > index;
  });

  if (typeof (lineIndex) === 'number') {
    const line = lines[lineIndex];
    return {line: line, lineIndex: lineIndex};
  }

  return null;
};


/**
 * Get a debug string of a line and a pointing caret beneath it.
 * @param {string} str The string.
 * @param {number} column The column to point at (1-indexed).
 * @return {string} The debug line.
 * @private
 */
goog.labs.format.csv.ParseError.getLineDebugString_ = function(str, column) {
  'use strict';
  let returnString = str + '\n';
  returnString += goog.string.repeat(' ', column - 1) + '^';
  return returnString;
};


/**
 * A token -- a single-character string or a sentinel.
 * @typedef {string|!goog.labs.format.csv.Sentinels_}
 */
goog.labs.format.csv.Token;


/**
 * Parses a CSV string to create a two-dimensional array.
 *
 * This function does not process header lines, etc -- such transformations can
 * be made on the resulting array.
 *
 * @param {string} text The entire CSV text to be parsed.
 * @param {boolean=} opt_ignoreErrors Whether to ignore parsing errors and
 *      instead try to recover and keep going.
 * @param {string=} opt_delimiter The delimiter to use. Defaults to ','
 * @return {!Array<!Array<string>>} The parsed CSV.
 */
goog.labs.format.csv.parse = function(text, opt_ignoreErrors, opt_delimiter) {
  'use strict';
  let index = 0;  // current char offset being considered

  const delimiter = opt_delimiter || ',';
  goog.asserts.assert(
      delimiter.length == 1, 'Delimiter must be a single character.');
  goog.asserts.assert(
      delimiter != '\r' && opt_delimiter != '\n',
      'Cannot use newline or carriage return as delimiter.');

  const EOF = goog.labs.format.csv.Sentinels_.EOF;
  const EOR = goog.labs.format.csv.Sentinels_.EOR;
  const NEWLINE = goog.labs.format.csv.Sentinels_.NEWLINE;  // \r?\n
  const EMPTY = goog.labs.format.csv.Sentinels_.EMPTY;

  let pushBackToken = null;  // A single-token pushback.
  let sawComma = false;      // Special case for terminal comma.

  /**
   * Push a single token into the push-back variable.
   * @param {goog.labs.format.csv.Token} t Single token.
   */
  function pushBack(t) {
    goog.labs.format.csv.assertToken_(t);
    goog.asserts.assert(pushBackToken === null);
    pushBackToken = t;
  }

  /**
   * @return {goog.labs.format.csv.Token} The next token in the stream.
   */
  function nextToken() {
    // Give the push back token if present.
    if (pushBackToken != null) {
      const c = pushBackToken;
      pushBackToken = null;
      return c;
    }

    // We're done. EOF.
    if (index >= text.length) {
      return EOF;
    }

    // Give the next charater.
    const chr = text.charAt(index++);
    goog.labs.format.csv.assertToken_(chr);

    // Check if this is a newline.  If so, give the new line sentinel.
    let isNewline = false;
    if (chr == '\n') {
      isNewline = true;
    } else if (chr == '\r') {
      // This is a '\r\n' newline. Treat as single token, go
      // forward two indicies.
      if (index < text.length && text.charAt(index) == '\n') {
        index++;
      }

      isNewline = true;
    }

    if (isNewline) {
      return NEWLINE;
    }

    return chr;
  }

  /**
   * Read a quoted field from input.
   * @return {string} The field, as a string.
   */
  function readQuotedField() {
    // We've already consumed the first quote by the time we get here.
    const start = index;
    let end = null;

    for (let token = nextToken(); token != EOF; token = nextToken()) {
      if (token == '"') {
        end = index - 1;
        token = nextToken();

        // Two double quotes in a row.  Keep scanning.
        if (token == '"') {
          end = null;
          continue;
        }

        // End of field.  Break out.
        if (token == delimiter || token == EOF || token == NEWLINE) {
          if (token == NEWLINE) {
            pushBack(token);
          }
          if (token == delimiter) {
            sawComma = true;
          }
          break;
        }

        if (!opt_ignoreErrors) {
          // Ignoring errors here means keep going in current field after
          // closing quote. E.g. "ab"c,d splits into abc,d
          throw new goog.labs.format.csv.ParseError(
              text, index - 1,
              'Unexpected character "' + token + '" after quote mark');
        } else {
          // Fall back to reading the rest of this field as unquoted.
          // Note: the rest is guaranteed not start with ", as that case is
          // eliminated above.
          const prefix = '"' + text.substring(start, index);
          const suffix = readField();
          if (suffix == EOR) {
            pushBack(NEWLINE);
            return prefix;
          } else {
            return prefix + suffix;
          }
        }
      }
    }

    if (end === null) {
      if (!opt_ignoreErrors) {
        throw new goog.labs.format.csv.ParseError(
            text, text.length - 1, 'Unexpected end of text after open quote');
      } else {
        end = text.length;
      }
    }

    // Take substring, combine double quotes.
    return text.substring(start, end).replace(/""/g, '"');
  }

  /**
   * Read a field from input.
   * @return {string|!goog.labs.format.csv.Sentinels_} The field, as a string,
   *     or a sentinel (if applicable).
   */
  function readField() {
    const start = index;
    const didSeeComma = sawComma;
    sawComma = false;
    let token = nextToken();
    if (token == EMPTY) {
      return EOR;
    }
    if (token == EOF || token == NEWLINE) {
      if (didSeeComma) {
        pushBack(EMPTY);
        return '';
      }
      return EOR;
    }

    // This is the beginning of a quoted field.
    if (token == '"') {
      return readQuotedField();
    }

    while (true) {
      // This is the end of line or file.
      if (token == EOF || token == NEWLINE) {
        pushBack(token);
        break;
      }

      // This is the end of record.
      if (token == delimiter) {
        sawComma = true;
        break;
      }

      if (token == '"' && !opt_ignoreErrors) {
        throw new goog.labs.format.csv.ParseError(
            text, index - 1, 'Unexpected quote mark');
      }

      token = nextToken();
    }


    const returnString = (token == EOF) ?
        text.substring(start) :  // Return to end of file.
        text.substring(start, index - 1);

    return returnString.replace(/[\r\n]+/g, '');  // Squash any CRLFs.
  }

  /**
   * Read the next record.
   * @return {!Array<string>|!goog.labs.format.csv.Sentinels_} A single record
   *     with multiple fields.
   */
  function readRecord() {
    if (index >= text.length) {
      return EOF;
    }
    const record = [];
    for (let field = readField(); field != EOR; field = readField()) {
      record.push(field);
    }
    return record;
  }

  // Read all records and return.
  const records = [];
  for (let record = readRecord(); record != EOF; record = readRecord()) {
    records.push(record);
  }
  return records;
};


/**
 * Sentinel tracking objects.
 * @enum {!Object}
 * @private
 */
goog.labs.format.csv.Sentinels_ = {
  /** Empty field */
  EMPTY: {},

  /** End of file */
  EOF: {},

  /** End of record */
  EOR: {},

  /** Newline. \r?\n */
  NEWLINE: {}
};


/**
 * @param {string} str A string.
 * @return {boolean} Whether the string is a single character.
 * @private
 */
goog.labs.format.csv.isCharacterString_ = function(str) {
  'use strict';
  return typeof str === 'string' && str.length == 1;
};


/**
 * Assert the parameter is a token.
 * @param {*} o What should be a token.
 * @throws {goog.asserts.AssertionError} If {@ code} is not a token.
 * @private
 */
goog.labs.format.csv.assertToken_ = function(o) {
  'use strict';
  if (typeof o === 'string') {
    goog.asserts.assertString(o);
    goog.asserts.assert(
        goog.labs.format.csv.isCharacterString_(o),
        'Should be a string of length 1 or a sentinel.');
  } else {
    goog.asserts.assert(
        goog.object.containsValue(goog.labs.format.csv.Sentinels_, o),
        'Should be a string of length 1 or a sentinel.');
  }
};
