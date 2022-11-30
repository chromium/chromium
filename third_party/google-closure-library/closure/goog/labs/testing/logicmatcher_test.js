/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.labs.testing.logicMatcherTest');
goog.setTestOnly();

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
  testAnyOf() {
    assertThat(5, anyOf(greaterThan(4), lessThan(3)), '5 > 4 || 5 < 3');
    assertThat(2, anyOf(greaterThan(4), lessThan(3)), '2 > 4 || 2 < 3');

    assertMatcherError(() => {
      assertThat(4, anyOf(greaterThan(5), lessThan(2)));
    }, 'anyOf should throw exception when it fails');
  },

  testAllOf() {
    assertThat(5, allOf(greaterThan(4), lessThan(6)), '5 > 4 && 5 < 6');

    assertMatcherError(() => {
      assertThat(4, allOf(lessThan(5), lessThan(3)));
    }, 'allOf should throw exception when it fails');
  },

  testIsNot() {
    assertThat(5, isNot(greaterThan(6)), '5 !> 6');

    assertMatcherError(() => {
      assertThat(4, isNot(greaterThan(3)));
    }, 'isNot should throw exception when it fails');
  },
});
