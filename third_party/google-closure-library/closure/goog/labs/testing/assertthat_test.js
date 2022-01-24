/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.labs.testing.assertThatTest');
goog.setTestOnly();

const MatcherError = goog.require('goog.labs.testing.MatcherError');
const assertThat = goog.require('goog.labs.testing.assertThat');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

let describeFn;
let failureMatchesFn;
let successMatchesFn;
let successTestMatcher;

let failureTestMatcher;

testSuite({
  setUp() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    successMatchesFn = new recordFunction(() => true);
    /** @suppress {checkTypes} suppression added to enable type checking */
    failureMatchesFn = new recordFunction(() => false);
    /** @suppress {checkTypes} suppression added to enable type checking */
    describeFn = new recordFunction();

    successTestMatcher = () =>
        ({matches: successMatchesFn, describe: describeFn});
    failureTestMatcher = () =>
        ({matches: failureMatchesFn, describe: describeFn});
  },

  testAssertthatAlwaysCallsMatches() {
    const value = 7;
    assertThat(value, successTestMatcher(), 'matches is called on success');

    assertEquals(1, successMatchesFn.getCallCount());
    const matchesCall = successMatchesFn.popLastCall();
    assertEquals(value, matchesCall.getArgument(0));

    const e = assertThrows(goog.bind(
        assertThat, null, value, failureTestMatcher(),
        'matches is called on failure'));

    assertTrue(e instanceof MatcherError);

    assertEquals(1, failureMatchesFn.getCallCount());
  },

  testAssertthatCallsDescribeOnFailure() {
    const value = 7;
    const e = assertThrows(goog.bind(
        assertThat, null, value, failureTestMatcher(),
        'describe is called on failure'));

    assertTrue(e instanceof MatcherError);

    assertEquals(1, failureMatchesFn.getCallCount());
    assertEquals(1, describeFn.getCallCount());

    const matchesCall = describeFn.popLastCall();
    assertEquals(value, matchesCall.getArgument(0));
  },
});
