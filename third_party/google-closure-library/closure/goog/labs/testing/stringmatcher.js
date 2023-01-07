/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Provides the built-in string matchers like containsString,
 *     startsWith, endsWith, etc.
 */

goog.provide('goog.labs.testing.stringmatcher');

goog.require('goog.asserts');
goog.require('goog.labs.testing.Matcher');
goog.require('goog.string');



/**
 * Matches any string value.
 *
 * @constructor @struct @implements {goog.labs.testing.Matcher} @final
 */
goog.labs.testing.stringmatcher.AnyStringMatcher = function() {};


/** @override */
goog.labs.testing.stringmatcher.AnyStringMatcher.prototype.matches = function(
    actualValue) {
  'use strict';
  return typeof actualValue === 'string';
};


/** @override */
goog.labs.testing.stringmatcher.AnyStringMatcher.prototype.describe = function(
    actualValue) {
  'use strict';
  return '<' + actualValue + '> is not a string';
};



/**
 * The ContainsString matcher.
 *
 * @param {string} value The expected string.
 *
 * @constructor
 * @struct
 * @implements {goog.labs.testing.Matcher}
 * @final
 */
goog.labs.testing.stringmatcher.ContainsStringMatcher = function(value) {
  'use strict';
  /**
   * @type {string}
   * @private
   */
  this.value_ = value;
};


/**
 * Determines if input string contains the expected string.
 *
 * @override
 */
goog.labs.testing.stringmatcher.ContainsStringMatcher.prototype.matches =
    function(actualValue) {
  'use strict';
  goog.asserts.assertString(actualValue);
  return goog.string.contains(actualValue, this.value_);
};


/**
 * @override
 */
goog.labs.testing.stringmatcher.ContainsStringMatcher.prototype.describe =
    function(actualValue) {
  'use strict';
  return actualValue + ' does not contain ' + this.value_;
};



/**
 * The EndsWith matcher.
 *
 * @param {string} value The expected string.
 *
 * @constructor
 * @struct
 * @implements {goog.labs.testing.Matcher}
 * @final
 */
goog.labs.testing.stringmatcher.EndsWithMatcher = function(value) {
  'use strict';
  /**
   * @type {string}
   * @private
   */
  this.value_ = value;
};


/**
 * Determines if input string ends with the expected string.
 *
 * @override
 */
goog.labs.testing.stringmatcher.EndsWithMatcher.prototype.matches = function(
    actualValue) {
  'use strict';
  goog.asserts.assertString(actualValue);
  return goog.string.endsWith(actualValue, this.value_);
};


/**
 * @override
 */
goog.labs.testing.stringmatcher.EndsWithMatcher.prototype.describe = function(
    actualValue) {
  'use strict';
  return actualValue + ' does not end with ' + this.value_;
};



/**
 * The EqualToIgnoringWhitespace matcher.
 *
 * @param {string} value The expected string.
 *
 * @constructor
 * @struct
 * @implements {goog.labs.testing.Matcher}
 * @final
 */
goog.labs.testing.stringmatcher.EqualToIgnoringWhitespaceMatcher = function(
    value) {
  'use strict';
  /**
   * @type {string}
   * @private
   */
  this.value_ = value;
};


/**
 * Determines if input string contains the expected string.
 *
 * @override
 */
goog.labs.testing.stringmatcher.EqualToIgnoringWhitespaceMatcher.prototype
    .matches = function(actualValue) {
  'use strict';
  goog.asserts.assertString(actualValue);
  var string1 = goog.string.collapseWhitespace(actualValue);

  return goog.string.caseInsensitiveCompare(this.value_, string1) === 0;
};


/**
 * @override
 */
goog.labs.testing.stringmatcher.EqualToIgnoringWhitespaceMatcher.prototype
    .describe = function(actualValue) {
  'use strict';
  return actualValue + ' is not equal(ignoring whitespace) to ' + this.value_;
};



/**
 * The Equals matcher.
 *
 * @param {string} value The expected string.
 *
 * @constructor
 * @struct
 * @implements {goog.labs.testing.Matcher}
 * @final
 */
goog.labs.testing.stringmatcher.EqualsMatcher = function(value) {
  'use strict';
  /**
   * @type {string}
   * @private
   */
  this.value_ = value;
};


/**
 * Determines if input string is equal to the expected string.
 *
 * @override
 */
goog.labs.testing.stringmatcher.EqualsMatcher.prototype.matches = function(
    actualValue) {
  'use strict';
  goog.asserts.assertString(actualValue);
  return this.value_ === actualValue;
};


/**
 * @override
 */
goog.labs.testing.stringmatcher.EqualsMatcher.prototype.describe = function(
    actualValue) {
  'use strict';
  return actualValue + ' is not equal to ' + this.value_;
};



/**
 * The MatchesRegex matcher.
 *
 * @param {!RegExp} regex The expected regex.
 *
 * @constructor
 * @struct
 * @implements {goog.labs.testing.Matcher}
 * @final
 */
goog.labs.testing.stringmatcher.RegexMatcher = function(regex) {
  'use strict';
  /**
   * @type {!RegExp}
   * @private
   */
  this.regex_ = regex;
};


/**
 * Determines if input string is equal to the expected string.
 *
 * @override
 */
goog.labs.testing.stringmatcher.RegexMatcher.prototype.matches = function(
    actualValue) {
  'use strict';
  goog.asserts.assertString(actualValue);
  return this.regex_.test(actualValue);
};


/**
 * @override
 */
goog.labs.testing.stringmatcher.RegexMatcher.prototype.describe = function(
    actualValue) {
  'use strict';
  return actualValue + ' does not match ' + this.regex_;
};



