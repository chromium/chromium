/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Unit tests for goog.collections.set.
 */

goog.module('goog.collections.setsTest');
goog.setTestOnly('goog.collections.setsTest');

const StructsSet = goog.require('goog.structs.Set');
const googIter = goog.require('goog.iter');
const sets = goog.require('goog.collections.sets');
const testSuite = goog.require('goog.testing.testSuite');


/**
 * @typedef {function(new:sets.SetLike,(!Iterable<?>|!Array<?>)=)}
 */
let SetLikeCtor;

/**
 * The list of well-known SetLike constructors whose implementations should be
 * equivalent under test.
 * @const {!Array<!SetLikeCtor>}
 */
const knownSetLikeImpls = [StructsSet, Set];

/**
 * For a given test implementation, this function calls the test implementation
 * once for every permutation (order-matters) of 2 of the well-known test
 * implementations. These tests are generally to ensure interoperability (e.g.
 * when constructing a new Set from the contents of an existing Set).
 * @param {function(!SetLikeCtor, !SetLikeCtor)} testImpl
 */
function testTwoSetLikeImplInterop(testImpl) {
  googIter.forEach(googIter.permutations(knownSetLikeImpls, 2), (p) => {
    testImpl(p[0], p[1]);
  });
}

/** Yield of the given arguments in order. */
function* yieldArguments(...args) {
  for (const arg of args) {
    yield arg;
  }
}

/** Produce an empty generator. */
function* emptyGenerator() {}

