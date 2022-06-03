/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.structs.StringSetTest');
goog.setTestOnly();

const StringSet = goog.require('goog.structs.StringSet');
const asserts = goog.require('goog.testing.asserts');
const iter = goog.require('goog.iter');
const testSuite = goog.require('goog.testing.testSuite');

const TEST_VALUES = [
  '',
  ' ',
  '  ',
  'true',
  'null',
  'undefined',
  '0',
  'new',
  'constructor',
  'prototype',
  '__proto__',
  'set',
  'hasOwnProperty',
  'toString',
  'valueOf',
];

const TEST_VALUES_WITH_DUPLICATES = [
  '',          '',
  ' ',         '  ',
  'true',      true,
  'null',      null,
  'undefined', undefined,
  '0',         0,
  'new',       'constructor',
  'prototype', '__proto__',
  'set',       'hasOwnProperty',
  'toString',  'valueOf',
];

testSuite({
  testConstructor() {
    const empty = new StringSet;
    assertSameElements('elements in empty set', [], empty.getValues());

    const s = new StringSet(TEST_VALUES_WITH_DUPLICATES);
    assertSameElements(
        'duplicates are filtered out by their string value', TEST_VALUES,
        s.getValues());
  },

  testConstructorAssertsThatObjectPrototypeHasNoEnumerableKeys() {
    assertNotThrows(() => new StringSet());
    Object.prototype.foo = 0;
    try {
      assertThrows(() => new StringSet());
    } finally {
      delete Object.prototype.foo;
    }
    assertNotThrows(() => new StringSet());
  },

  testOverridingObjectPrototypeToStringIsSafe() {
    const originalToString = Object.prototype.toString;
    Object.prototype.toString = () => 'object';
    try {
      assertEquals(0, new StringSet().getCount());
      assertFalse(new StringSet().contains('toString'));
    } finally {
      Object.prototype.toString = originalToString;
    }
  },

  testAdd() {
    const s = new StringSet;
    TEST_VALUES_WITH_DUPLICATES.forEach(s.add, s);
    assertSameElements(TEST_VALUES, s.getValues());
  },

  testAddArray() {
    const s = new StringSet;
    s.addArray(TEST_VALUES_WITH_DUPLICATES);
    assertSameElements('added elements from array', TEST_VALUES, s.getValues());
  },

  testAddSet() {
    const s = new StringSet;
    s.addSet(new StringSet([1, 2]));
    assertSameElements('empty set + {1, 2}', ['1', '2'], s.getValues());
    s.addSet(new StringSet([2, 3]));
    assertSameElements('{1, 2} + {2, 3}', ['1', '2', '3'], s.getValues());
  },

  testClear() {
    const s = new StringSet([1, 2]);
    s.clear();
    assertSameElements('cleared set', [], s.getValues());
  },

  testClone() {
    const s = new StringSet([1, 2]);
    const c = s.clone();
    assertSameElements('elements in clone', ['1', '2'], c.getValues());
    s.add(3);
    assertSameElements(
        'changing the original set does not affect the clone', ['1', '2'],
        c.getValues());
  },

  testContains() {
    const e = new StringSet;
    TEST_VALUES.forEach(element => {
      assertFalse(`empty set does not contain ${element}`, e.contains(element));
      assertFalse(`empty set does not contain ${element}`, e.has(element));
    });

    const s = new StringSet(TEST_VALUES);
    TEST_VALUES_WITH_DUPLICATES.forEach(element => {
      assertTrue(`s contains ${element}`, s.contains(element));
      assertTrue(`s contains ${element}`, s.has(element));
    });
    assertFalse('s does not contain 42', s.contains(42));
    assertFalse('s does not contain 42', s.has(42));
  },

  testContainsArray() {
    const s = new StringSet(TEST_VALUES);
    assertTrue('set contains empty array', s.containsArray([]));
    assertTrue(
        'set contains all elements of itself with some duplication',
        s.containsArray(TEST_VALUES_WITH_DUPLICATES));
    assertFalse('set does not contain 42', s.containsArray([42]));
  },

  testEquals() {
    const s = new StringSet([1, 2]);
    assertTrue('set equals to itself', s.equals(s));
    assertTrue('set equals to its clone', s.equals(s.clone()));
    assertFalse(
        'set does not equal to its subset', s.equals(new StringSet([1])));
    assertFalse(
        'set does not equal to its superset',
        s.equals(new StringSet([1, 2, 3])));
  },

  testForEach() {
    const s = new StringSet(TEST_VALUES);
    const values = [];
    const context = {};
    s.forEach(function(value, key, stringSet) {
      assertEquals('context of forEach callback', context, this);
      values.push(value);
      assertUndefined('key argument of forEach callback', key);
      assertEquals('set argument of forEach callback', s, stringSet);
    }, context);
    assertSameElements(
        'values passed to forEach callback', TEST_VALUES, values);
  },

  testGetCount() {
    const empty = new StringSet;
    assertEquals('count(empty set)', 0, empty.getCount());

    const s = new StringSet(TEST_VALUES);
    assertEquals('count(non-empty set)', TEST_VALUES.length, s.getCount());
  },

  testGetDifference() {
    const s1 = new StringSet([1, 2]);
    const s2 = new StringSet([2, 3]);
    assertSameElements(
        '{1, 2} - {2, 3}', ['1'], s1.getDifference(s2).getValues());
  },

  testGetIntersection() {
    const s1 = new StringSet([1, 2]);
    const s2 = new StringSet([2, 3]);
    assertSameElements(
        '{1, 2} * {2, 3}', ['2'], s1.getIntersection(s2).getValues());
  },

  testGetSymmetricDifference() {
    const s1 = new StringSet([1, 2]);
    const s2 = new StringSet([2, 3]);
    assertSameElements(
        '{1, 2} sym.diff. {2, 3}', ['1', '3'],
        s1.getSymmetricDifference(s2).getValues());
  },

  testGetUnion() {
    const s1 = new StringSet([1, 2]);
    const s2 = new StringSet([2, 3]);
    assertSameElements(
        '{1, 2} + {2, 3}', ['1', '2', '3'], s1.getUnion(s2).getValues());
  },

  testIsDisjoint() {
    const s = new StringSet;
    const s12 = new StringSet([1, 2]);
    const s23 = new StringSet([2, 3]);
    const s34 = new StringSet([3, 4]);

    assertTrue('{} and {1, 2} are disjoint', s.isDisjoint(s12));
    assertTrue('{1, 2} and {3, 4} are disjoint', s12.isDisjoint(s34));
    assertFalse('{1, 2} and {2, 3} are not disjoint', s12.isDisjoint(s23));
  },

  testIsEmpty() {
    assertTrue('empty set', new StringSet().isEmpty());
    assertFalse('non-empty set', new StringSet(['']).isEmpty());
  },

  testIsSubsetOf() {
    const s1 = new StringSet([1]);
    const s12 = new StringSet([1, 2]);
    const s123 = new StringSet([1, 2, 3]);
    const s23 = new StringSet([2, 3]);

    assertTrue('{1, 2} is subset of {1, 2}', s12.isSubsetOf(s12));
    assertTrue('{1, 2} is subset of {1, 2, 3}', s12.isSubsetOf(s123));
    assertFalse('{1, 2} is not subset of {1}', s12.isSubsetOf(s1));
    assertFalse('{1, 2} is not subset of {2, 3}', s12.isSubsetOf(s23));
  },

  testIsSupersetOf() {
    const s1 = new StringSet([1]);
    const s12 = new StringSet([1, 2]);
    const s123 = new StringSet([1, 2, 3]);
    const s23 = new StringSet([2, 3]);

    assertTrue('{1, 2} is superset of {1}', s12.isSupersetOf(s1));
    assertTrue('{1, 2} is superset of {1, 2}', s12.isSupersetOf(s12));
    assertFalse('{1, 2} is not superset of {1, 2, 3}', s12.isSupersetOf(s123));
    assertFalse('{1, 2} is not superset of {2, 3}', s12.isSupersetOf(s23));
  },

  testRemove() {
    const n = new StringSet([1, 2]);
    assertFalse('3 not removed from {1, 2}', n.remove(3));
    assertSameElements('set == {1, 2}', ['1', '2'], n.getValues());
    assertTrue('2 removed from {1, 2}', n.remove(2));
    assertSameElements('set == {1}', ['1'], n.getValues());
    assertTrue('"1" removed from {1}', n.remove('1'));
    assertSameElements('set == {}', [], n.getValues());

    const s = new StringSet(TEST_VALUES);
    TEST_VALUES.forEach(s.remove, s);
    assertSameElements(
        'all special values have been removed', [], s.getValues());
  },

  testDelete() {
    const n = new StringSet([1, 2]);
    assertFalse('3 not deleted from {1, 2}', n.delete(3));
    assertSameElements('set == {1, 2}', ['1', '2'], n.values());
    assertTrue('2 deleted from {1, 2}', n.delete(2));
    assertSameElements('set == {1}', ['1'], n.values());
    assertTrue('"1" deleted from {1}', n.delete('1'));
    assertSameElements('set == {}', [], n.values());

    const s = new StringSet(TEST_VALUES);
    TEST_VALUES.forEach(s.delete, s);
    assertSameElements('all special values have been removed', [], s.values());
  },

  testRemoveArray() {
    const s = new StringSet(TEST_VALUES);
    s.removeArray(TEST_VALUES.slice(0, TEST_VALUES.length - 2));
    assertSameElements(
        'removed elements from array',
        TEST_VALUES.slice(TEST_VALUES.length - 2), s.getValues());
  },

  testRemoveSet() {
    const s1 = new StringSet([1, 2]);
    const s2 = new StringSet([2, 3]);
    s1.removeSet(s2);
    assertSameElements('{1, 2} - {2, 3}', ['1'], s1.getValues());
  },

  testIterator() {
    const s = new StringSet(TEST_VALUES_WITH_DUPLICATES);
    const values = [];
    iter.forEach(s, (value) => {
      values.push(value);
    });
    assertSameElements(
        '__iterator__ takes all elements once', TEST_VALUES, values);
  },
});
