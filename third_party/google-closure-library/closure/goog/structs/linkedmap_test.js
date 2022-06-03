/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.structs.LinkedMapTest');
goog.setTestOnly();

const LinkedMap = goog.require('goog.structs.LinkedMap');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

function fillLinkedMap(m) {
  m.set('a', 0);
  m.set('b', 1);
  m.set('c', 2);
  m.set('d', 3);
}

const someObj = {};

testSuite({
  testLinkedMap() {
    const m = new LinkedMap();
    fillLinkedMap(m);

    assertArrayEquals(['a', 'b', 'c', 'd'], m.getKeys());
    assertArrayEquals([0, 1, 2, 3], m.getValues());
  },

  testMaxSizeLinkedMap() {
    const m = new LinkedMap(3);
    fillLinkedMap(m);

    assertArrayEquals(['b', 'c', 'd'], m.getKeys());
    assertArrayEquals([1, 2, 3], m.getValues());
  },

  testLruLinkedMap() {
    const m = new LinkedMap(undefined, true);
    fillLinkedMap(m);

    assertArrayEquals(['d', 'c', 'b', 'a'], m.getKeys());
    assertArrayEquals([3, 2, 1, 0], m.getValues());

    m.get('a');
    assertArrayEquals(['a', 'd', 'c', 'b'], m.getKeys());
    assertArrayEquals([0, 3, 2, 1], m.getValues());

    m.set('b', 4);
    assertArrayEquals(['b', 'a', 'd', 'c'], m.getKeys());
    assertArrayEquals([4, 0, 3, 2], m.getValues());
  },

  testMaxSizeLruLinkedMap() {
    const m = new LinkedMap(3, true);
    fillLinkedMap(m);

    assertArrayEquals(['d', 'c', 'b'], m.getKeys());
    assertArrayEquals([3, 2, 1], m.getValues());

    m.get('c');
    assertArrayEquals(['c', 'd', 'b'], m.getKeys());
    assertArrayEquals([2, 3, 1], m.getValues());

    m.set('d', 4);
    assertArrayEquals(['d', 'c', 'b'], m.getKeys());
    assertArrayEquals([4, 2, 1], m.getValues());
  },

  testMaxSizeLruLinkedMapWithEvictionCallback() {
    const cb = recordFunction();
    const m = new LinkedMap(4, true, cb);
    fillLinkedMap(m);
    assertEquals(0, cb.getCallCount());  // But cache is full.
    assertArrayEquals(['d', 'c', 'b', 'a'], m.getKeys());
    m.set('d', 'exists');
    assertEquals(0, cb.getCallCount());
    m.set('extra1', 'val1');
    assertEquals(1, cb.getCallCount());
    assertArrayEquals(['a', 0], cb.getLastCall().getArguments());
    m.set('extra2', 'val2');
    assertEquals(2, cb.getCallCount());
    assertArrayEquals(['b', 1], cb.getLastCall().getArguments());
    m.set('extra2', 'val2_2');
    assertEquals(2, cb.getCallCount());
  },

  testGetCount() {
    const m = new LinkedMap();
    assertEquals(0, m.getCount());
    m.set('a', 0);
    assertEquals(1, m.getCount());
    m.set('a', 1);
    assertEquals(1, m.getCount());
    m.set('b', 2);
    assertEquals(2, m.getCount());
    m.remove('a');
    assertEquals(1, m.getCount());
  },

  testIsEmpty() {
    const m = new LinkedMap();
    assertTrue(m.isEmpty());
    m.set('a', 0);
    assertFalse(m.isEmpty());
    m.remove('a');
    assertTrue(m.isEmpty());
  },

  testSetMaxCount() {
    const m = new LinkedMap(3);
    fillLinkedMap(m);
    assertEquals(3, m.getCount());

    m.setMaxCount(5);
    m.set('e', 5);
    m.set('f', 6);
    m.set('g', 7);
    assertEquals(5, m.getCount());

    m.setMaxCount(4);
    assertEquals(4, m.getCount());

    m.setMaxCount(0);
    m.set('h', 8);
    m.set('i', 9);
    m.set('j', 10);
    assertEquals(7, m.getCount());
  },

  testClear() {
    const m = new LinkedMap();
    fillLinkedMap(m);
    m.clear();
    assertTrue(m.isEmpty());
  },

  testForEach() {
    const m = new LinkedMap();
    fillLinkedMap(m);

    m.forEach(function(val, key, linkedMap) {
      linkedMap.set(key, val * 2);
      assertEquals('forEach should run in provided context.', someObj, this);
    }, someObj);

    assertArrayEquals(['a', 'b', 'c', 'd'], m.getKeys());
    assertArrayEquals([0, 2, 4, 6], m.getValues());
  },

  testMap() {
    const m = new LinkedMap();
    fillLinkedMap(m);

    const result = m.map(function(val, key, linkedMap) {
      assertEquals('The LinkedMap object should get passed in', m, linkedMap);
      assertEquals('map should run in provided context', someObj, this);
      return key + val;
    }, someObj);

    assertArrayEquals(['a0', 'b1', 'c2', 'd3'], result);
  },

  testSome() {
    const m = new LinkedMap();
    fillLinkedMap(m);

    const result = m.some(function(val, key, linkedMap) {
      assertEquals('The LinkedMap object should get passed in', m, linkedMap);
      assertEquals('map should run in provided context', someObj, this);
      return val > 2;
    }, someObj);

    assertTrue(result);
    assertFalse(m.some((val) => val > 3));

    assertTrue(m.some((val, key) => key == 'c'));
    assertFalse(m.some((val, key) => key == 'e'));
  },

  testEvery() {
    const m = new LinkedMap();
    fillLinkedMap(m);

    const result = m.every(function(val, key, linkedMap) {
      assertEquals('The LinkedMap object should get passed in', m, linkedMap);
      assertEquals('map should run in provided context', someObj, this);
      return val < 5;
    }, someObj);

    assertTrue(result);
    assertFalse(m.every((val) => val < 2));

    assertTrue(m.every((val, key) => key.length == 1));
    assertFalse(m.every((val, key) => key == 'b'));
  },

  testPeek() {
    const m = new LinkedMap();
    assertEquals(undefined, m.peek());
    assertEquals(undefined, m.peekLast());

    fillLinkedMap(m);
    assertEquals(0, m.peek());

    m.remove('a');
    assertEquals(1, m.peek());

    assertEquals(3, m.peekLast());

    assertEquals(3, m.peekValue('d'));
    assertEquals(1, m.peek());

    m.remove('d');
    assertEquals(2, m.peekLast());
  },

  testPop() {
    const m = new LinkedMap();
    assertEquals(undefined, m.shift());
    assertEquals(undefined, m.pop());

    fillLinkedMap(m);
    assertEquals(4, m.getCount());

    assertEquals(0, m.shift());
    assertEquals(1, m.peek());

    assertEquals(3, m.pop());
    assertEquals(2, m.peekLast());

    assertEquals(2, m.getCount());
  },

  testContains() {
    const m = new LinkedMap();
    fillLinkedMap(m);

    assertTrue(m.contains(2));
    assertFalse(m.contains(4));
  },

  testContainsKey() {
    const m = new LinkedMap();
    fillLinkedMap(m);

    assertTrue(m.containsKey('b'));
    assertFalse(m.containsKey('elephant'));
    assertFalse(m.containsKey('undefined'));
  },

  /**
     @suppress {visibility,missingProperties} suppression added to enable type
     checking
   */
  testRemoveNodeCalls() {
    const m = new LinkedMap(1);
    /** @suppress {visibility} suppression added to enable type checking */
    m.removeNode = recordFunction(m.removeNode);

    m.set('1', 1);
    assertEquals(
        'removeNode not called after adding an element', 0,
        m.removeNode.getCallCount());
    m.set('1', 2);
    assertEquals(
        'removeNode not called after updating an element', 0,
        m.removeNode.getCallCount());
    m.set('2', 2);
    assertEquals(
        'removeNode called after adding an overflowing element', 1,
        m.removeNode.getCallCount());

    m.remove('3');
    assertEquals(
        'removeNode not called after removing a non-existing element', 1,
        m.removeNode.getCallCount());
    m.remove('2');
    assertEquals(
        'removeNode called after removing an existing element', 2,
        m.removeNode.getCallCount());

    m.set('1', 1);
    m.clear();
    assertEquals(
        'removeNode called after clearing the map', 3,
        m.removeNode.getCallCount());
    m.clear();
    assertEquals(
        'removeNode not called after clearing an empty map', 3,
        m.removeNode.getCallCount());

    m.set('1', 1);
    m.pop();
    assertEquals(
        'removeNode called after calling pop', 4, m.removeNode.getCallCount());
    m.pop();
    assertEquals(
        'removeNode not called after calling pop on an empty map', 4,
        m.removeNode.getCallCount());

    m.set('1', 1);
    m.shift();
    assertEquals(
        'removeNode called after calling shift', 5,
        m.removeNode.getCallCount());
    m.shift();
    assertEquals(
        'removeNode not called after calling shift on an empty map', 5,
        m.removeNode.getCallCount());

    m.setMaxCount(2);
    m.set('1', 1);
    m.set('2', 2);
    assertEquals(
        'removeNode not called after increasing the maximum map size', 5,
        m.removeNode.getCallCount());
    m.setMaxCount(1);
    assertEquals(
        'removeNode called after decreasing the maximum map size', 6,
        m.removeNode.getCallCount());
  },
});
