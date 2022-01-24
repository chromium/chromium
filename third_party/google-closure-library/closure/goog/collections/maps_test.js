/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Tests for collections.maps. These tests try to ensure that for
 * all known common MapLike implementations they correctly implement the
 * necessary public API for use with these functions.
 */
goog.module('goog.collections.mapsTest');
goog.setTestOnly('goog.collections.mapsTest');

const StructsMap = goog.require('goog.structs.Map');
const googIter = goog.require('goog.iter');
const maps = goog.require('goog.collections.maps');
const testSuite = goog.require('goog.testing.testSuite');


/**
 * @typedef {function(new:maps.MapLike)}
 */
let MapLikeCtor;

/**
 * The list of well-known MapLike constructors whose implementations should be
 * equivalent under test.
 * @const {!Array<!MapLikeCtor>}
 */
const knownMapLikeImpls = [StructsMap, Map];

/**
 * For a given test implementation (testImpl), this function will call the test
 * implementation once for each well-known MapLike implementation.
 * @param {function(!MapLikeCtor)} testImpl
 */
function testAllMapLikeImpls(testImpl) {
  for (const mapLikeImpl of knownMapLikeImpls) {
    testImpl(mapLikeImpl);
  }
}

/**
 * For a given test implementation, this function calls the test implementation
 * once for every permutation (order-matters) of 2 of the well-known test
 * implementations. These tests are generally to ensure interoperability (e.g.
 * when constructing a new Map from the contents of an existing Map).
 * @param {function(!MapLikeCtor, !MapLikeCtor)} testImpl
 */
function testTwoMapLikeImplInterop(testImpl) {
  googIter.forEach(googIter.permutations(knownMapLikeImpls, 2), (p) => {
    testImpl(p[0], p[1]);
  });
}

/**
 * Creates a test map populated with basic data.
 * @param {!MapLikeCtor} mapLikeCtor
 * @return {!maps.MapLike<string, number>}
 */
function createTestMap(mapLikeCtor) {
  const m = new mapLikeCtor();
  m.set('a', 0);
  m.set('b', 1);
  m.set('c', 2);
  m.set('d', 3);
  return m;
}

testSuite({

  testSetAll() {
    testAllMapLikeImpls((mapLikeCtor) => {
      const m = new mapLikeCtor();
      maps.setAll(m, Object.entries({a: 0, b: 1, c: 2, d: 3}));
      assertTrue('addAll so it should not be empty', m.size > 0);
      assertTrue('addAll so it should have \'c\' key', m.has('c'));
    });

    testAllMapLikeImpls((mapLikeCtor) => {
      const m = /** @type {!Map<string,number>} */ (createTestMap(Map));
      const m2 = new mapLikeCtor();
      maps.setAll(m2, m.entries());
      assertTrue('addAll so it should not be empty', m2.size > 0);
      assertTrue('addAll so it should have \'c\' key', m2.has('c'));
    });
  },

  /** @suppress {checkTypes} */
  testHasValue() {
    testAllMapLikeImpls((mapLikeCtor) => {
      const m = createTestMap(mapLikeCtor);
      assertTrue(maps.hasValue(m, 3));
      assertFalse(maps.hasValue(m, 4));

      // Emulate a lack of type-checking to ensure that objects are being
      // compared using === when no comparison function is provided.
      assertFalse(maps.hasValue(m, '3'));
    });
  },

  testHasValueWithCustomEquality() {
    testAllMapLikeImpls((mapLikeCtor) => {
      const m = createTestMap(mapLikeCtor);

      const equalsFn = (a, b) => a == b;
      assertTrue(maps.hasValue(m, '3', equalsFn));
      assertFalse(maps.hasValue(m, '4', equalsFn));
    });
  },

  testTranspose() {
    testAllMapLikeImpls((mapLikeCtor) => {
      const m = new mapLikeCtor();
      m.set('a', 1);
      m.set('b', 2);
      m.set('c', 3);
      m.set('d', 4);
      m.set('e', 5);

      const transposed = maps.transpose(m);
      assertEquals(
          'Should contain the keys from the original map as values', 'abcde',
          Array.from(transposed.values()).join(''));
      assertEquals(
          'Should contain the values from the original map as keys', '12345',
          Array.from(transposed.keys()).join(''));
    });
  },

  testToObject() {
    testAllMapLikeImpls((mapLikeCtor) => {
      Object.prototype.b = 0;
      try {
        const m = new mapLikeCtor();
        m.set('a', 0);
        const obj = maps.toObject(m);
        assertTrue(
            'object representation has key "a"', obj.hasOwnProperty('a'));
        assertFalse(
            'object representation does not have key "b"',
            obj.hasOwnProperty('b'));
        assertEquals('value for key "a"', 0, obj['a']);
      } finally {
        delete Object.prototype.b;
      }
    });
  },

  testEqualsWithSameObject() {
    testAllMapLikeImpls((mapLikeCtor) => {
      const map1 = createTestMap(mapLikeCtor);
      assertTrue('maps are the same object', maps.equals(map1, map1));
    });
  },

  testEqualsWithDifferentSizeMaps() {
    testTwoMapLikeImplInterop((mapACtor, mapBCtor) => {
      const map1 = createTestMap(mapACtor);
      const map2 = new mapBCtor();

      assertFalse('maps are different sizes', maps.equals(map1, map2));
    });
  },

  /** @suppress {checkTypes} */
  testEqualsWithDefaultEqualityFn() {
    testTwoMapLikeImplInterop((mapACtor, mapBCtor) => {
      let map1 = new mapACtor();
      let map2 = new mapBCtor();

      assertTrue('maps are both empty', maps.equals(map1, map2));

      map1 = createTestMap(mapACtor);
      map2 = createTestMap(mapBCtor);
      assertTrue('maps are the same', maps.equals(map1, map2));

      // Emulate a lack of type-checking to ensure that objects are being
      // compared using === when no comparison function is provided.
      map2.set('d', '3');
      assertFalse('maps have 3 and \'3\'', maps.equals(map1, map2));
    });
  },

  testEqualsWithCustomEqualityFn() {
    testTwoMapLikeImplInterop((mapACtor, mapBCtor) => {
      const map1 = new mapACtor();
      const map2 = new mapBCtor();

      map1.set('a', 0);
      map1.set('b', 1);

      map2.set('a', '0');
      map2.set('b', '1');

      const equalsFn = (a, b) => a == b;

      assertTrue('maps are equal with ==', maps.equals(map1, map2, equalsFn));
    });
  },
});
