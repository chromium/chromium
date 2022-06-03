// Copyright 2012 The Closure Library Authors. All Rights Reserved.
// Use of this source code is governed by the Apache License, Version 2.0.

goog.module('goog.result.chainTest');
goog.setTestOnly();

const MockClock = goog.require('goog.testing.MockClock');
const Timer = goog.require('goog.Timer');
const googResult = goog.require('goog.result');
const testSuite = goog.require('goog.testing.testSuite');
const testingRecordFunction = goog.require('goog.testing.recordFunction');

let actionCallback;
let counter;
let dependentResult;
let givenResult;

let mockClock;

// SYNCHRONOUS TESTS:

// ASYNCHRONOUS TESTS:

// HELPER FUNCTIONS:

// Assert that the recordFunction was called once with an argument of
// 'result' (the second argument) which has a state of SUCCESS and
// a value of 'value' (the third argument).
function assertSuccess(recordFunction, result, value) {
  assertEquals(1, recordFunction.getCallCount());
  const res = recordFunction.popLastCall().getArgument(0);
  assertEquals(result, res);
  assertEquals(googResult.Result.State.SUCCESS, res.getState());
  assertEquals(value, res.getValue());
}

// Assert that the recordFunction was called once with an argument of
// 'result' (the second argument) which has a state of ERROR.
function assertError(recordFunction, result, value) {
  assertEquals(1, recordFunction.getCallCount());
  const res = recordFunction.popLastCall().getArgument(0);
  assertEquals(result, res);
  assertEquals(googResult.Result.State.ERROR, res.getState());
  assertEquals(value, res.getError());
}

// Assert that the recordFunction wasn't called
function assertNoCall(recordFunction) {
  assertEquals(0, recordFunction.getCallCount());
}
testSuite({
  setUpPage() {
    mockClock = new MockClock();
    mockClock.install();
  },

  setUp() {
    mockClock.reset();
    givenResult = new googResult.SimpleResult();
    dependentResult = new googResult.SimpleResult();
    counter = new testingRecordFunction();
    actionCallback = testingRecordFunction((result) => dependentResult);
  },

  tearDown() {
    givenResult = dependentResult = counter = null;
  },

  tearDownPage() {
    mockClock.uninstall();
  },

  testChainWhenBothResultsSuccess() {
    const finalResult = googResult.chain(givenResult, actionCallback);
    googResult.wait(finalResult, counter);

    givenResult.setValue(1);
    dependentResult.setValue(2);

    assertSuccess(actionCallback, givenResult, 1);
    assertSuccess(counter, finalResult, 2);
  },

  testChainWhenFirstResultError() {
    const finalResult = googResult.chain(givenResult, actionCallback);
    googResult.wait(finalResult, counter);

    givenResult.setError(4);

    assertNoCall(actionCallback);
    assertError(counter, finalResult, 4);
  },

  testChainWhenSecondResultError() {
    const finalResult = googResult.chain(givenResult, actionCallback);
    googResult.wait(finalResult, counter);

    givenResult.setValue(1);
    dependentResult.setError(5);

    assertSuccess(actionCallback, givenResult, 1);
    assertError(counter, finalResult, 5);
  },

  testChainCancelFirstResult() {
    const finalResult = googResult.chain(givenResult, actionCallback);
    googResult.wait(finalResult, counter);

    googResult.cancelParentResults(finalResult);

    assertNoCall(actionCallback);
    assertTrue(givenResult.isCanceled());
    assertTrue(finalResult.isCanceled());
  },

  testChainCancelSecondResult() {
    const finalResult = googResult.chain(givenResult, actionCallback);
    googResult.wait(finalResult, counter);

    givenResult.setValue(1);
    googResult.cancelParentResults(finalResult);

    assertSuccess(actionCallback, givenResult, 1);
    assertTrue(dependentResult.isCanceled());
    assertTrue(finalResult.isCanceled());
  },

  testDoubleChainCancel() {
    const intermediateResult = googResult.chain(givenResult, actionCallback);
    const finalResult = googResult.chain(intermediateResult, actionCallback);

    assertTrue(googResult.cancelParentResults(finalResult));
    assertTrue(finalResult.isCanceled());
    assertTrue(intermediateResult.isCanceled());
    assertFalse(givenResult.isCanceled());
    assertFalse(googResult.cancelParentResults(finalResult));
  },

  testCustomScope() {
    const scope = {};
    const finalResult = googResult.chain(givenResult, actionCallback, scope);
    googResult.wait(finalResult, counter);

    givenResult.setValue(1);
    dependentResult.setValue(2);

    assertEquals(scope, actionCallback.popLastCall().getThis());
  },

  testChainAsyncWhenBothResultsSuccess() {
    const finalResult = googResult.chain(givenResult, actionCallback);
    googResult.wait(finalResult, counter);

    Timer.callOnce(() => {
      givenResult.setValue(1);
    });
    mockClock.tick();

    assertSuccess(actionCallback, givenResult, 1);

    Timer.callOnce(() => {
      dependentResult.setValue(2);
    });
    mockClock.tick();

    assertSuccess(counter, finalResult, 2);
  },

  testChainAsyncWhenFirstResultError() {
    const finalResult = googResult.chain(givenResult, actionCallback);
    googResult.wait(finalResult, counter);

    Timer.callOnce(() => {
      givenResult.setError(6);
    });
    mockClock.tick();

    assertNoCall(actionCallback);
    assertError(counter, finalResult, 6);
  },

  testChainAsyncWhenSecondResultError() {
    const finalResult = googResult.chain(givenResult, actionCallback);
    googResult.wait(finalResult, counter);

    Timer.callOnce(() => {
      givenResult.setValue(1);
    });
    mockClock.tick();

    assertSuccess(actionCallback, givenResult, 1);

    Timer.callOnce(() => {
      dependentResult.setError(7);
    });
    mockClock.tick();

    assertError(counter, finalResult, 7);
  },

  testChainAsyncCancelFirstResult() {
    const finalResult = googResult.chain(givenResult, actionCallback);
    googResult.wait(finalResult, counter);

    Timer.callOnce(() => {
      googResult.cancelParentResults(finalResult);
    });
    mockClock.tick();

    assertNoCall(actionCallback);
    assertTrue(givenResult.isCanceled());
    assertTrue(finalResult.isCanceled());
  },

  testChainAsyncCancelSecondResult() {
    const finalResult = googResult.chain(givenResult, actionCallback);
    googResult.wait(finalResult, counter);

    Timer.callOnce(() => {
      givenResult.setValue(1);
    });
    mockClock.tick();

    assertSuccess(actionCallback, givenResult, 1);

    Timer.callOnce(() => {
      googResult.cancelParentResults(finalResult);
    });
    mockClock.tick();

    assertTrue(dependentResult.isCanceled());
    assertTrue(finalResult.isCanceled());
  },
});
