/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Provides the base Matcher interface. User code should use the
 * matchers through assertThat statements and not directly.
 */


goog.provide('goog.labs.testing.Matcher');



/**
 * A matcher object to be used in assertThat statements.
 * @interface
 */
goog.labs.testing.Matcher = function() {};


/**
 * Determines whether a value matches the constraints of the match.
 *
 * @param {*} value The object to match.
 * @return {boolean} Whether the input value matches this matcher.
 */
goog.labs.testing.Matcher.prototype.matches = function(value) {};


/**
 * Describes why the matcher failed.
 *
 * @param {*} value The value that didn't match.
 * @param {string=} opt_description A partial description to which the reason
 *     will be appended.
 *
 * @return {string} Description of why the matcher failed.
 */
goog.labs.testing.Matcher.prototype.describe = function(
    value, opt_description) {};


/**
 * Generates a Matcher from the ‘matches’ and ‘describe’ functions passed in.
 *
 * @param {!Function} matchesFunction The ‘matches’ function.
 * @param {Function=} opt_describeFunction The ‘describe’ function.
 * @return {!Function} The custom matcher.
 */
goog.labs.testing.Matcher.makeMatcher = function(
    matchesFunction, opt_describeFunction) {
  'use strict';
  /**
   * @constructor
   * @implements {goog.labs.testing.Matcher}
   * @final
   */
  const matcherConstructor = function() {};

  /** @override */
  matcherConstructor.prototype.matches = matchesFunction;

  if (opt_describeFunction) {
    /** @override */
    matcherConstructor.prototype.describe = opt_describeFunction;
  }

  return matcherConstructor;
};
