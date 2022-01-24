/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Provides a JS storage class implementing the HTML5 Storage
 * interface.
 */


goog.setTestOnly('goog.testing.MockStorage');
goog.provide('goog.testing.MockStorage');


/**
 * A JS storage instance, implementing the HTML5 Storage interface.
 * See http://www.w3.org/TR/webstorage/ for details.
 *
 * @constructor
 * @implements {Storage}
 * @final
 */
goog.testing.MockStorage = function() {
  'use strict';
  /**
   * The underlying storage object.
   * @type {!Map}
   * @private
   */
  this.store_ = new Map();

  /**
   * The number of elements in the storage.
   * @type {number}
   */
  this.length = 0;
};


/**
 * Sets an item to the storage.
 * @param {string} key Storage key.
 * @param {*} value Storage value. Must be convertible to string.
 * @override
 */
goog.testing.MockStorage.prototype.setItem = function(key, value) {
  'use strict';
  this.store_.set(key, String(value));
  this.length = this.store_.size;
};


/**
 * Gets an item from the storage.  The item returned is the "structured clone"
 * of the value from setItem.  In practice this means it's the value cast to a
 * string.
 * @param {string} key Storage key.
 * @return {?string} Storage value for key; null if does not exist.
 * @override
 */
goog.testing.MockStorage.prototype.getItem = function(key) {
  'use strict';
  var val = this.store_.get(key);
  // Enforce that getItem returns string values.
  return (val != null) ? /** @type {string} */ (val) : null;
};


/**
 * Removes and item from the storage.
 * @param {string} key Storage key.
 * @override
 */
goog.testing.MockStorage.prototype.removeItem = function(key) {
  'use strict';
  this.store_.delete(key);
  this.length = this.store_.size;
};


/**
 * Clears the storage.
 * @override
 */
goog.testing.MockStorage.prototype.clear = function() {
  'use strict';
  this.store_.clear();
  this.length = 0;
};


/**
 * Returns the key at the given index.
 * @param {number} index The index for the key.
 * @return {?string} Key at the given index, null if not found.
 * @override
 */
goog.testing.MockStorage.prototype.key = function(index) {
  'use strict';
  let i = 0;
  for (const key of this.store_.keys()) {
    if (i == index) return key;
    i++;
  }
  return null;
};
