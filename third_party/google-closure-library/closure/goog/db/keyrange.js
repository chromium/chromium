/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Wrapper for a IndexedDB key range.
 */


goog.provide('goog.db.KeyRange');



/**
 * Creates a new IDBKeyRange wrapper object. Should not be created directly,
 * instead use one of the static factory methods. For example:
 * @see goog.db.KeyRange.bound
 * @see goog.db.KeyRange.lowerBound
 *
 * @param {!IDBKeyRange} range Underlying IDBKeyRange object.
 * @constructor
 * @final
 */
goog.db.KeyRange = function(range) {
  'use strict';
  /**
   * Underlying IDBKeyRange object.
   *
   * @type {!IDBKeyRange}
   * @private
   */
  this.range_ = range;
};


/**
 * The IDBKeyRange.
 * @type {!Object}
 * @private
 */
goog.db.KeyRange.IDB_KEY_RANGE_ =
    goog.global.IDBKeyRange || goog.global.webkitIDBKeyRange;


/**
 * Creates a new key range for a single value.
 * @param {IDBKeyType} key The single value in the range.
 * @return {!goog.db.KeyRange} The key range.
 * @suppress {strictMissingProperties} Part of the go/strict_warnings_migration
 */
goog.db.KeyRange.only = function(key) {
  'use strict';
  return new goog.db.KeyRange(goog.db.KeyRange.IDB_KEY_RANGE_.only(key));
};


/**
 * Creates a key range with upper and lower bounds.
 * @param {IDBKeyType} lower The value of the lower bound.
 * @param {IDBKeyType} upper The value of the upper bound.
 * @param {boolean=} opt_lowerOpen If true, the range excludes the lower bound
 *     value.
 * @param {boolean=} opt_upperOpen If true, the range excludes the upper bound
 *     value.
 * @return {!goog.db.KeyRange} The key range.
 * @suppress {strictMissingProperties} Part of the go/strict_warnings_migration
 */
goog.db.KeyRange.bound = function(lower, upper, opt_lowerOpen, opt_upperOpen) {
  'use strict';
  return new goog.db.KeyRange(goog.db.KeyRange.IDB_KEY_RANGE_.bound(
      lower, upper, opt_lowerOpen, opt_upperOpen));
};


/**
 * Creates a key range with a lower bound only, finishes at the last record.
 * @param {IDBKeyType} lower The value of the lower bound.
 * @param {boolean=} opt_lowerOpen If true, the range excludes the lower bound
 *     value.
 * @return {!goog.db.KeyRange} The key range.
 * @suppress {strictMissingProperties} Part of the go/strict_warnings_migration
 */
goog.db.KeyRange.lowerBound = function(lower, opt_lowerOpen) {
  'use strict';
  return new goog.db.KeyRange(
      goog.db.KeyRange.IDB_KEY_RANGE_.lowerBound(lower, opt_lowerOpen));
};


/**
 * Creates a key range with a upper bound only, starts at the first record.
 * @param {IDBKeyType} upper The value of the upper bound.
 * @param {boolean=} opt_upperOpen If true, the range excludes the upper bound
 *     value.
 * @return {!goog.db.KeyRange} The key range.
 * @suppress {strictMissingProperties} Part of the go/strict_warnings_migration
 */
goog.db.KeyRange.upperBound = function(upper, opt_upperOpen) {
  'use strict';
  return new goog.db.KeyRange(
      goog.db.KeyRange.IDB_KEY_RANGE_.upperBound(upper, opt_upperOpen));
};


/**
 * Returns underlying key range object. This is used in ObjectStore's openCursor
 * and count methods.
 * @return {!IDBKeyRange}
 */
goog.db.KeyRange.prototype.range = function() {
  'use strict';
  return this.range_;
};
