/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Interface for storing, retrieving and scanning data using some
 * persistence mechanism.
 */

goog.module('goog.storage.mechanism.IterableMechanism');
goog.module.declareLegacyNamespace();

const Mechanism = goog.require('goog.storage.mechanism.Mechanism');
const {Iterator: GoogIterator} = goog.require('goog.iter');
const {ShimIterable} = goog.require('goog.iter.es6');
const {assertString} = goog.require('goog.asserts');



/**
 * Interface for all iterable storage mechanisms.
 *
 * @constructor
 * @struct
 * @extends {Mechanism}
 * @implements {Iterable<!Array<string>>}
 * @abstract
 */
const IterableMechanism = function() {
  'use strict';
  IterableMechanism.base(this, 'constructor');
};
goog.inherits(IterableMechanism, Mechanism);


/**
 * Get the number of stored key-value pairs.
 *
 * Could be overridden in a subclass, as the default implementation is not very
 * efficient - it iterates over all keys.
 *
 * @return {number} Number of stored elements.
 */
IterableMechanism.prototype.getCount = function() {
  'use strict';
  let count = 0;
  for (const key of this) {
    assertString(key);
    count++;
  }
  return count;
};


/**
 * Returns an iterator that iterates over the elements in the storage. Will
 * throw goog.iter.StopIteration after the last element.
 *
 * @param {boolean=} opt_keys True to iterate over the keys. False to iterate
 *     over the values.  The default value is false.
 * @return {!GoogIterator} The iterator.
 * @deprecated Use ES6 iteration protocols instead.
 */
IterableMechanism.prototype.__iterator__ = goog.abstractMethod;


/**
 * Returns an interator that iterates over all the keys for elements in storage.
 *
 * @return {!IteratorIterable<string>}
 */
IterableMechanism.prototype[Symbol.iterator] = function() {
  return ShimIterable.of(this.__iterator__(true)).toEs6();
};


/**
 * Remove all key-value pairs.
 *
 * Could be overridden in a subclass, as the default implementation is not
 * very efficient - it iterates over all keys.
 */
IterableMechanism.prototype.clear = function() {
  'use strict';
  // This converts the keys to an array first because otherwise
  // removing while iterating results in unstable ordering of keys and
  // can skip keys or terminate early.
  const keys = Array.from(this);
  for (const key of keys) {
    this.remove(key);
  }
};

exports = IterableMechanism;
