/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.async.WorkQueueTest');
goog.setTestOnly();

const WorkQueue = goog.require('goog.async.WorkQueue');
const testSuite = goog.require('goog.testing.testSuite');

const id = 0;
let queue = null;

testSuite({
  setUp() {
    queue = new WorkQueue();
  },

  tearDown() {
    queue = null;
  },

  testEntriesReturnedInOrder() {
    const fn1 = () => {};
    const scope1 = {};
    const fn2 = () => {};
    const scope2 = {};
    queue.add(fn1, scope1);
    queue.add(fn2, scope2);

    let item = queue.remove();
    assertEquals(fn1, item.fn);
    assertEquals(scope1, item.scope);
    assertNull(item.next);

    item = queue.remove();
    assertEquals(fn2, item.fn);
    assertEquals(scope2, item.scope);
    assertNull(item.next);

    item = queue.remove();
    assertNull(item);
  },

  /** @suppress {visibility} access private fields */
  testReturnedItemReused() {
    const fn1 = () => {};
    const scope1 = {};

    const fn2 = () => {};
    const scope2 = {};

    assertEquals(0, WorkQueue.freelist_.occupants());

    queue.add(fn1, scope1);
    const item1 = queue.remove();

    assertEquals(0, WorkQueue.freelist_.occupants());

    queue.returnUnused(item1);

    assertEquals(1, WorkQueue.freelist_.occupants());

    queue.add(fn2, scope2);

    assertEquals(0, WorkQueue.freelist_.occupants());

    const item2 = queue.remove();

    assertEquals(item1, item2);
  },

  testEmptyQueueReturnNull() {
    const item1 = queue.remove();
    assertNull(item1);
  },
});
