/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Provides the built-in dictionary matcher methods like
 *     hasEntry, hasEntries, hasKey, hasValue, etc.
 */

goog.provide('goog.labs.testing.dictionarymatcher');

goog.require('goog.asserts');
goog.require('goog.labs.testing.Matcher');
goog.require('goog.object');



/**
 * The HasEntries matcher.
 *
 * @param {!Object} entries The entries to check in the object.
 *
 * @constructor
 * @struct
 * @implements {goog.labs.testing.Matcher}
 * @final
 */
goog.labs.testing.dictionarymatcher.HasEntriesMatcher = function(entries) {
  'use strict';
  /**
   * @type {Object}
   * @private
   */
  this.entries_ = entries;
};


/**
 * Determines if an object has particular entries.
 *
 * @override
 */
goog.labs.testing.dictionarymatcher.HasEntriesMatcher.prototype.matches =
    function(actualObject) {
  'use strict';
  goog.asserts.assertObject(actualObject, 'Expected an Object');
  var object = /** @type {!Object} */ (actualObject);
  return goog.object.every(this.entries_, function(value, key) {
    'use strict';
    return goog.object.containsKey(object, key) && object[key] === value;
  });
};


/**
 * @override
 */
goog.labs.testing.dictionarymatcher.HasEntriesMatcher.prototype.describe =
    function(actualObject) {
  'use strict';
  goog.asserts.assertObject(actualObject, 'Expected an Object');
  var object = /** @type {!Object} */ (actualObject);
  var errorString = 'Input object did not contain the following entries:\n';
  goog.object.forEach(this.entries_, function(value, key) {
    'use strict';
    if (!goog.object.containsKey(object, key) || object[key] !== value) {
      errorString += key + ': ' + value + '\n';
    }
  });
  return errorString;
};



/**
 * The HasEntry matcher.
 *
 * @param {string} key The key for the entry.
 * @param {*} value The value for the key.
 *
 * @constructor
 * @struct
 * @implements {goog.labs.testing.Matcher}
 * @final
 */
goog.labs.testing.dictionarymatcher.HasEntryMatcher = function(key, value) {
  'use strict';
  /**
   * @type {string}
   * @private
   */
  this.key_ = key;
  /**
   * @type {*}
   * @private
   */
  this.value_ = value;
};


/**
 * Determines if an object has a particular entry.
 *
 * @override
 */
goog.labs.testing.dictionarymatcher.HasEntryMatcher.prototype.matches =
    function(actualObject) {
  'use strict';
  goog.asserts.assertObject(actualObject);
  return goog.object.containsKey(actualObject, this.key_) &&
      actualObject[this.key_] === this.value_;
};


/**
 * @override
 */
goog.labs.testing.dictionarymatcher.HasEntryMatcher.prototype.describe =
    function(actualObject) {
  'use strict';
  goog.asserts.assertObject(actualObject);
  var errorMsg;
  if (goog.object.containsKey(actualObject, this.key_)) {
    errorMsg = 'Input object did not contain key: ' + this.key_;
  } else {
    errorMsg = 'Value for key did not match value: ' + this.value_;
  }
  return errorMsg;
};



/**
 * The HasKey matcher.
 *
 * @param {string} key The key to check in the object.
 *
 * @constructor
 * @struct
 * @implements {goog.labs.testing.Matcher}
 * @final
 */
goog.labs.testing.dictionarymatcher.HasKeyMatcher = function(key) {
  'use strict';
  /**
   * @type {string}
   * @private
   */
  this.key_ = key;
};


/**
 * Determines if an object has a key.
 *
 * @override
 */
goog.labs.testing.dictionarymatcher.HasKeyMatcher.prototype.matches = function(
    actualObject) {
  'use strict';
  goog.asserts.assertObject(actualObject);
  return goog.object.containsKey(actualObject, this.key_);
};


/**
 * @override
 */
goog.labs.testing.dictionarymatcher.HasKeyMatcher.prototype.describe = function(
    actualObject) {
  'use strict';
  goog.asserts.assertObject(actualObject);
  return 'Input object did not contain the key: ' + this.key_;
};



/**
 * The HasValue matcher.
 *
 * @param {*} value The value to check in the object.
 *
 * @constructor
 * @struct
 * @implements {goog.labs.testing.Matcher}
 * @final
 */
goog.labs.testing.dictionarymatcher.HasValueMatcher = function(value) {
  'use strict';
  /**
   * @type {*}
   * @private
   */
  this.value_ = value;
};


/**
 * Determines if an object contains a value
 *
 * @override
 */
goog.labs.testing.dictionarymatcher.HasValueMatcher.prototype.matches =
    function(actualObject) {
  'use strict';
  goog.asserts.assertObject(actualObject, 'Expected an Object');
  var object = /** @type {!Object} */ (actualObject);
  return goog.object.containsValue(object, this.value_);
};


/**
 * @override
 */
goog.labs.testing.dictionarymatcher.HasValueMatcher.prototype.describe =
    function(actualObject) {
  'use strict';
  return 'Input object did not contain the value: ' + this.value_;
};


/**
 * Gives a matcher that asserts an object contains all the given key-value pairs
 * in the input object.
 *
 * @param {!Object} entries The entries to check for presence in the object.
 * @return {!goog.labs.testing.dictionarymatcher.HasEntriesMatcher} A
 *     HasEntriesMatcher.
 */
goog.labs.testing.dictionarymatcher.HasEntriesMatcher.hasEntries = function(
    entries) {
  'use strict';
  return new goog.labs.testing.dictionarymatcher.HasEntriesMatcher(entries);
};


/**
 * Gives a matcher that asserts an object contains the given key-value pair.
 *
 * @param {string} key The key to check for presence in the object.
 * @param {*} value The value to check for presence in the object.
 * @return {!goog.labs.testing.dictionarymatcher.HasEntryMatcher} A
 *     HasEntryMatcher.
 */
goog.labs.testing.dictionarymatcher.HasEntryMatcher.hasEntry = function(
    key, value) {
  'use strict';
  return new goog.labs.testing.dictionarymatcher.HasEntryMatcher(key, value);
};


/**
 * Gives a matcher that asserts an object contains the given key.
 *
 * @param {string} key The key to check for presence in the object.
 * @return {!goog.labs.testing.dictionarymatcher.HasKeyMatcher} A HasKeyMatcher.
 */
goog.labs.testing.dictionarymatcher.HasKeyMatcher.hasKey = function(key) {
  'use strict';
  return new goog.labs.testing.dictionarymatcher.HasKeyMatcher(key);
};


/**
 * Gives a matcher that asserts an object contains the given value.
 *
 * @param {*} value The value to check for presence in the object.
 * @return {!goog.labs.testing.dictionarymatcher.HasValueMatcher} A
 *     HasValueMatcher.
 */
goog.labs.testing.dictionarymatcher.HasValueMatcher.hasValue = function(value) {
  'use strict';
  return new goog.labs.testing.dictionarymatcher.HasValueMatcher(value);
};
