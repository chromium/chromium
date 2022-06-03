/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Provides main functionality of assertThat. assertThat calls the
 * matcher's matches method to test if a matcher matches assertThat's arguments.
 */

goog.module('goog.labs.testing.MatcherError');
goog.module.declareLegacyNamespace();

const DebugError = goog.require('goog.debug.Error');

/**
 * Error thrown when a Matcher fails to match the input value.
 * @param {string=} message The error message.
 * @constructor
 * @extends {DebugError}
 * @final
 */
function MatcherError(message) {
  MatcherError.base(this, 'constructor', message);
}
goog.inherits(MatcherError, DebugError);

exports = MatcherError;
