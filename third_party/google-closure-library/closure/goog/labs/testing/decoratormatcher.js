/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Provides the built-in decorators: is, describedAs, anything.
 */

goog.provide('goog.labs.testing.decoratormatcher');

goog.require('goog.labs.testing.Matcher');


/**
 * The Anything matcher. Matches all possible inputs.
 *
 * @constructor
 * @implements {goog.labs.testing.Matcher}
 * @final
 */
goog.labs.testing.decoratormatcher.AnythingMatcher = function() {};


/**
 * Matches anything. Useful if one doesn't care what the object under test is.
 *
 * @override
 */
goog.labs.testing.decoratormatcher.AnythingMatcher.prototype.matches = function(
    actualObject) {
  'use strict';
  return true;
};


/**
 * This method is never called but is needed so AnythingMatcher implements the
 * Matcher interface.
 *
 * @override
 */
goog.labs.testing.decoratormatcher.AnythingMatcher.prototype.describe =
    function(actualObject) {
  'use strict';
  throw new Error('AnythingMatcher should never fail!');
};


/**
 * Returns a matcher that matches anything.
 *
 * @return {!goog.labs.testing.decoratormatcher.AnythingMatcher} A
 *     AnythingMatcher.
 */
goog.labs.testing.decoratormatcher.AnythingMatcher.anything = function() {
  'use strict';
  return new goog.labs.testing.decoratormatcher.AnythingMatcher();
};


/**
 * Returns any matcher that is passed to it (aids readability).
 *
 * @param {!goog.labs.testing.Matcher} matcher A matcher.
 * @return {!goog.labs.testing.Matcher} The wrapped matcher.
 */
goog.labs.testing.decoratormatcher.AnythingMatcher.is = function(matcher) {
  'use strict';
  return matcher;
};


/**
 * Returns a matcher with a customized description for the given matcher.
 *
 * @param {string} description The custom description for the matcher.
 * @param {!goog.labs.testing.Matcher} matcher The matcher.
 * @return {!goog.labs.testing.Matcher} The matcher with custom description.
 */
goog.labs.testing.decoratormatcher.AnythingMatcher.describedAs = function(
    description, matcher) {
  'use strict';
  return /** @type {!goog.labs.testing.Matcher} */ ({
    matches: function(value) {
      'use strict';
      return matcher.matches(value);
    },
    describe: function() {
      'use strict';
      return description;
    }
  });
};
