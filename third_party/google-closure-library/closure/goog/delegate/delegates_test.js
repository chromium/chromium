/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.delegate.delegatesTest');
goog.setTestOnly();

const delegates = goog.require('goog.delegate.delegates');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({

  shouldRunTests() {
    return typeof Array.prototype.map == 'function';
  },

  testFunctionsActuallyCalled() {
    const funcs = [
      recordFunction(),
      recordFunction(() => ''),
      recordFunction(() => 42),
      recordFunction(),
    ];
    const assertCallCounts = (...counts) => {
      assertArrayEquals(counts, funcs.map(f => f.getCallCount()));
      funcs.forEach(f => f.reset());
    };

    assertUndefined(delegates.callFirst(funcs, f => f()));
    assertCallCounts(1, 0, 0, 0);
    assertEquals('', delegates.callUntilDefinedAndNotNull(funcs, f => f()));
    assertCallCounts(1, 1, 0, 0);
    assertEquals(42, delegates.callUntilTruthy(funcs, f => f()));
    assertCallCounts(1, 1, 1, 0);
  },

  testResultNeverDefined() {
    const funcs = [
      recordFunction(),
      recordFunction(),
    ];
    const assertCallCounts = (...counts) => {
      assertArrayEquals(counts, funcs.map(f => f.getCallCount()));
      funcs.forEach(f => f.reset());
    };

    assertUndefined(delegates.callFirst(funcs, f => f()));
    assertCallCounts(1, 0);
    assertUndefined(delegates.callUntilDefinedAndNotNull(funcs, f => f()));
    assertCallCounts(1, 1);
    assertFalse(delegates.callUntilTruthy(funcs, f => f()));
    assertCallCounts(1, 1);
  },
});
