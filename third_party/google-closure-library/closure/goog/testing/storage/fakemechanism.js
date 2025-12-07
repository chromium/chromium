/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Provides a fake storage mechanism for testing.
 */

goog.module('goog.testing.storage.FakeMechanism');
goog.module.declareLegacyNamespace();
goog.setTestOnly('goog.testing.storage.FakeMechanism');

const IterableMechanism = goog.require('goog.storage.mechanism.IterableMechanism');
const Iterator = goog.require('goog.iter.Iterator');
const {ShimIterable} = goog.require('goog.iter.es6');



/**
 * Creates a fake iterable mechanism.
 *
 * @constructor
 * @extends {IterableMechanism}
 * @final
 */
const FakeMechanism = function() {
  /**
   * @type {!Map}
   * @private
   */
  this.storage_ = new Map();
};
goog.inherits(FakeMechanism, IterableMechanism);


/**
 * Set a value for a key.
 *
 * @param {string} key The key to set.
 * @param {string} value The string to save.
 * @override
 */
FakeMechanism.prototype.set = function(key, value) {
  this.storage_.set(key, value);
};


/**
 * Get the value stored under a key.
 *
 * @param {string} key The key to get.
 * @return {?string} The corresponding value, null if not found.
 * @override
 */
FakeMechanism.prototype.get = function(key) {
  if (this.storage_.has(key)) {
    return this.storage_.get(key);
  }
  return null;
};


/**
 * Remove a key and its value.
 *
 * @param {string} key The key to remove.
 * @override
 */
FakeMechanism.prototype.remove = function(key) {
  this.storage_.delete(key);
};


/**
 * Returns an iterator that iterates over the elements in the storage. Will
 * throw goog.iter.StopIteration after the last element.
 *
 * @param {boolean=} opt_keys True to iterate over the keys. False to iterate
 *     over the values.  The default value is false.
 * @return {!Iterator} The iterator.
 * @override
 */
FakeMechanism.prototype.__iterator__ = function(opt_keys) {
  return opt_keys ? ShimIterable.of(this.storage_.keys()).toGoog() :
                    ShimIterable.of(this.storage_.values()).toGoog();
};

exports = FakeMechanism;
