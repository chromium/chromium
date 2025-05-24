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


goog.module('goog.structs.PriorityQueue');
goog.module.declareLegacyNamespace();

const Heap = goog.require('goog.structs.Heap');


/**
 * Class for Priority Queue datastructure.
 *
 * @extends {Heap<number, VALUE>}
 * @template VALUE
 * @final
 */
class PriorityQueue extends Heap {
  /**
   * Puts the specified value in the queue.
   * @param {number} priority The priority of the value. A smaller value here
   *     means a higher priority.
   * @param {VALUE} value The value.
   */
  enqueue(priority, value) {
    this.insert(priority, value);
  }

  /**
   * Retrieves and removes the head of this queue.
   * @return {VALUE} The element at the head of this queue. Returns undefined if
   *     the queue is empty.
   */
  dequeue() {
    return this.remove();
  }
}
exports = PriorityQueue;
