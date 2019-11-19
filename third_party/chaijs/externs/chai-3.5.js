/*
 * Copyright 2016 The Closure Compiler Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @fileoverview Externs definitions for Chai, 3.5 branch.
 *
 * This file defines both the BDD and TDD APIs. The BDD API should be complete.
 *
 * This file defines some virtual types for the chained methods, please don't
 * use these types directly, but follow the official API guidelines.
 *
 * @externs
 * @see http://chaijs.com/
 */

/** @const */
var chai = {};

/**
 * @constructor
 * @param {*} obj
 * @param {string=} msg
 * @param {!Function=} ssfi
 * @param {boolean=} lockSsfi
 */
chai.Assertion = function(obj, msg, ssfi, lockSsfi) {};

// Below are the externs for the BDD expect API: http://chaijs.com/api/bdd/

/**
 * @param {*} subject
 * @param {string=} opt_description
 * @return {!chai.Assertion}
 */
var expect = function(subject, opt_description) {};


/** @type {!chai.Assertion} */ chai.Assertion.prototype.to;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.be;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.been;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.is;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.that;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.which;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.and;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.has;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.have;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.with;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.at;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.of;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.same;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.not;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.deep;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.any;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.all;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.length;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.itself;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.ok;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.true;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.false;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.null;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.undefined;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.NaN;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.exist;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.empty;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.arguments;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.extensible;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.sealed;
/** @type {!chai.Assertion} */ chai.Assertion.prototype.frozen;


/**
 * @param {string} type
 * @param {string=} opt_message
 * @return {!chai.Assertion}
 */
chai.Assertion.prototype.a = function(type, opt_message) {};

/**
 * @param {string} type
 * @param {string=} opt_message
 */
chai.Assertion.prototype.an = function(type, opt_message) {};

/**
 * @param {*} value
 * @param {string=} opt_message
 */
chai.Assertion.prototype.include = function(value, opt_message) {};

/** @type {!chai.Assertion} */ chai.Assertion.prototype.include.all;

/**
 * @param {*} value
 * @param {string=} opt_message
 */
chai.Assertion.prototype.includes = function(value, opt_message) {};

/**
 * @param {*} value
 * @param {string=} opt_message
 */
chai.Assertion.prototype.contain = function(value, opt_message) {};

/**
 * @param {*} value
 * @param {string=} opt_message
 */
chai.Assertion.prototype.contains = function(value, opt_message) {};

/**
 * @param {*} value
 * @param {string=} opt_message
 */
chai.Assertion.prototype.equal = function(value, opt_message) {};

/**
 * @param {*} value
 * @param {string=} opt_message
 */
chai.Assertion.prototype.eql = function(value, opt_message) {};

/**
 * @param {number} value
 * @param {string=} opt_message
 */
chai.Assertion.prototype.above = function(value, opt_message) {};

/**
 * @param {number} value
 * @param {string=} opt_message
 */
chai.Assertion.prototype.least = function(value, opt_message) {};

/**
 * @param {number} value
 * @param {string=} opt_message
 */
chai.Assertion.prototype.below = function(value, opt_message) {};

/**
 * @param {number} value
 * @param {string=} opt_message
 */
chai.Assertion.prototype.most = function(value, opt_message) {};

/**
 * @param {number} start
 * @param {number} finish
 * @param {string=} opt_message
 */
chai.Assertion.prototype.within = function(start, finish, opt_message) {};

/**
 * @param {function(new: Object)} constructor
 * @param {string=} opt_message
 */
chai.Assertion.prototype.instanceof = function(constructor, opt_message) {};

/**
 * @param {function(new: Object)} constructor
 * @param {string=} opt_message
 */
chai.Assertion.prototype.an.instanceof = function(constructor, opt_message) {};

/**
 * @param {string} name
 * @param {*=} opt_value
 * @param {string=} opt_message
 */
chai.Assertion.prototype.property = function(name, opt_value, opt_message) {};

/**
 * @param {string} name
 * @param {string=} opt_message
 */
chai.Assertion.prototype.ownProperty = function(name, opt_message) {};

/**
 * @param {string} name
 * @param {!Object=} opt_descriptor
 * @param {string=} opt_message
 */
chai.Assertion.prototype.ownPropertyDescriptor = function(
    name, opt_descriptor, opt_message) {};

