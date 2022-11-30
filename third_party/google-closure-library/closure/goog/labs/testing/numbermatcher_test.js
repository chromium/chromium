/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.labs.testing.numberMatcherTest');
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
  /** @suppress {checkTypes} suppression added to enable type checking */
  testAnyNumber() {
    assertThat(0, anyNumber(3), 'typeof 0 == "number"');
    assertMatcherError(() => {
      assertThat('chicken', anyNumber());
    }, 'typeof "chicken" == "number"');
  },

  testGreaterThan() {
    assertThat(4, greaterThan(3), '4 > 3');
    assertMatcherError(() => {
      assertThat(2, greaterThan(3));
    }, '2 > 3');
  },

  testGreaterThanEqualTo() {
    assertThat(5, greaterThanEqualTo(4), '5 >= 4');
    assertThat(5, greaterThanEqualTo(5), '5 >= 5');
    assertMatcherError(() => {
      assertThat(3, greaterThanEqualTo(5));
    }, '3 >= 5');
  },

  testLessThan() {
    assertThat(6, lessThan(7), '6 < 7');
    assertMatcherError(() => {
      assertThat(7, lessThan(5));
    }, '7 < 5');
  },

  testLessThanEqualTo() {
    assertThat(8, lessThanEqualTo(8), '8 <= 8');
    assertThat(8, lessThanEqualTo(9), '8 <= 9');
    assertMatcherError(() => {
      assertThat(7, lessThanEqualTo(5));
    }, '7 <= 5');
  },

  testEqualTo() {
    assertThat(7, equalTo(7), '7 equals 7');
    assertMatcherError(() => {
      assertThat(7, equalTo(5));
    }, '7 == 5');
  },

  testCloseTo() {
    assertThat(7, closeTo(10, 4), '7 within range(4) of 10');
    assertMatcherError(() => {
      assertThat(5, closeTo(10, 3));
    }, '5 within range(3) of 10');
  },
});
