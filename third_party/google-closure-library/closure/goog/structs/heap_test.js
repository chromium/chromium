/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.structs.HeapTest');
goog.setTestOnly();

const Heap = goog.require('goog.structs.Heap');
const structs = goog.require('goog.structs');
const testSuite = goog.require('goog.testing.testSuite');

/**
 * Constructs a heap from key-value pairs passed as arguments
 * @param {...!Array} var_args List of length-2 arrays [key, value]
 * @return {!Heap} Heap constructed from passed in key-value pairs
 */
function makeHeap(var_args) {
  const h = new Heap();
  let key;
  let value;

  for (let i = 0; i < arguments.length; i++) {
    key = arguments[i][0];
    value = arguments[i][1];
    h.insert(key, value);
  }

  return h;
}

testSuite({
  testGetCount1() {
    const h = makeHeap([0, 'a'], [1, 'b'], [2, 'c'], [3, 'd']);
    assertEquals('count, should be 4', 4, h.getCount());
    h.remove();
    assertEquals('count, should be 3', 3, h.getCount());
  },

  testGetCount2() {
    const h = makeHeap([0, 'a'], [1, 'b'], [2, 'c'], [3, 'd']);
    h.remove();
    h.remove();
    h.remove();
    h.remove();
    assertEquals('count, should be 0', 0, h.getCount());
  },

  testKeys() {
    const h = makeHeap([0, 'a'], [1, 'b'], [2, 'c'], [3, 'd']);
    const keys = h.getKeys();
    for (let i = 0; i < 4; i++) {
      assertTrue(`getKeys, key ${i} found`, structs.contains(keys, i));
    }
    assertEquals('getKeys, Should be 4 keys', 4, structs.getCount(keys));
  },

  testValues() {
    const h = makeHeap([0, 'a'], [1, 'b'], [2, 'c'], [3, 'd']);
    const values = h.getValues();

    assertTrue('getKeys, value "a" found', structs.contains(values, 'a'));
    assertTrue('getKeys, value "b" found', structs.contains(values, 'b'));
    assertTrue('getKeys, value "c" found', structs.contains(values, 'c'));
    assertTrue('getKeys, value "d" found', structs.contains(values, 'd'));
    assertEquals('getKeys, Should be 4 keys', 4, structs.getCount(values));
  },

  testContainsKey() {
    const h = makeHeap([0, 'a'], [1, 'b'], [2, 'c'], [3, 'd']);

    for (let i = 0; i < 4; i++) {
      assertTrue(`containsKey, key ${i} found`, h.containsKey(i));
    }
    assertFalse('containsKey, value 4 not found', h.containsKey(4));
  },

  testContainsValue() {
    const h = makeHeap([0, 'a'], [1, 'b'], [2, 'c'], [3, 'd']);

    assertTrue('containsValue, value "a" found', h.containsValue('a'));
    assertTrue('containsValue, value "b" found', h.containsValue('b'));
    assertTrue('containsValue, value "c" found', h.containsValue('c'));
    assertTrue('containsValue, value "d" found', h.containsValue('d'));
    assertFalse('containsValue, value "e" not found', h.containsValue('e'));
  },

  testClone() {
    const h = makeHeap([0, 'a'], [1, 'b'], [2, 'c'], [3, 'd']);
    const h2 = h.clone();
    assertTrue('clone so it should not be empty', !h2.isEmpty());
    assertTrue('clone so it should contain key 0', h2.containsKey(0));
    assertTrue('clone so it should contain value "a"', h2.containsValue('a'));
  },

  testClear() {
    const h = makeHeap([0, 'a'], [1, 'b'], [2, 'c'], [3, 'd']);
    h.clear();
    assertTrue('cleared so it should be empty', h.isEmpty());
  },

  testIsEmpty() {
    const h = makeHeap([0, 'a'], [1, 'b'], [2, 'c'], [3, 'd']);
    assertFalse('4 values so should not be empty', h.isEmpty());

    h.remove();
    h.remove();
    h.remove();
    assertFalse('1 values so should not be empty', h.isEmpty());

    h.remove();
    assertTrue('0 values so should be empty', h.isEmpty());
  },

  testPeek1() {
    const h = makeHeap([0, 'a'], [1, 'b'], [2, 'c'], [3, 'd']);
    assertEquals('peek, Should be "a"', 'a', h.peek());
  },

  testPeek2() {
    const h = makeHeap([1, 'b'], [3, 'd'], [0, 'a'], [2, 'c']);
    assertEquals('peek, Should be "a"', 'a', h.peek());
  },

  testPeek3() {
    const h = makeHeap([0, 'a'], [1, 'b'], [2, 'c'], [3, 'd']);
    h.clear();
    assertEquals('peek, Should be "undefined"', undefined, h.peek());
  },

  testPeekKey1() {
    const h = makeHeap([0, 'a'], [1, 'b'], [2, 'c'], [3, 'd']);
    assertEquals('peekKey, Should be "0"', 0, h.peekKey());
  },

  testPeekKey2() {
    const h = makeHeap([1, 'b'], [3, 'd'], [0, 'a'], [2, 'c']);
    assertEquals('peekKey, Should be "0"', 0, h.peekKey());
  },

  testPeekKey3() {
    const h = makeHeap([0, 'a'], [1, 'b'], [2, 'c'], [3, 'd']);
    h.clear();
    assertEquals('peekKey, Should be "undefined"', undefined, h.peekKey());
  },

  testRemove1() {
    const h = makeHeap([0, 'a'], [1, 'b'], [2, 'c'], [3, 'd']);

    assertEquals('remove, Should be "a"', 'a', h.remove());
    assertEquals('remove, Should be "b"', 'b', h.remove());
    assertEquals('remove, Should be "c"', 'c', h.remove());
    assertEquals('remove, Should be "d"', 'd', h.remove());
  },

  testRemove2() {
    const h = makeHeap([1, 'b'], [3, 'd'], [0, 'a'], [2, 'c']);

    assertEquals('remove, Should be "a"', 'a', h.remove());
    assertEquals('remove, Should be "b"', 'b', h.remove());
    assertEquals('remove, Should be "c"', 'c', h.remove());
    assertEquals('remove, Should be "d"', 'd', h.remove());
  },

  testInsertPeek1() {
    const h = makeHeap();

    h.insert(3, 'd');
    assertEquals('peek, Should be "d"', 'd', h.peek());
    h.insert(2, 'c');
    assertEquals('peek, Should be "c"', 'c', h.peek());
    h.insert(1, 'b');
    assertEquals('peek, Should be "b"', 'b', h.peek());
    h.insert(0, 'a');
    assertEquals('peek, Should be "a"', 'a', h.peek());
  },

  testInsertPeek2() {
    const h = makeHeap();

    h.insert(1, 'b');
    assertEquals('peek, Should be "b"', 'b', h.peek());
    h.insert(3, 'd');
    assertEquals('peek, Should be "b"', 'b', h.peek());
    h.insert(0, 'a');
    assertEquals('peek, Should be "a"', 'a', h.peek());
    h.insert(2, 'c');
    assertEquals('peek, Should be "a"', 'a', h.peek());
  },

  testInsertAllPeek1() {
    const h1 = makeHeap([1, 'e']);
    const h2 = makeHeap([0, 'a'], [1, 'b'], [2, 'c'], [3, 'd']);

    h1.insertAll(h2);
    assertEquals('peek, should be "a"', 'a', h1.peek());
  },

  testInsertAllPeek2() {
    const h1 = makeHeap([-1, 'z']);
    const h2 = makeHeap([0, 'a'], [1, 'b'], [2, 'c'], [3, 'd']);

    h1.insertAll(h2);
    assertEquals('peek, should be "z"', 'z', h1.peek());
  },

  testInsertAllPeek3() {
    const h1 = makeHeap();
    const h2 = makeHeap([0, 'a'], [1, 'b'], [2, 'c'], [3, 'd']);

    h1.insertAll(h2);
    assertEquals('peek, should be "a"', 'a', h1.peek());
  },
});
