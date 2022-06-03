/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @fileoverview Datastructure: Circular Buffer.
 *
 * Implements a buffer with a maximum size. New entries override the oldest
 * entries when the maximum size has been reached.
 */


goog.provide('goog.structs.CircularBuffer');



/**
 * Class for CircularBuffer.
 * @param {number=} opt_maxSize The maximum size of the buffer.
 * @constructor
 * @template T
 */
goog.structs.CircularBuffer = function(opt_maxSize) {
  'use strict';
  /**
   * Index of the next element in the circular array structure.
   * @private {number}
   */
  this.nextPtr_ = 0;

  /**
   * Maximum size of the circular array structure.
   * @private {number}
   */
  this.maxSize_ = opt_maxSize || 100;

  /**
   * Underlying array for the CircularBuffer.
   * @private {!Array<T>}
   */
  this.buff_ = [];
};


/**
 * Adds an item to the buffer. May remove the oldest item if the buffer is at
 * max size.
 * @param {T} item The item to add.
 * @return {T|undefined} The removed old item, if the buffer is at max size.
 *     Return undefined, otherwise.
 */
goog.structs.CircularBuffer.prototype.add = function(item) {
  'use strict';
  const previousItem = this.buff_[this.nextPtr_];
  this.buff_[this.nextPtr_] = item;
  this.nextPtr_ = (this.nextPtr_ + 1) % this.maxSize_;
  return previousItem;
};


/**
 * Returns the item at the specified index.
 * @param {number} index The index of the item. The index of an item can change
 *     after calls to `add()` if the buffer is at maximum size.
 * @return {T} The item at the specified index.
 */
goog.structs.CircularBuffer.prototype.get = function(index) {
  'use strict';
  index = this.normalizeIndex_(index);
  return this.buff_[index];
};


/**
 * Sets the item at the specified index.
 * @param {number} index The index of the item. The index of an item can change
 *     after calls to `add()` if the buffer is at maximum size.
 * @param {T} item The item to add.
 */
goog.structs.CircularBuffer.prototype.set = function(index, item) {
  'use strict';
  index = this.normalizeIndex_(index);
  this.buff_[index] = item;
};


/**
 * Returns the current number of items in the buffer.
 * @return {number} The current number of items in the buffer.
 */
goog.structs.CircularBuffer.prototype.getCount = function() {
  'use strict';
  return this.buff_.length;
};


/**
 * @return {boolean} Whether the buffer is empty.
 */
goog.structs.CircularBuffer.prototype.isEmpty = function() {
  'use strict';
  return this.buff_.length == 0;
};


/**
 * Empties the current buffer.
 */
goog.structs.CircularBuffer.prototype.clear = function() {
  'use strict';
  this.buff_.length = 0;
  this.nextPtr_ = 0;
};


/**
 * @return {!Array<T>} The values in the buffer ordered from oldest to newest.
 */
goog.structs.CircularBuffer.prototype.getValues = function() {
  'use strict';
  // getNewestValues returns all the values if the maxCount parameter is the
  // count
  return this.getNewestValues(this.getCount());
};


/**
 * Returns the newest values in the buffer up to `count`.
 * @param {number} maxCount The maximum number of values to get. Should be a
 *     positive number.
 * @return {!Array<T>} The newest values in the buffer up to `count`. The
 *     values are ordered from oldest to newest.
 */
goog.structs.CircularBuffer.prototype.getNewestValues = function(maxCount) {
  'use strict';
  const l = this.getCount();
  const start = this.getCount() - maxCount;
  const rv = [];
  for (let i = start; i < l; i++) {
    rv.push(this.get(i));
  }
  return rv;
};


/** @return {!Array<number>} The indexes in the buffer. */
goog.structs.CircularBuffer.prototype.getKeys = function() {
  'use strict';
  const rv = [];
  const l = this.getCount();
  for (let i = 0; i < l; i++) {
    rv[i] = i;
  }
  return rv;
};


/**
 * Whether the buffer contains the key/index.
 * @param {number} key The key/index to check for.
 * @return {boolean} Whether the buffer contains the key/index.
 */
goog.structs.CircularBuffer.prototype.containsKey = function(key) {
  'use strict';
  return key < this.getCount();
};


/**
 * Whether the buffer contains the given value.
 * @param {T} value The value to check for.
 * @return {boolean} Whether the buffer contains the given value.
 */
goog.structs.CircularBuffer.prototype.containsValue = function(value) {
  'use strict';
  const l = this.getCount();
  for (let i = 0; i < l; i++) {
    if (this.get(i) == value) {
      return true;
    }
  }
  return false;
};


/**
 * Returns the last item inserted into the buffer.
 * @return {T|null} The last item inserted into the buffer,
 *     or null if the buffer is empty.
 */
goog.structs.CircularBuffer.prototype.getLast = function() {
  'use strict';
  if (this.getCount() == 0) {
    return null;
  }
  return this.get(this.getCount() - 1);
};


/**
 * Helper function to convert an index in the number space of oldest to
 * newest items in the array to the position that the element will be at in the
 * underlying array.
 * @param {number} index The index of the item in a list ordered from oldest to
 *     newest.
 * @return {number} The index of the item in the CircularBuffer's underlying
 *     array.
 * @private
 */
goog.structs.CircularBuffer.prototype.normalizeIndex_ = function(index) {
  'use strict';
  if (index >= this.buff_.length) {
    throw new Error('Out of bounds exception');
  }

  if (this.buff_.length < this.maxSize_) {
    return index;
  }

  return (this.nextPtr_ + Number(index)) % this.maxSize_;
};