testSuite({
  testIntersection() {
    testTwoSetLikeImplInterop((setACtor, setBCtor) => {
      // arrays
      assertSameElements([2], sets.intersection(new setACtor([1, 2]), [2, 3]));
      assertSameElements([], sets.intersection(new setACtor([]), []));
      assertSameElements(
          [1], sets.intersection(new setACtor([1, 1, 1]), [1, 1]));

      // generators
      assertSameElements(
          [2], sets.intersection(new setACtor([1, 2]), yieldArguments(2, 3)));
      assertSameElements(
          [], sets.intersection(new setACtor([]), emptyGenerator()));
      assertSameElements(
          [1],
          sets.intersection(new setACtor([1, 1, 1]), yieldArguments(1, 1)));

      // sets
      assertSameElements(
          [2], sets.intersection(new setACtor([1, 2]), new setBCtor([2, 3])));
      assertSameElements(
          [], sets.intersection(new setACtor([]), new setBCtor()));
      assertSameElements(
          [1],
          sets.intersection(new setACtor([1, 1, 1]), new setBCtor([1, 1])));
    });
  },

  testUnion() {
    testTwoSetLikeImplInterop((setACtor, setBCtor) => {
      // arrays
      assertSameElements([1, 2, 3], sets.union(new setACtor([1, 2]), [2, 3]));
      assertSameElements([], sets.union(new setACtor([]), []));
      assertSameElements([1], sets.union(new setACtor([1, 1, 1]), [1, 1]));

      // generators
      assertSameElements(
          [1, 2, 3], sets.union(new setACtor([1, 2]), yieldArguments(2, 3)));
      assertSameElements([], sets.union(new setACtor([]), emptyGenerator()));
      assertSameElements(
          [1], sets.union(new setACtor([1, 1, 1]), yieldArguments(1, 1)));

      // sets
      assertSameElements(
          [1, 2, 3], sets.union(new setACtor([1, 2]), new setBCtor([2, 3])));
      assertSameElements([], sets.union(new setACtor([]), new setBCtor()));
      assertSameElements(
          [1], sets.union(new setACtor([1, 1, 1]), new setBCtor([1, 1])));
    });
  },

  testDifference() {
    testTwoSetLikeImplInterop((setACtor, setBCtor) => {
      // arrays
      assertSameElements([1], sets.difference(new setACtor([1, 2]), [2, 3]));
      assertSameElements([], sets.difference(new setACtor([]), []));
      assertSameElements([], sets.difference(new setACtor([1, 1, 1]), [1, 1]));

      // generators
      assertSameElements(
          [1], sets.difference(new setACtor([1, 2]), yieldArguments(2, 3)));
      assertSameElements(
          [], sets.difference(new setACtor([]), emptyGenerator()));
      assertSameElements(
          [], sets.difference(new setACtor([1, 1, 1]), yieldArguments(1, 1)));

      // sets
      assertSameElements(
          [1], sets.difference(new setACtor([1, 2]), new setBCtor([2, 3])));
      assertSameElements([], sets.difference(new setACtor([]), new setBCtor()));
      assertSameElements(
          [], sets.difference(new setACtor([1, 1, 1]), new setBCtor([1, 1])));
    });
  },

  testSymmetricDifference() {
    // sets (only sets are accepted for symmetricDifference)
    assertSameElements(
        [1, 3], sets.symmetricDifference(new Set([1, 2]), new Set([2, 3])));
    assertSameElements([], sets.symmetricDifference(new Set([]), new Set()));
    assertSameElements(
        [], sets.symmetricDifference(new Set([1, 1, 1]), new Set([1, 1])));
  },

  testAddAll() {
    testTwoSetLikeImplInterop((setACtor, setBCtor) => {
      const s = new setACtor();
      sets.addAll(s, ['a', 'b', 'c', 'd']);
      assertTrue('addAll so it should not be empty', s.size > 0);
      assertTrue('addAll so it should have \'c\' key', s.has('c'));

      const s2 = new setBCtor();
      sets.addAll(s2, s);
      assertTrue('addAll so it should not be empty', s2.size > 0);
      assertTrue('addAll so it should has \'c\' key', s2.has('c'));
    });
  },

  testRemoveAll() {
    const testRemoveAllImpl = function(
        msg, elements1, elements2, expectedResult) {
      testTwoSetLikeImplInterop((setACtor, setBCtor) => {
        const set1 = new setACtor(elements1);
        const set2 = new setBCtor(elements2);
        sets.removeAll(set1, set2);

        assertTrue(
            `${msg}: set1 count increased after removeAll`,
            elements1.length >= set1.size);
        assertEquals(
            `${msg}: set2 count changed after removeAll`, elements2.length,
            set2.size);
        assertTrue(
            `${msg}: wrong set1 after removeAll`,
            sets.equals(set1, expectedResult));
        assertTrue(
            `${msg}: non-empty intersection after removeAll: set1->set2`,
            sets.equals(sets.intersection(set1, set2), []));
        assertTrue(
            `${msg}: non-empty intersection after removeAll: set2->set1`,
            sets.equals(sets.intersection(set2, set1), []));
      });
    };
    testRemoveAllImpl('removeAll of empty set from empty set', [], [], []);
    testRemoveAllImpl(
        'removeAll of empty set from populated set', ['a', 'b', 'c', 'd'], [],
        ['a', 'b', 'c', 'd']);
    testRemoveAllImpl(
        'removeAll of [a,d] from [a,b,c,d]', ['a', 'b', 'c', 'd'], ['a', 'd'],
        ['b', 'c']);
    testRemoveAllImpl(
        'removeAll of [b,c] from [a,b,c,d]', ['a', 'b', 'c', 'd'], ['b', 'c'],
        ['a', 'd']);
    testRemoveAllImpl(
        'removeAll of [b,c,e] from [a,b,c,d]', ['a', 'b', 'c', 'd'],
        ['b', 'c', 'e'], ['a', 'd']);
    testRemoveAllImpl(
        'removeAll of [a,b,c,d] from [a,d]', ['a', 'd'], ['a', 'b', 'c', 'd'],
        []);
    testRemoveAllImpl(
        'removeAll of [a,b,c,d] from [b,c]', ['b', 'c'], ['a', 'b', 'c', 'd'],
        []);
    testRemoveAllImpl(
        'removeAll of [a,b,c,d] from [b,c,e]', ['b', 'c', 'e'],
        ['a', 'b', 'c', 'd'], ['e']);
  },

  testHasAll() {
    testTwoSetLikeImplInterop((setACtor, setBCtor) => {
      const s = new setACtor([1, 2, 3]);

      assertTrue('{1, 2, 3} contains []', sets.hasAll(s, []));
      assertTrue('{1, 2, 3} contains [1]', sets.hasAll(s, [1]));
      assertTrue('{1, 2, 3} contains [1, 1]', sets.hasAll(s, [1, 1]));
      assertTrue('{1, 2, 3} contains [3, 2, 1]', sets.hasAll(s, [3, 2, 1]));
      assertFalse('{1, 2, 3} doesn\'t contain [4]', sets.hasAll(s, [4]));
      assertFalse('{1, 2, 3} doesn\'t contain [1, 4]', sets.hasAll(s, [1, 4]));

      assertTrue(
          '{1, 2, 3} contains {a: 1}', sets.hasAll(s, Object.values({a: 1})));
      assertFalse(
          '{1, 2, 3} doesn\'t contain {a: 4}',
          sets.hasAll(s, Object.values({a: 4})));

      assertTrue('{1, 2, 3} contains {1}', sets.hasAll(s, new setBCtor([1])));
      assertFalse(
          '{1, 2, 3} doesn\'t contain {4}', sets.hasAll(s, new setBCtor([4])));
    });
  },

  testEquals() {
    /**
     * Helper method for testEquals().
     * @param {?} a First element to use in the tests.
     * @param {?} b Second element to use in the tests.
     * @param {?} c Third element to use in the tests.
     * @param {?} d Fourth element to use in the tests.
     */
    const testEqualsImpl = function(a, b, c, d) {
      testTwoSetLikeImplInterop((setACtor, setBCtor) => {
        const s = new setACtor([a, b, c]);

        assertTrue('set == itself', sets.equals(s, s));
        assertTrue('set == same set', sets.equals(s, new setBCtor([a, b, c])));
        assertTrue('set == its clone', sets.equals(s, new setBCtor(s)));
        assertTrue('set == array of same elements', sets.equals(s, [a, b, c]));
        assertTrue(
            'set == array of same elements in different order',
            sets.equals(s, [c, b, a]));

        assertFalse('set != empty set', sets.equals(s, new setBCtor));
        assertFalse('set != its subset', sets.equals(s, new setBCtor([a, c])));
        assertFalse(
            'set != its superset', sets.equals(s, new setBCtor([a, b, c, d])));
        assertFalse(
            'set != different set', sets.equals(s, new setBCtor([b, c, d])));
        assertFalse('set != its subset as array', sets.equals(s, [a, c]));
        assertFalse(
            'set != its superset as array', sets.equals(s, [a, b, c, d]));
        assertFalse('set != different set as array', sets.equals(s, [b, c, d]));
        assertFalse('set != [a, b, c, c]', sets.equals(s, [a, b, c, c]));
        assertFalse('set != [a, b, b]', sets.equals(s, [a, b, b]));
        assertFalse('set != [a, a]', sets.equals(s, [a, a]));
      });
    };
    testEqualsImpl(1, 2, 3, 4);
    testEqualsImpl('a', 'b', 'c', 'd');
  },

  testIsSubsetOf() {
    /**
     * Helper method for testIsSubsetOf().
     * @param {?} a First element to use in the tests.
     * @param {?} b Second element to use in the tests.
     * @param {?} c Third element to use in the tests.
     * @param {?} d Fourth element to use in the tests.
     */
    const testSubsetOfImpl = function(a, b, c, d) {
      testTwoSetLikeImplInterop((setACtor, setBCtor) => {
        const s = new setACtor([a, b, c]);

        assertTrue('set <= itself', sets.isSubsetOf(s, s));
        assertTrue(
            'set <= same set', sets.isSubsetOf(s, new setBCtor([a, b, c])));
        assertTrue('set <= its clone', sets.isSubsetOf(s, new setBCtor(s)));
        assertTrue(
            'set <= array of same elements', sets.isSubsetOf(s, [a, b, c]));
        assertTrue(
            'set <= array of same elements in different order',
            sets.equals(s, [c, b, a]));

        assertTrue(
            'set <= Set([a, b, c, d])',
            sets.isSubsetOf(s, new setBCtor([a, b, c, d])));
        assertTrue('set <= [a, b, c, d]', sets.isSubsetOf(s, [a, b, c, d]));
        assertTrue('set <= [a, b, c, c]', sets.isSubsetOf(s, [a, b, c, c]));

        assertFalse(
            'set !<= Set([a, b])', sets.isSubsetOf(s, new setBCtor([a, b])));
        assertFalse('set !<= [a, b]', sets.isSubsetOf(s, [a, b]));
        assertFalse(
            'set !<= Set([c, d])', sets.isSubsetOf(s, new setBCtor([c, d])));
        assertFalse('set !<= [c, d]', sets.isSubsetOf(s, [c, d]));
        assertFalse(
            'set !<= Set([a, c, d])',
            sets.isSubsetOf(s, new setBCtor([a, c, d])));
        assertFalse('set !<= [a, c, d]', sets.isSubsetOf(s, [a, c, d]));
        assertFalse('set !<= [a, a, b]', sets.isSubsetOf(s, [a, a, b]));
        assertFalse('set !<= [a, a, b, b]', sets.isSubsetOf(s, [a, a, b, b]));
      });
    };
    testSubsetOfImpl(1, 2, 3, 4);
    testSubsetOfImpl('a', 'b', 'c', 'd');
  },
});
