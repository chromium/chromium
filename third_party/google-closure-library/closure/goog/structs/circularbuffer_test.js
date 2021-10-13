/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.structs.CircularBufferTest');
goog.setTestOnly();

const CircularBuffer = goog.require('goog.structs.CircularBuffer');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testCircularBuffer() {
    const buff = new CircularBuffer(2);
    assertUndefined(buff.add('first'));
    assertEquals(1, buff.getCount());
    assertEquals('first', buff.get(0));
    assertEquals('first', buff.getLast());
    assertUndefined(buff.add('second'));
    assertEquals(2, buff.getCount());
    assertEquals('first', buff.get(0));
    assertEquals('second', buff.get(1));
    assertEquals('second', buff.getLast());
    assertEquals('first', buff.add('third'));
    assertEquals(2, buff.getCount());
    assertEquals('second', buff.get(0));
    assertEquals('third', buff.get(1));
    assertEquals('third', buff.getLast());
  },

  testIsEmpty() {
    const buff = new CircularBuffer(2);
    assertTrue('initially empty', buff.isEmpty());
    assertUndefined(buff.add('first'));
    assertFalse('not empty after add empty', buff.isEmpty());
  },

  testClear() {
    const buff = new CircularBuffer(2);
    assertUndefined(buff.add('first'));
    buff.clear();
    assertTrue('should be empty after clear', buff.isEmpty());
  },

  testGetValues() {
    const buff = new CircularBuffer(2);
    assertUndefined(buff.add('first'));
    assertUndefined(buff.add('second'));
    assertArrayEquals(['first', 'second'], buff.getValues());
  },

  testGetNewestValues() {
    const buff = new CircularBuffer(5);
    assertUndefined(buff.add('first'));
    assertUndefined(buff.add('second'));
    assertUndefined(buff.add('third'));
    assertUndefined(buff.add('fourth'));
    assertUndefined(buff.add('fifth'));
    assertArrayEquals(['fourth', 'fifth'], buff.getNewestValues(2));
  },

  testGetKeys() {
    const buff = new CircularBuffer(2);
    assertUndefined(buff.add('first'));
    assertUndefined(buff.add('second'));
    assertArrayEquals([0, 1], buff.getKeys());
  },

  testContainsValue() {
    const buff = new CircularBuffer(2);
    assertUndefined(buff.add('first'));
    assertUndefined(buff.add('second'));
    assertTrue(buff.containsValue('first'));
    assertTrue(buff.containsValue('second'));
    assertFalse(buff.containsValue('third'));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testContainsKey() {
    const buff = new CircularBuffer(3);
    assertUndefined(buff.add('first'));
    assertUndefined(buff.add('second'));
    assertUndefined(buff.add('third'));
    assertTrue(buff.containsKey(0));
    assertTrue(buff.containsKey('0'));
    assertTrue(buff.containsKey(1));
    assertTrue(buff.containsKey('1'));
    assertTrue(buff.containsKey(2));
    assertTrue(buff.containsKey('2'));
    assertFalse(buff.containsKey(3));
    assertFalse(buff.containsKey('3'));
  },
});
