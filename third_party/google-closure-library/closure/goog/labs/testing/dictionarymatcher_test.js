/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.labs.testing.dictionaryMatcherTest');
goog.setTestOnly();

/** @suppress {extraRequire} */
const MatcherError = goog.require('goog.labs.testing.MatcherError');
const assertThat = goog.require('goog.labs.testing.assertThat');
/** @suppress {extraRequire} */
const matchers = goog.require('goog.labs.testing');
const testSuite = goog.require('goog.testing.testSuite');

function assertMatcherError(callable, errorString) {
  const e = assertThrows(errorString || 'callable throws exception', callable);
  assertTrue(e instanceof MatcherError);
}
testSuite({
  testHasEntries() {
    const obj1 = {x: 1, y: 2, z: 3};
    assertThat(obj1, hasEntries({x: 1, y: 2}), 'obj1 has entries: {x:1, y:2}');

    assertMatcherError(() => {
      assertThat(obj1, hasEntries({z: 5, a: 4}));
    }, 'hasEntries should throw exception when it fails');
  },

  testHasEntry() {
    const obj1 = {x: 1, y: 2, z: 3};
    assertThat(obj1, hasEntry('x', 1), 'obj1 has entry: {x:1}');

    assertMatcherError(() => {
      assertThat(obj1, hasEntry('z', 5));
    }, 'hasEntry should throw exception when it fails');
  },

  testHasKey() {
    const obj1 = {x: 1};
    assertThat(obj1, hasKey('x'), 'obj1 has key x');

    assertMatcherError(() => {
      assertThat(obj1, hasKey('z'));
    }, 'hasKey should throw exception when it fails');
  },

  testHasValue() {
    const obj1 = {x: 1};
    assertThat(obj1, hasValue(1), 'obj1 has value 1');

    assertMatcherError(() => {
      assertThat(obj1, hasValue(2));
    }, 'hasValue should throw exception when it fails');
  },
});
