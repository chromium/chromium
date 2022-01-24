/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.structs.PriorityQueueTest');
goog.setTestOnly();

const PriorityQueue = goog.require('goog.structs.PriorityQueue');
const structs = goog.require('goog.structs');
const testSuite = goog.require('goog.testing.testSuite');

function getPriorityQueue() {
  const p = new PriorityQueue();
  p.enqueue(0, 'a');
  p.enqueue(1, 'b');
  p.enqueue(2, 'c');
  p.enqueue(3, 'd');
  return p;
}

function getPriorityQueue2() {
  const p = new PriorityQueue();
  p.insert(1, 'b');
  p.insert(3, 'd');
  p.insert(0, 'a');
  p.insert(2, 'c');
  return p;
}

testSuite({
  testGetCount1() {
    const p = getPriorityQueue();
    assertEquals('count, should be 4', p.getCount(), 4);
    p.dequeue();
    assertEquals('count, should be 3', p.getCount(), 3);
  },

  testGetCount2() {
    const p = getPriorityQueue();
    assertEquals('count, should be 4', p.getCount(), 4);
    p.dequeue();
    assertEquals('count, should be 3', p.getCount(), 3);
  },

  testGetCount3() {
    const p = getPriorityQueue();
    p.dequeue();
    p.dequeue();
    p.dequeue();
    p.dequeue();
    assertEquals('count, should be 0', p.getCount(), 0);
  },

  testKeys() {
    const p = getPriorityQueue();
    const keys = p.getKeys();
    for (let i = 0; i < 4; i++) {
      assertTrue(`getKeys, key ${i} found`, structs.contains(keys, i));
    }
    assertEquals('getKeys, Should be 4 keys', structs.getCount(keys), 4);
  },

  testValues() {
    const p = getPriorityQueue();
    const values = p.getValues();

    assertTrue('getKeys, value "a" found', structs.contains(values, 'a'));
    assertTrue('getKeys, value "b" found', structs.contains(values, 'b'));
    assertTrue('getKeys, value "c" found', structs.contains(values, 'c'));
    assertTrue('getKeys, value "d" found', structs.contains(values, 'd'));
    assertEquals('getKeys, Should be 4 keys', structs.getCount(values), 4);
  },

  testClear() {
    const p = getPriorityQueue();
    p.clear();
    assertTrue('cleared so it should be empty', p.isEmpty());
  },

  testIsEmpty() {
    const p = getPriorityQueue();
    assertFalse('4 values so should not be empty', p.isEmpty());

    p.dequeue();
    p.dequeue();
    p.dequeue();
    assertFalse('1 values so should not be empty', p.isEmpty());

    p.dequeue();
    assertTrue('0 values so should be empty', p.isEmpty());
  },

  testPeek1() {
    const p = getPriorityQueue();
    assertEquals('peek, Should be "a"', p.peek(), 'a');
  },

  testPeek2() {
    const p = getPriorityQueue2();
    assertEquals('peek, Should be "a"', p.peek(), 'a');
  },

  testPeek3() {
    const p = getPriorityQueue();
    p.clear();
    assertEquals('peek, Should be "a"', p.peek(), undefined);
  },

  testDequeue1() {
    const p = getPriorityQueue();

    assertEquals('dequeue, Should be "a"', p.dequeue(), 'a');
    assertEquals('dequeue, Should be "b"', p.dequeue(), 'b');
    assertEquals('dequeue, Should be "c"', p.dequeue(), 'c');
    assertEquals('dequeue, Should be "d"', p.dequeue(), 'd');
  },

  testDequeue2() {
    const p = getPriorityQueue2();

    assertEquals('dequeue, Should be "a"', p.dequeue(), 'a');
    assertEquals('dequeue, Should be "b"', p.dequeue(), 'b');
    assertEquals('dequeue, Should be "c"', p.dequeue(), 'c');
    assertEquals('dequeue, Should be "d"', p.dequeue(), 'd');
  },

  testEnqueuePeek1() {
    const p = new PriorityQueue();

    p.enqueue(3, 'd');
    assertEquals('peak, Should be "d"', p.peek(), 'd');
    p.enqueue(2, 'c');
    assertEquals('peak, Should be "c"', p.peek(), 'c');
    p.enqueue(1, 'b');
    assertEquals('peak, Should be "b"', p.peek(), 'b');
    p.enqueue(0, 'a');
    assertEquals('peak, Should be "a"', p.peek(), 'a');
  },

  testEnqueuePeek2() {
    const p = new PriorityQueue();

    p.enqueue(1, 'b');
    assertEquals('peak, Should be "b"', p.peek(), 'b');
    p.enqueue(3, 'd');
    assertEquals('peak, Should be "b"', p.peek(), 'b');
    p.enqueue(0, 'a');
    assertEquals('peak, Should be "a"', p.peek(), 'a');
    p.enqueue(2, 'c');
    assertEquals('peak, Should be "a"', p.peek(), 'a');
  },
});
