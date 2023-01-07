/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.AsyncTestCaseTest');
goog.setTestOnly();

const AsyncTestCase = goog.require('goog.testing.AsyncTestCase');
const DebugError = goog.require('goog.debug.Error');
const asserts = goog.require('goog.testing.asserts');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testControlBreakingExceptionThrown() {
    const asyncTestCase = new AsyncTestCase();

    // doAsyncError with no message.
    try {
      asyncTestCase.doAsyncError();
    } catch (e) {
      assertTrue(e.isControlBreakingException);
      assertEquals('', e.message);
    }

    // doAsyncError with string.
    const errorMessage1 = 'Error message 1';
    try {
      asyncTestCase.doAsyncError(errorMessage1);
    } catch (e) {
      assertTrue(e.isControlBreakingException);
      assertEquals(errorMessage1, e.message);
    }

    // doAsyncError with error.
    const errorMessage2 = 'Error message 2';
    try {
      const error = new DebugError(errorMessage2);
      asyncTestCase.doAsyncError(error);
    } catch (e) {
      assertTrue(e.isControlBreakingException);
      assertEquals(errorMessage2, e.message);
    }
  },

  testMaybeFailTestEarly() {
    const message = 'Error in setUpPage().';
    const asyncTestCase = new AsyncTestCase();
    asyncTestCase.setUpPage = () => {
      throw new Error(message);
    };
    asyncTestCase.addNewTest('test', () => {
      assertTrue(true);
    });
    asyncTestCase.runTests();
    window.setTimeout(() => {
      assertFalse(asyncTestCase.isSuccess());
      const errors = asyncTestCase.getResult().errors;
      assertEquals(1, errors.length);
      assertEquals(message, errors[0].message);
    }, asyncTestCase.stepTimeout * 2);
  },
});
