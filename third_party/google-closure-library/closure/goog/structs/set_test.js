/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.structs.SetTest');
goog.setTestOnly();

const StructsSet = goog.require('goog.structs.Set');
const iter = goog.require('goog.iter');
const structs = goog.require('goog.structs');
const testSuite = goog.require('goog.testing.testSuite');

function stringifySet(s) {
  return structs.getValues(s).join('');
}

/** Helper function to assert intersection is commutative. */
function assertIntersection(msg, set1, set2, expectedIntersection) {
  assertTrue(
      `${msg}: set1->set2`,
      set1.intersection(set2).equals(expectedIntersection));
  assertTrue(
      `${msg}: set2->set1`,
      set2.intersection(set1).equals(expectedIntersection));
}

/** Helper function to test removeAll. */
function assertRemoveAll(msg, elements1, elements2, expectedResult) {
  const set1 = new StructsSet(elements1);
  const set2 = new StructsSet(elements2);
  set1.removeAll(set2);

  assertTrue(
      `${msg}: set1 count increased after removeAll`,
      elements1.length >= set1.getCount());
  assertEquals(
      `${msg}: set2 count changed after removeAll`, elements2.length,
      set2.getCount());
  assertTrue(
      `${msg}: set1 size increased after removeAll`,
      elements1.length >= set1.size);
  assertEquals(
      `${msg}: set2 size changed after removeAll`, elements2.length, set2.size);
  assertTrue(`${msg}: wrong set1 after removeAll`, set1.equals(expectedResult));
  assertIntersection(
      `${msg}: non-empty intersection after removeAll`, set1, set2, []);
}

/**
 * Helper method for testEquals().
 * @param {Object} a First element to use in the tests.
 * @param {Object} b Second element to use in the tests.
 * @param {Object} c Third element to use in the tests.
 * @param {Object} d Fourth element to use in the tests.
 */
function helperForTestEquals(a, b, c, d) {
  const s = new StructsSet([a, b, c]);

  assertTrue('set == itself', s.equals(s));
  assertTrue('set == same set', s.equals(new StructsSet([a, b, c])));
  assertTrue('set == its clone', s.equals(s.clone()));
  assertTrue('set == array of same elements', s.equals([a, b, c]));
  assertTrue(
      'set == array of same elements in different order', s.equals([c, b, a]));

  assertFalse('set != empty set', s.equals(new StructsSet));
  assertFalse('set != its subset', s.equals(new StructsSet([a, c])));
  assertFalse('set != its superset', s.equals(new StructsSet([a, b, c, d])));
  assertFalse('set != different set', s.equals(new StructsSet([b, c, d])));
  assertFalse('set != its subset as array', s.equals([a, c]));
  assertFalse('set != its superset as array', s.equals([a, b, c, d]));
  assertFalse('set != different set as array', s.equals([b, c, d]));
  assertFalse('set != [a, b, c, c]', s.equals([a, b, c, c]));
  assertFalse('set != [a, b, b]', s.equals([a, b, b]));
  assertFalse('set != [a, a]', s.equals([a, a]));
}

/**
 * Helper method for testIsSubsetOf().
 * @param {Object} a First element to use in the tests.
 * @param {Object} b Second element to use in the tests.
 * @param {Object} c Third element to use in the tests.
 * @param {Object} d Fourth element to use in the tests.
 */
function helperForTestIsSubsetOf(a, b, c, d) {
  const s = new StructsSet([a, b, c]);

  assertTrue('set <= itself', s.isSubsetOf(s));
  assertTrue('set <= same set', s.isSubsetOf(new StructsSet([a, b, c])));
  assertTrue('set <= its clone', s.isSubsetOf(s.clone()));
  assertTrue('set <= array of same elements', s.isSubsetOf([a, b, c]));
  assertTrue(
      'set <= array of same elements in different order', s.equals([c, b, a]));

  assertTrue(
      'set <= Set([a, b, c, d])', s.isSubsetOf(new StructsSet([a, b, c, d])));
  assertTrue('set <= [a, b, c, d]', s.isSubsetOf([a, b, c, d]));
  assertTrue('set <= [a, b, c, c]', s.isSubsetOf([a, b, c, c]));

  assertFalse('set !<= Set([a, b])', s.isSubsetOf(new StructsSet([a, b])));
  assertFalse('set !<= [a, b]', s.isSubsetOf([a, b]));
  assertFalse('set !<= Set([c, d])', s.isSubsetOf(new StructsSet([c, d])));
  assertFalse('set !<= [c, d]', s.isSubsetOf([c, d]));
  assertFalse(
      'set !<= Set([a, c, d])', s.isSubsetOf(new StructsSet([a, c, d])));
  assertFalse('set !<= [a, c, d]', s.isSubsetOf([a, c, d]));
  assertFalse('set !<= [a, a, b]', s.isSubsetOf([a, a, b]));
  assertFalse('set !<= [a, a, b, b]', s.isSubsetOf([a, a, b, b]));
}