/**
 * The StartsWith matcher.
 *
 * @param {string} value The expected string.
 *
 * @constructor
 * @struct
 * @implements {goog.labs.testing.Matcher}
 * @final
 */
goog.labs.testing.stringmatcher.StartsWithMatcher = function(value) {
  'use strict';
  /**
   * @type {string}
   * @private
   */
  this.value_ = value;
};


/**
 * Determines if input string starts with the expected string.
 *
 * @override
 */
goog.labs.testing.stringmatcher.StartsWithMatcher.prototype.matches = function(
    actualValue) {
  'use strict';
  goog.asserts.assertString(actualValue);
  return goog.string.startsWith(actualValue, this.value_);
};


/**
 * @override
 */
goog.labs.testing.stringmatcher.StartsWithMatcher.prototype.describe = function(
    actualValue) {
  'use strict';
  return actualValue + ' does not start with ' + this.value_;
};



/**
 * The StringContainsInOrdermatcher.
 *
 * @param {Array<string>} values The expected string values.
 *
 * @constructor
 * @struct
 * @implements {goog.labs.testing.Matcher}
 * @final
 */
goog.labs.testing.stringmatcher.StringContainsInOrderMatcher = function(
    values) {
  'use strict';
  /**
   * @type {Array<string>}
   * @private
   */
  this.values_ = values;
};


/**
 * Determines if input string contains, in order, the expected array of strings.
 * @override
 * @suppress {strictPrimitiveOperators} Part of the go/strict_warnings_migration
 */
goog.labs.testing.stringmatcher.StringContainsInOrderMatcher.prototype.matches =
    function(actualValue) {
  'use strict';
  goog.asserts.assertString(actualValue);
  var currentIndex, previousIndex = 0;
  for (var i = 0; i < this.values_.length; i++) {
    currentIndex = goog.string.contains(actualValue, this.values_[i]);
    if (currentIndex < 0 || currentIndex < previousIndex) {
      return false;
    }
    previousIndex = currentIndex;
  }
  return true;
};


/**
 * @override
 */
goog.labs.testing.stringmatcher.StringContainsInOrderMatcher.prototype
    .describe = function(actualValue) {
  'use strict';
  return actualValue + ' does not contain the expected values in order.';
};


/** @return {!goog.labs.testing.stringmatcher.AnyStringMatcher} */
goog.labs.testing.stringmatcher.AnyStringMatcher.anyString = function() {
  'use strict';
  return new goog.labs.testing.stringmatcher.AnyStringMatcher();
};


/**
 * Matches a string containing the given string.
 *
 * @param {string} value The expected value.
 *
 * @return {!goog.labs.testing.stringmatcher.ContainsStringMatcher} A
 *     ContainsStringMatcher.
 */
goog.labs.testing.stringmatcher.ContainsStringMatcher.containsString = function(
    value) {
  'use strict';
  return new goog.labs.testing.stringmatcher.ContainsStringMatcher(value);
};


/**
 * Matches a string that ends with the given string.
 *
 * @param {string} value The expected value.
 *
 * @return {!goog.labs.testing.stringmatcher.EndsWithMatcher} A
 *     EndsWithMatcher.
 */
goog.labs.testing.stringmatcher.EndsWithMatcher.endsWith = function(value) {
  'use strict';
  return new goog.labs.testing.stringmatcher.EndsWithMatcher(value);
};


/**
 * Matches a string that equals (ignoring whitespace) the given string.
 *
 * @param {string} value The expected value.
 *
 * @return {!goog.labs.testing.stringmatcher.EqualToIgnoringWhitespaceMatcher} A
 *     EqualToIgnoringWhitespaceMatcher.
 */
goog.labs.testing.stringmatcher.EqualToIgnoringWhitespaceMatcher
    .equalToIgnoringWhitespace = function(value) {
  'use strict';
  return new goog.labs.testing.stringmatcher.EqualToIgnoringWhitespaceMatcher(
      value);
};


/**
 * Matches a string that equals the given string.
 *
 * @param {string} value The expected value.
 *
 * @return {!goog.labs.testing.stringmatcher.EqualsMatcher} A EqualsMatcher.
 */
goog.labs.testing.stringmatcher.EqualsMatcher.equals = function(value) {
  'use strict';
  return new goog.labs.testing.stringmatcher.EqualsMatcher(value);
};


/**
 * Matches a string against a regular expression.
 *
 * @param {!RegExp} regex The expected regex.
 *
 * @return {!goog.labs.testing.stringmatcher.RegexMatcher} A RegexMatcher.
 */
goog.labs.testing.stringmatcher.RegexMatcher.matchesRegex = function(regex) {
  'use strict';
  return new goog.labs.testing.stringmatcher.RegexMatcher(regex);
};


/**
 * Matches a string that starts with the given string.
 *
 * @param {string} value The expected value.
 *
 * @return {!goog.labs.testing.stringmatcher.StartsWithMatcher} A
 *     StartsWithMatcher.
 */
goog.labs.testing.stringmatcher.StartsWithMatcher.startsWith = function(value) {
  'use strict';
  return new goog.labs.testing.stringmatcher.StartsWithMatcher(value);
};


/**
 * Matches a string that contains the given strings in order.
 *
 * @param {Array<string>} values The expected value.
 *
 * @return {!goog.labs.testing.stringmatcher.StringContainsInOrderMatcher} A
 *     StringContainsInOrderMatcher.
 */
goog.labs.testing.stringmatcher.StringContainsInOrderMatcher
    .stringContainsInOrder = function(values) {
  'use strict';
  return new goog.labs.testing.stringmatcher.StringContainsInOrderMatcher(
      values);
};
