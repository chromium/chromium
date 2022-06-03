/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Generic queue for writing unit tests.
 */

goog.setTestOnly('goog.testing.TestQueue');
goog.provide('goog.testing.TestQueue');



/**
 * Generic queue for writing unit tests
 * @constructor
 */
goog.testing.TestQueue = function() {
  'use strict';
  /**
   * Events that have accumulated
   * @type {Array<Object>}
   * @private
   */
  this.events_ = [];
};


/**
 * Adds a new event onto the queue.
 * @param {Object} event The event to queue.
 */
goog.testing.TestQueue.prototype.enqueue = function(event) {
  'use strict';
  this.events_.push(event);
};


/**
 * Returns whether the queue is empty.
 * @return {boolean} Whether the queue is empty.
 */
goog.testing.TestQueue.prototype.isEmpty = function() {
  'use strict';
  return this.events_.length == 0;
};


/**
 * Gets the next event from the queue. Throws an exception if the queue is
 * empty.
 * @param {string=} opt_comment Comment if the queue is empty.
 * @return {Object} The next event from the queue.
 */
goog.testing.TestQueue.prototype.dequeue = function(opt_comment) {
  'use strict';
  if (this.isEmpty()) {
    throw new Error('Handler is empty: ' + opt_comment);
  }
  return this.events_.shift();
};