testSuite({
  testGetCount() {
    let s = new StructsSet;
    const a = new String('a');
    s.add(a);
    const b = new String('b');
    s.add(b);
    const c = new String('c');
    s.add(c);
    assertEquals('count, should be 3', s.getCount(), 3);
    const d = new String('d');
    s.add(d);
    assertEquals('count, should be 4', s.getCount(), 4);
    s.remove(d);
    assertEquals('count, should be 3', s.getCount(), 3);

    s = new StructsSet;
    s.add('a');
    s.add('b');
    s.add('c');
    assertEquals('count, should be 3', s.getCount(), 3);
    s.add('d');
    assertEquals('count, should be 4', s.getCount(), 4);
    s.remove('d');
    assertEquals('count, should be 3', s.getCount(), 3);
  },


  testSize() {
    const s = new StructsSet();
    s.add('a');
    s.add('b');
    s.add('c');
    assertEquals('size, should be 3', s.size, 3);
    s.add('d');
    assertEquals('size, should be 4', s.size, 4);
    s.delete('d');
    assertEquals('size, should be 3', s.size, 3);
  },

  testGetValues() {
    const s = new StructsSet;
    const a = new String('a');
    s.add(a);
    const b = new String('b');
    s.add(b);
    const c = new String('c');
    s.add(c);
    const d = new String('d');
    s.add(d);
    assertEquals(s.getValues().join(''), 'abcd');

    const s2 = new StructsSet;
    s2.add('a');
    s2.add('b');
    s2.add('c');
    s2.add('d');
    assertEquals(s2.getValues().join(''), 'abcd');
  },


  testValuesIterator() {
    const s = new StructsSet();
    s.add('a');
    s.add('b');
    s.add('c');
    s.add('d');
    assertEquals(Array.from(s.values()).join(''), 'abcd');
  },

  testContainsHas() {
    let s = new StructsSet;
    const a = new String('a');
    s.add(a);
    const b = new String('b');
    s.add(b);
    const c = new String('c');
    s.add(c);
    const d = new String('d');
    s.add(d);
    const e = new String('e');

    assertTrue('contains, Should contain String(\'a\')', s.contains(a));
    assertTrue('has, Should contain String(\'a\')', s.has(a));
    // Explicitly test that the implementation doesn't treat `new String('a')`
    // and `'a'` the same (e.g. uses === equality for values, not ==).
    assertFalse('contains, Should not contain literal \'a\'', s.contains('a'));
    assertFalse('has, Should not contain literal \'a\'', s.has('a'));
    assertFalse('contains, Should not contain \'e\'', s.contains(e));
    assertFalse('has, Should not contain \'e\'', s.has(e));

    s = new StructsSet;
    s.add('a');
    s.add('b');
    s.add('c');
    s.add('d');

    assertTrue('contains, Should contain \'a\'', s.contains('a'));
    assertTrue('has, Should contain \'a\'', s.has('a'));
    assertFalse('contains, Should not contain \'e\'', s.contains('e'));
    assertFalse('has, Should not contain \'e\'', s.has('e'));
  },

  testContainsFunctionValue() {
    const s = new StructsSet;

    const fn1 = () => {};

    assertFalse(s.contains(fn1));
    assertFalse(s.has(fn1));
    s.add(fn1);
    assertTrue(s.contains(fn1));
    assertTrue(s.has(fn1));

    const fn2 = () => {};

    assertFalse(s.contains(fn2));
    assertFalse(s.has(fn2));
    s.add(fn2);
    assertTrue(s.contains(fn2));
    assertTrue(s.has(fn2));

    assertEquals(s.getCount(), 2);
    assertEquals(s.size, 2);
  },

  testContainsAll() {
    const set = new StructsSet([1, 2, 3]);

    assertTrue('{1, 2, 3} contains []', set.containsAll([]));
    assertTrue('{1, 2, 3} contains [1]', set.containsAll([1]));
    assertTrue('{1, 2, 3} contains [1, 1]', set.containsAll([1, 1]));
    assertTrue('{1, 2, 3} contains [3, 2, 1]', set.containsAll([3, 2, 1]));
    assertFalse('{1, 2, 3} doesn\'t contain [4]', set.containsAll([4]));
    assertFalse('{1, 2, 3} doesn\'t contain [1, 4]', set.containsAll([1, 4]));

    assertTrue('{1, 2, 3} contains {a: 1}', set.containsAll({a: 1}));
    assertFalse('{1, 2, 3} doesn\'t contain {a: 4}', set.containsAll({a: 4}));

    assertTrue('{1, 2, 3} contains {1}', set.containsAll(new StructsSet([1])));
    assertFalse(
        '{1, 2, 3} doesn\'t contain {4}', set.containsAll(new StructsSet([4])));
  },

  testIntersection() {
    const emptySet = new StructsSet;

    assertTrue(
        'intersection of empty set and [] should be empty',
        emptySet.intersection([]).isEmpty());
    assertIntersection(
        'intersection of 2 empty sets should be empty', emptySet,
        new StructsSet(), new StructsSet());

    const abcdSet = new StructsSet();
    abcdSet.add('a');
    abcdSet.add('b');
    abcdSet.add('c');
    abcdSet.add('d');

    assertTrue(
        'intersection of populated set and [] should be empty',
        abcdSet.intersection([]).isEmpty());
    assertIntersection(
        'intersection of populated set and empty set should be empty', abcdSet,
        new StructsSet(), new StructsSet());

    const bcSet = new StructsSet(['b', 'c']);
    assertIntersection(
        'intersection of [a,b,c,d] and [b,c]', abcdSet, bcSet, bcSet);

    const bceSet = new StructsSet(['b', 'c', 'e']);
    assertIntersection(
        'intersection of [a,b,c,d] and [b,c,e]', abcdSet, bceSet, bcSet);
  },

  testDifference() {
    const emptySet = new StructsSet;

    assertTrue(
        'difference of empty set and [] should be empty',
        emptySet.difference([]).isEmpty());
    assertTrue(
        'difference of 2 empty sets should be empty',
        emptySet.difference(new StructsSet()).isEmpty());

    const abcdSet = new StructsSet(['a', 'b', 'c', 'd']);

    assertTrue(
        'difference of populated set and [] should be the populated set',
        abcdSet.difference([]).equals(abcdSet));
    assertTrue(
        'difference of populated set and empty set should be the populated set',
        abcdSet.difference(new StructsSet()).equals(abcdSet));
    assertTrue(
        'difference of two identical sets should be the empty set',
        abcdSet.difference(abcdSet).equals(new StructsSet()));

    const bcSet = new StructsSet(['b', 'c']);
    assertTrue(
        'difference of [a,b,c,d] and [b,c] should be [a,d]',
        abcdSet.difference(bcSet).equals(new StructsSet(['a', 'd'])));
    assertTrue(
        'difference of [b,c] and [a,b,c,d] should be the empty set',
        bcSet.difference(abcdSet).equals(new StructsSet()));

    const xyzSet = new StructsSet(['x', 'y', 'z']);
    assertTrue(
        'difference of [a,b,c,d] and [x,y,z] should be the [a,b,c,d]',
        abcdSet.difference(xyzSet).equals(abcdSet));
  },

  testRemoveAll() {
    assertRemoveAll('removeAll of empty set from empty set', [], [], []);
    assertRemoveAll(
        'removeAll of empty set from populated set', ['a', 'b', 'c', 'd'], [],
        ['a', 'b', 'c', 'd']);
    assertRemoveAll(
        'removeAll of [a,d] from [a,b,c,d]', ['a', 'b', 'c', 'd'], ['a', 'd'],
        ['b', 'c']);
    assertRemoveAll(
        'removeAll of [b,c] from [a,b,c,d]', ['a', 'b', 'c', 'd'], ['b', 'c'],
        ['a', 'd']);
    assertRemoveAll(
        'removeAll of [b,c,e] from [a,b,c,d]', ['a', 'b', 'c', 'd'],
        ['b', 'c', 'e'], ['a', 'd']);
    assertRemoveAll(
        'removeAll of [a,b,c,d] from [a,d]', ['a', 'd'], ['a', 'b', 'c', 'd'],
        []);
    assertRemoveAll(
        'removeAll of [a,b,c,d] from [b,c]', ['b', 'c'], ['a', 'b', 'c', 'd'],
        []);
    assertRemoveAll(
        'removeAll of [a,b,c,d] from [b,c,e]', ['b', 'c', 'e'],
        ['a', 'b', 'c', 'd'], ['e']);
  },

  testAdd() {
    let s = new StructsSet;
    const a = new String('a');
    const b = new String('b');
    s.add(a);
    assertTrue(s.contains(a));
    s.add(b);
    assertTrue(s.contains(b));

    s = new StructsSet;
    s.add('a');
    assertTrue(s.contains('a'));
    s.add('b');
    assertTrue(s.contains('b'));
    s.add(null);
    assertTrue('contains null', s.contains(null));
    assertFalse('does not contain "null"', s.contains('null'));
  },

  testClear() {
    let s = new StructsSet;
    const a = new String('a');
    s.add(a);
    const b = new String('b');
    s.add(b);
    const c = new String('c');
    s.add(c);
    const d = new String('d');
    s.add(d);
    s.clear();
    assertTrue('cleared so it should be empty', s.isEmpty());
    assertEquals('cleared so it should be empty', s.size, 0);
    assertTrue('cleared so it should not contain \'a\' key', !s.contains(a));
    assertTrue('cleared so it should not have \'a\' key', !s.has(a));

    s = new StructsSet;
    s.add('a');
    s.add('b');
    s.add('c');
    s.add('d');
    s.clear();
    assertTrue('cleared so it should be empty', s.isEmpty());
    assertEquals('cleared so it should be empty', s.size, 0);
    assertTrue('cleared so it should not contain \'a\' key', !s.contains('a'));
    assertTrue('cleared so it should not have \'a\' key', !s.has('a'));
  },

  testAddAll() {
    let s = new StructsSet;
    const a = new String('a');
    const b = new String('b');
    const c = new String('c');
    const d = new String('d');
    s.addAll([a, b, c, d]);
    assertTrue('addAll so it should not be empty', !s.isEmpty());
    assertTrue('addAll so it should not be empty', s.size > 0);
    assertTrue('addAll so it should contain \'c\' key', s.contains(c));
    assertTrue('addAll so it should have \'c\' key', s.has(c));

    let s2 = new StructsSet;
    s2.addAll(s);
    assertTrue('addAll so it should not be empty', !s2.isEmpty());
    assertTrue('addAll so it should not be empty', s2.size > 0);
    assertTrue('addAll so it should contain \'c\' key', s2.contains(c));
    assertTrue('addAll so it should has \'c\' key', s2.has(c));

    s = new StructsSet;
    s.addAll(['a', 'b', 'c', 'd']);
    assertTrue('addAll so it should not be empty', !s.isEmpty());
    assertTrue('addAll so it should not be empty', s.size > 0);
    assertTrue('addAll so it should contain \'c\' key', s.contains('c'));
    assertTrue('addAll so it should have \'c\' key', s.has('c'));

    s2 = new StructsSet;
    s2.addAll(s);
    assertTrue('addAll so it should not be empty', !s2.isEmpty());
    assertTrue('addAll so it should not be empty', s2.size > 0);
    assertTrue('addAll so it should contain \'c\' key', s2.contains('c'));
    assertTrue('addAll so it should have \'c\' key', s2.has('c'));
  },

  testConstructor() {
    let s = new StructsSet;
    const a = new String('a');
    s.add(a);
    const b = new String('b');
    s.add(b);
    const c = new String('c');
    s.add(c);
    const d = new String('d');
    s.add(d);
    let s2 = new StructsSet(s);
    assertFalse('constr with Set so it should not be empty', s2.isEmpty());
    assertTrue('constr with Set so it should not be empty', s2.size > 0);
    assertTrue('constr with Set so it should contain c', s2.contains(c));
    assertTrue('constr with Set so it should have c', s2.has(c));

    s = new StructsSet;
    s.add('a');
    s.add('b');
    s.add('c');
    s.add('d');
    s2 = new StructsSet(s);
    assertFalse('constr with Set so it should not be empty', s2.isEmpty());
    assertTrue('constr with Set so it should not be empty', s2.size > 0);
    assertTrue('constr with Set so it should contain c', s2.contains('c'));
    assertTrue('constr with Set so it should have c', s2.has('c'));
  },

  testClone() {
    let s = new StructsSet;
    const a = new String('a');
    s.add(a);
    const b = new String('b');
    s.add(b);
    const c = new String('c');
    s.add(c);
    const d = new String('d');
    s.add(d);

    let s2 = s.clone();
    assertFalse('clone so it should not be empty', s2.isEmpty());
    assertTrue('clone so it should not be empty', s2.size > 0);
    assertTrue('clone so it should contain \'c\' key', s2.contains(c));
    assertTrue('clone so it should have \'c\' key', s2.has(c));

    s = new StructsSet;
    s.add('a');
    s.add('b');
    s.add('c');
    s.add('d');

    s2 = s.clone();
    assertFalse('clone so it should not be empty', s2.isEmpty());
    assertTrue('clone so it should not be empty', s2.size > 0);
    assertTrue('clone so it should contain \'c\' key', s2.contains('c'));
    assertTrue('clone so it should have \'c\' key', s2.has('c'));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testEquals() {
    helperForTestEquals(1, 2, 3, 4);
    helperForTestEquals('a', 'b', 'c', 'd');
    helperForTestEquals(
        new String('a'), new String('b'), new String('c'), new String('d'));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testIsSubsetOf() {
    helperForTestIsSubsetOf(1, 2, 3, 4);
    helperForTestIsSubsetOf('a', 'b', 'c', 'd');
    helperForTestIsSubsetOf(
        new String('a'), new String('b'), new String('c'), new String('d'));
  },

  testForEach() {
    let s = new StructsSet;
    const a = new String('a');
    s.add(a);
    const b = new String('b');
    s.add(b);
    const c = new String('c');
    s.add(c);
    const d = new String('d');
    s.add(d);
    let str = '';
    structs.forEach(s, (val, key, set) => {
      assertUndefined(key);
      assertEquals(s, set);
      str += val;
    });
    assertEquals(str, 'abcd');

    s = new StructsSet;
    s.add('a');
    s.add('b');
    s.add('c');
    s.add('d');
    str = '';
    structs.forEach(s, (val, key, set) => {
      assertUndefined(key);
      assertEquals(s, set);
      str += val;
    });
    assertEquals(str, 'abcd');
  },

  testFilter() {
    let s = new StructsSet;
    const a = new Number(0);
    s.add(a);
    const b = new Number(1);
    s.add(b);
    const c = new Number(2);
    s.add(c);
    const d = new Number(3);
    s.add(d);

    let s2 = structs.filter(s, (val, key, set) => {
      assertUndefined(key);
      assertEquals(s, set);
      return val > 1;
    });
    assertEquals(stringifySet(s2), '23');

    s = new StructsSet;
    s.add(0);
    s.add(1);
    s.add(2);
    s.add(3);

    s2 = structs.filter(s, (val, key, set) => {
      assertUndefined(key);
      assertEquals(s, set);
      return val > 1;
    });
    assertEquals(stringifySet(s2), '23');
  },

  testSome() {
    let s = new StructsSet;
    const a = new Number(0);
    s.add(a);
    let b = new Number(1);
    s.add(b);
    const c = new Number(2);
    s.add(c);
    const d = new Number(3);
    s.add(d);

    b = structs.some(s, (val, key, s2) => {
      assertUndefined(key);
      assertEquals(s, s2);
      return val > 1;
    });
    assertTrue(b);

    b = structs.some(s, (val, key, s2) => {
      assertUndefined(key);
      assertEquals(s, s2);
      return val > 100;
    });
    assertFalse(b);

    s = new StructsSet;
    s.add(0);
    s.add(1);
    s.add(2);
    s.add(3);

    b = structs.some(s, (val, key, s2) => {
      assertUndefined(key);
      assertEquals(s, s2);
      return val > 1;
    });
    assertTrue(b);

    b = structs.some(s, (val, key, s2) => {
      assertUndefined(key);
      assertEquals(s, s2);
      return val > 100;
    });
    assertFalse(b);
  },

  testEvery() {
    let s = new StructsSet;
    const a = new Number(0);
    s.add(a);
    let b = new Number(1);
    s.add(b);
    const c = new Number(2);
    s.add(c);
    const d = new Number(3);
    s.add(d);

    b = structs.every(s, (val, key, s2) => {
      assertUndefined(key);
      assertEquals(s, s2);
      return val >= 0;
    });
    assertTrue(b);

    b = structs.every(s, (val, key, s2) => {
      assertUndefined(key);
      assertEquals(s, s2);
      return val > 1;
    });
    assertFalse(b);

    s = new StructsSet;
    s.add(0);
    s.add(1);
    s.add(2);
    s.add(3);

    b = structs.every(s, (val, key, s2) => {
      assertUndefined(key);
      assertEquals(s, s2);
      return val >= 0;
    });
    assertTrue(b);
    b = structs.every(s, (val, key, s2) => {
      assertUndefined(key);
      assertEquals(s, s2);
      return val > 1;
    });
    assertFalse(b);
  },

  testIterator() {
    const s = new StructsSet;
    s.add(0);
    s.add(1);
    s.add(2);
    s.add(3);
    s.add(4);

    assertEquals('01234', iter.join(s, ''));

    s.remove(1);
    s.remove(3);

    assertEquals('024', iter.join(s, ''));
  },
});