/**
 * @param {number} value
 * @param {string=} opt_message
 */
chai.Assertion.prototype.lengthOf = function(value, opt_message) {};

/**
 * @param {!RegExp} re
 * @param {string=} opt_message
 */
chai.Assertion.prototype.match = function(re, opt_message) {};

/**
 * @param {string} value
 * @param {string=} opt_message
 */
chai.Assertion.prototype.string = function(value, opt_message) {};

/**
 * @param {*} value
 * @param {string=} opt_message
 */
chai.Assertion.prototype.keys = function(value, opt_message) {};

/**
 * Note: incomplete definition because it is tricky.
 * @param {...*} var_args
 */
chai.Assertion.prototype.throw = function(var_args) {};

/**
 * @param {string} method
 * @param {string=} opt_message
 */
chai.Assertion.prototype.respondTo = function(method, opt_message) {};

/**
 * @param {function(*): boolean} matcher
 * @param {string=} opt_message
 */
chai.Assertion.prototype.satisfy = function(matcher, opt_message) {};

/**
 * @param {!Array<*>} value
 * @param {string=} opt_message
 */
chai.Assertion.prototype.members = function(value, opt_message) {};

/**
 * @param {!Array<*>} value
 * @param {string=} opt_message
 */
chai.Assertion.prototype.oneOf = function(value, opt_message) {};

/**
 * @param {*} value
 * @param {string} name
 * @param {string=} opt_message
 */
chai.Assertion.prototype.change = function(value, name, opt_message) {};

/**
 * @param {*} value
 * @param {string} name
 * @param {string=} opt_message
 */
chai.Assertion.prototype.increase = function(value, name, opt_message) {};

/**
 * @param {*} value
 * @param {string} name
 * @param {string=} opt_message
 */
chai.Assertion.prototype.decrease = function(value, name, opt_message) {};

// Below are the externs for the TDD expect API: http://chaijs.com/api/assert/

/** @const */
var assert = {};

/**
 * @param {*} actual
 * @param {*} expected
 * @param {string=} opt_message
 * @param {string=} opt_operator
 */
assert.fail = function(actual, expected, opt_message, opt_operator) {};

/**
 * @param {*} object
 * @param {string=} opt_message
 */
assert.isOk = function(object, opt_message) {};

/**
 * @param {*} object
 * @param {string=} opt_message
 */
assert.isNotOk = function(object, opt_message) {};

/**
 * @param {*} actual
 * @param {*} expected
 * @param {string=} opt_message
 */
assert.equal = function(actual, expected, opt_message) {};

/**
 * @param {*} actual
 * @param {*} expected
 * @param {string=} opt_message
 */
assert.strictEqual = function(actual, expected, opt_message) {};

/**
 * @param {*} actual
 * @param {*} expected
 * @param {string=} opt_message
 */
assert.notStrictEqual = function(actual, expected, opt_message) {};

/**
 * @param {*} actual
 * @param {*} expected
 * @param {string=} opt_message
 */
assert.notEqual = function(actual, expected, opt_message) {};

/**
 * @param {*} actual
 * @param {*} expected
 * @param {string=} opt_message
 */
assert.deepEqual = function(actual, expected, opt_message) {};

/**
 * @param {*} actual
 * @param {*} expected
 * @param {string=} opt_message
 */
assert.notDeepEqual = function(actual, expected, opt_message) {};

/**
 * @param {*} valueToCheck
 * @param {*} valueToBeAbove
 * @param {string=} opt_message
 */
assert.isAbove = function(valueToCheck, valueToBeAbove, opt_message) {};

/**
 * @param {*} valueToCheck
 * @param {*} valueToBeBelow
 * @param {string=} opt_message
 */
assert.isBelow = function(valueToCheck, valueToBeBelow, opt_message) {};

/**
 * @param {*} value
 * @param {string=} opt_message
 */
assert.isTrue = function(value, opt_message) {};

/**
 * @param {*} value
 * @param {string=} opt_message
 */
assert.isNotTrue = function(value, opt_message) {};

/**
 * @param {*} value
 * @param {string=} opt_message
 */
assert.isFalse = function(value, opt_message) {};

/**
 * @param {*} value
 * @param {string=} opt_message
 */
