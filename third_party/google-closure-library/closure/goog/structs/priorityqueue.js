/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Datastructure: Priority Queue.
 *
 *
 * This file provides the implementation of a Priority Queue. Smaller priorities
 * move to the front of the queue. If two values have the same priority,
 * it is arbitrary which value will come to the front of the queue first.
 */

// TODO(user): Should this rely on natural ordering via some Comparable
//     interface?


goog.provide('goog.structs.PriorityQueue');

goog.require('goog.structs.Heap');



/**
 * Class for Priority Queue datastructure.
 *
 * @constructor
 * @extends {goog.structs.Heap<number, VALUE>}
 * @template VALUE
 * @final
 */
goog.structs.PriorityQueue = function() {
  'use strict';
  goog.structs.Heap.call(this);
};
goog.inherits(goog.structs.PriorityQueue, goog.structs.Heap);


/**
 * Puts the specified value in the queue.
 * @param {number} priority The priority of the value. A smaller value here
 *     means a higher priority.
 * @param {VALUE} value The value.
 */
goog.structs.PriorityQueue.prototype.enqueue = function(priority, value) {
  'use strict';
  this.insert(priority, value);
};


/**
 * Retrieves and removes the head of this queue.
 * @return {VALUE} The element at the head of this queue. Returns undefined if
 *     the queue is empty.
 */
goog.structs.PriorityQueue.prototype.dequeue = function() {
  'use strict';
  return this.remove();
};
