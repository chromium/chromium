/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Provides the built-in logic matchers: anyOf, allOf, and isNot.
 */

goog.provide('goog.labs.testing.logicmatcher');


goog.require('goog.array');
goog.require('goog.labs.testing.Matcher');



/**
 * The AllOf matcher.
 *
 * @param {!Array<!goog.labs.testing.Matcher>} matchers Input matchers.
 *
 * @constructor
 * @struct
 * @implements {goog.labs.testing.Matcher}
 * @final
 */
goog.labs.testing.logicmatcher.AllOfMatcher = function(matchers) {
  'use strict';
  /**
   * @type {!Array<!goog.labs.testing.Matcher>}
   * @private
   */
  this.matchers_ = matchers;
};


/**
 * Determines if all of the matchers match the input value.
 *
 * @override
 */
goog.labs.testing.logicmatcher.AllOfMatcher.prototype.matches = function(
    actualValue) {
  'use strict';
  return this.matchers_.every(function(matcher) {
    'use strict';
    return matcher.matches(actualValue);
  });
};


/**
 * Describes why the matcher failed. The returned string is a concatenation of
 * all the failed matchers' error strings.
 *
 * @override
 */
goog.labs.testing.logicmatcher.AllOfMatcher.prototype.describe = function(
    actualValue) {
  'use strict';
  // TODO(vbhasin) : Optimize this to remove duplication with matches ?
  var errorString = '';
  this.matchers_.forEach(function(matcher) {
    'use strict';
    if (!matcher.matches(actualValue)) {
      errorString += matcher.describe(actualValue) + '\n';
    }
  });
  return errorString;
};



/**
 * The AnyOf matcher.
 *
 * @param {!Array<!goog.labs.testing.Matcher>} matchers Input matchers.
 *
 * @constructor
 * @struct
 * @implements {goog.labs.testing.Matcher}
 * @final
 */
goog.labs.testing.logicmatcher.AnyOfMatcher = function(matchers) {
  'use strict';
  /**
   * @type {!Array<!goog.labs.testing.Matcher>}
   * @private
   */
  this.matchers_ = matchers;
};


/**
 * Determines if any of the matchers matches the input value.
 *
 * @override
 */
goog.labs.testing.logicmatcher.AnyOfMatcher.prototype.matches = function(
    actualValue) {
  'use strict';
  return goog.array.some(this.matchers_, function(matcher) {
    'use strict';
    return matcher.matches(actualValue);
  });
};


/**
 * Describes why the matcher failed.
 *
 * @override
 */
goog.labs.testing.logicmatcher.AnyOfMatcher.prototype.describe = function(
    actualValue) {
  'use strict';
  // TODO(vbhasin) : Optimize this to remove duplication with matches ?
  var errorString = '';
  this.matchers_.forEach(function(matcher) {
    'use strict';
    if (!matcher.matches(actualValue)) {
      errorString += matcher.describe(actualValue) + '\n';
    }
  });
  return errorString;
};



/**
 * The IsNot matcher.
 *
 * @param {!goog.labs.testing.Matcher} matcher The matcher to negate.
 *
 * @constructor
 * @struct
 * @implements {goog.labs.testing.Matcher}
 * @final
 */
goog.labs.testing.logicmatcher.IsNotMatcher = function(matcher) {
  'use strict';
  /**
   * @type {!goog.labs.testing.Matcher}
   * @private
   */
  this.matcher_ = matcher;
};


/**
 * Determines if the input value doesn't satisfy a matcher.
 *
 * @override
 */
goog.labs.testing.logicmatcher.IsNotMatcher.prototype.matches = function(
    actualValue) {
  'use strict';
  return !this.matcher_.matches(actualValue);
};


/**
 * Describes why the matcher failed.
 *
 * @override
 */
goog.labs.testing.logicmatcher.IsNotMatcher.prototype.describe = function(
    actualValue) {
  'use strict';
  return 'The following is false: ' + this.matcher_.describe(actualValue);
};


/**
 * Creates a matcher that will succeed only if all of the given matchers
 * succeed.
 *
 * @param {...goog.labs.testing.Matcher} var_args The matchers to test
 *     against.
 *
 * @return {!goog.labs.testing.logicmatcher.AllOfMatcher} The AllOf matcher.
 */
goog.labs.testing.logicmatcher.AllOfMatcher.allOf = function(var_args) {
  'use strict';
  var matchers = Array.prototype.slice.call(arguments);
  return new goog.labs.testing.logicmatcher.AllOfMatcher(matchers);
};


/**
 * Accepts a set of matchers and returns a matcher which matches
 * values which satisfy the constraints of any of the given matchers.
 *
 * @param {...goog.labs.testing.Matcher} var_args The matchers to test
 *     against.
 *
 * @return {!goog.labs.testing.logicmatcher.AnyOfMatcher} The AnyOf matcher.
 */
goog.labs.testing.logicmatcher.AnyOfMatcher.anyOf = function(var_args) {
  'use strict';
  var matchers = Array.prototype.slice.call(arguments);
  return new goog.labs.testing.logicmatcher.AnyOfMatcher(matchers);
};


/**
 * Returns a matcher that negates the input matcher. The returned
 * matcher matches the values not matched by the input matcher and vice-versa.
 *
 * @param {!goog.labs.testing.Matcher} matcher The matcher to test against.
 *
 * @return {!goog.labs.testing.logicmatcher.IsNotMatcher} The IsNot matcher.
 */
goog.labs.testing.logicmatcher.IsNotMatcher.isNot = function(matcher) {
  'use strict';
  return new goog.labs.testing.logicmatcher.IsNotMatcher(matcher);
};
