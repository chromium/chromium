/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Provides main functionality of assertThat. assertThat calls the
 * matcher's matches method to test if a matcher matches assertThat's arguments.
 */

goog.module('goog.labs.testing.assertThat');
goog.module.declareLegacyNamespace();

const Matcher = goog.requireType('goog.labs.testing.Matcher');
const MatcherError = goog.require('goog.labs.testing.MatcherError');

/**
 * Asserts that the actual value evaluated by the matcher is true.
 *
 * @param {*} actual The object to assert by the matcher.
 * @param {!Matcher} matcher A matcher to verify values.
 * @param {string=} opt_reason Description of what is asserted.
 */
function assertThat(actual, matcher, opt_reason) {
  if (!matcher.matches(actual)) {
    // Prefix the error description with a reason from the assert
    const prefix = opt_reason ? opt_reason + ': ' : '';
    const desc = prefix + matcher.describe(actual);

    // some sort of failure here
    throw new MatcherError(desc);
  }
}

exports = assertThat;