assert.isNotFalse = function(value, opt_message) {};

/**
 * @param {*} value
 * @param {string=} opt_message
 */
assert.isNaN = function(value, opt_message) {};

/**
 * @param {*} value
 * @param {string=} opt_message
 */
assert.isUndefined = function(value, opt_message) {};

/**
 * @param {*} value
 * @param {string=} opt_message
 */
assert.isDefined = function(value, opt_message) {};

/**
 * @param {*} value
 * @param {string=} opt_message
 */
assert.isFunction = function(value, opt_message) {};

/**
 * @param {*} value
 * @param {string=} opt_message
 */
assert.isNotFunction = function(value, opt_message) {};

/**
 * @param {*} value
 * @param {string=} message
 */
assert.isNotNull = function(value, message) {}

/**
 * @param {*} value
 * @param {string=} opt_message
 */
assert.isNull = function(value, opt_message) {};

/**
 * @param {*} value
 * @param {string=} opt_message
 */
assert.exists = function(value, opt_message) {};

/**
 * @param {*} value
 * @param {string=} opt_message
 */
assert.notExists = function(value, opt_message) {};

/**
 * @param {*} object
 * @param {function(new: Object)} constructor
 * @param {string=} opt_message
 */
assert.instanceOf = function(object, constructor, opt_message) {};

/**
 * @param {!Array<*>|string} haystack
 * @param {*} needle
 * @param {string=} opt_message
 */
assert.include = function(haystack, needle, opt_message) {};

/**
 * @param {!Array<*>|string} haystack
 * @param {*} needle
 * @param {string=} opt_message
 */
assert.notInclude = function(haystack, needle, opt_message) {};

/**
 * @param {*} needle
 * @param {!Array<*>} haystack
 * @param {string=} opt_message
 */
assert.oneOf = function(needle, haystack, opt_message) {};

/**
 * @param {*} collection
 * @param {number} length
 * @param {string=} message
 */
assert.lengthOf = function(collection, length, message) {};

/**
 * @param {*} object
 * @param {!RegExp} re
 * @param {string=} opt_message
 */
assert.match = function(object, re, opt_message) {};

/**
 * @param {?Object|undefined} object
 * @param {string} property
 * @param {*} value
 * @param {string=} opt_message
 */
assert.propertyVal = function(object, property, value, opt_message) {};

/**
 * @param {function()} fn
 * @param {function(new: Object)|string|!RegExp} constructor
 * @param {string|!RegExp=} opt_regexp
 * @param {string=} opt_message
 */
assert.throws = function(fn, constructor, opt_regexp, opt_message) {};

/**
 * @param {function()} fn
 * @param {function(new: Object)|string|!RegExp} constructor
 * @param {string|!RegExp=} opt_regexp
 * @param {string=} opt_message
 */
assert.doesNotThrow = function(fn, constructor, opt_regexp, opt_message) {};

/**
 * @param {!Array<*>} set1
 * @param {!Array<*>} set2
 * @param {string=} opt_message
 */
assert.sameMembers = function(set1, set2, opt_message) {};

/**
 * @param {!Array<*>} set1
 * @param {!Array<*>} set2
 * @param {string=} opt_message
 */
assert.sameDeepMembers = function(set1, set2, opt_message) {};

// Below are the externs for the APIs to build custom assertions:
// http://www.chaijs.com/api/plugins/

/**
 * @param {string} name
 * @param {!Function} getter
 */
chai.Assertion.addMethod = function(name, getter) {};

/**
 * @param {string} name
 * @param {function()} getter
 */
chai.Assertion.addProperty = function(name, getter) {};

/** @type {*} */
chai.Assertion.prototype._obj;

/**
 * @param {boolean} expr
 * @param {string} msg
 * @param {string} negateMsg
 * @param {*} expected
 * @param {*=} actual
 * @param {boolean=} showDiff
 */
chai.Assertion.prototype.assert = function(
    expr, msg, negateMsg, expected, actual, showDiff) {};

/** @const */
chai.assert = assert;

/** @const */
chai.expect = expect;

/** @const */
chai.util = {};

/**
 * @param {!chai.Assertion} obj
 * @param {string} key
 * @param {*=} value
 */
chai.util.flag = function(obj, key, value) {};
