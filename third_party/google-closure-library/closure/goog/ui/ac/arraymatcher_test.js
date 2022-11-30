/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.ac.ArrayMatcherTest');
goog.setTestOnly();

const ArrayMatcher = goog.require('goog.ui.ac.ArrayMatcher');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testRequestingRows() {
    const items = ['a', 'Ab', 'abc', 'ba', 'ca'];
    const am = new ArrayMatcher(items, true);

    let res;
    function matcher(token, matches) {
      assertEquals('a', token);
      res = matches;
      assertEquals('Should have three matches', 3, matches.length);
      assertEquals('a', matches[0]);
      assertEquals('Ab', matches[1]);
      assertEquals('abc', matches[2]);
    }

    am.requestMatchingRows('a', 10, matcher);
    const res2 = ArrayMatcher.getMatchesForRows('a', 10, items);
    assertArrayEquals(res, res2);
  },

  testRequestingRowsMaxMatches() {
    const items = ['a', 'Ab', 'abc', 'ba', 'ca'];
    const am = new ArrayMatcher(items, true);

    function matcher(token, matches) {
      assertEquals('a', token);
      assertEquals('Should have two matches', 2, matches.length);
      assertEquals('a', matches[0]);
      assertEquals('Ab', matches[1]);
    }

    am.requestMatchingRows('a', 2, matcher);
  },

  testRequestingRowsSimilarMatches() {
    // No prefix matches so use similar
    const items = ['b', 'c', 'ba', 'ca'];
    const am = new ArrayMatcher(items, false);

    function matcher(token, matches) {
      assertEquals('a', token);
      assertEquals('Should have two matches', 2, matches.length);
      assertEquals('ba', matches[0]);
      assertEquals('ca', matches[1]);
    }

    am.requestMatchingRows('a', 10, matcher);
  },

  testRequestingRowsSimilarMatchesMaxMatches() {
    // No prefix matches so use similar
    const items = ['b', 'c', 'ba', 'ca'];
    const am = new ArrayMatcher(items, false);

    function matcher(token, matches) {
      assertEquals('a', token);
      assertEquals('Should have one match', 1, matches.length);
      assertEquals('ba', matches[0]);
    }

    am.requestMatchingRows('a', 1, matcher);
  },

  testGetPrefixMatches() {
    const items = ['a', 'b', 'c'];
    const am = new ArrayMatcher(items, true);

    const res = am.getPrefixMatches('a', 10);
    assertEquals('Should have one match', 1, res.length);
    assertEquals('Should return \'a\'', 'a', res[0]);
    const res2 = ArrayMatcher.getPrefixMatchesForRows('a', 10, items);
    assertArrayEquals(res, res2);
  },

  testGetPrefixMatchesMaxMatches() {
    const items = ['a', 'Ab', 'abc', 'ba', 'ca'];
    const am = new ArrayMatcher(items, true);

    const res = am.getPrefixMatches('a', 2);
    assertEquals('Should have two matches', 2, res.length);
    assertEquals('a', res[0]);
  },

  testGetPrefixMatchesEmptyToken() {
    const items = ['a', 'b', 'c'];
    const am = new ArrayMatcher(items, true);

    const res = am.getPrefixMatches('', 10);
    assertEquals('Should have no matches', 0, res.length);
  },

  testGetSimilarRowsSimple() {
    const items = ['xa', 'xb', 'xc'];
    const am = new ArrayMatcher(items, true);

    const res = am.getSimilarRows('a', 10);
    assertEquals('Should have one match', 1, res.length);
    assertEquals('xa', res[0]);
    const res2 = ArrayMatcher.getSimilarMatchesForRows('a', 10, items);
    assertArrayEquals(res, res2);
  },

  testGetSimilarRowsMaxMatches() {
    const items = ['xa', 'xAa', 'xaAa'];
    const am = new ArrayMatcher(items, true);

    const res = am.getSimilarRows('a', 2);
    assertEquals('Should have two matches', 2, res.length);
    assertEquals('xa', res[0]);
    assertEquals('xAa', res[1]);
  },

  testGetSimilarRowsTermDistance() {
    const items = ['surgeon', 'pleasantly', 'closely', 'ba'];
    const am = new ArrayMatcher(items, true);

    const res = am.getSimilarRows('urgently', 4);
    assertEquals('Should have one match', 1, res.length);
    assertEquals('surgeon', res[0]);

    const res2 = ArrayMatcher.getSimilarMatchesForRows('urgently', 4, items);
    assertArrayEquals(res, res2);
  },

  testGetSimilarRowsContainedTerms() {
    const items = ['application', 'apple', 'happy'];
    const am = new ArrayMatcher(items, true);
    const res = am.getSimilarRows('app', 4);
    assertEquals('Should have three matches', 3, res.length);
    assertEquals('application', res[0]);
    assertEquals('apple', res[1]);
    assertEquals('happy', res[2]);

    const res2 = ArrayMatcher.getSimilarMatchesForRows('app', 4, items);
    assertArrayEquals(res, res2);
  },
});
