// Copyright 2012 The Closure Library Authors. All Rights Reserved.
// Use of this source code is governed by the Apache License, Version 2.0.

goog.module('goog.result.transformTest');
goog.setTestOnly();

const MockClock = goog.require('goog.testing.MockClock');
const SimpleResult = goog.require('goog.result.SimpleResult');
const Timer = goog.require('goog.Timer');
const googResult = goog.require('goog.result');
const testSuite = goog.require('goog.testing.testSuite');
const testingRecordFunction = goog.require('goog.testing.recordFunction');

let mockClock;
let multiplyResult;
let result;
let resultCallback;

function assertSuccessCall(recordFunction, result, value) {
  assertEquals(1, recordFunction.getCallCount());

  const res = recordFunction.popLastCall().getArgument(0);
  assertEquals(result, res);
  assertEquals(googResult.Result.State.SUCCESS, res.getState());
  assertEquals(value, res.getValue());
}

function assertErrorCall(recordFunction, result, value) {
  assertEquals(1, recordFunction.getCallCount());

  const res = recordFunction.popLastCall().getArgument(0);
  assertEquals(result, res);
  assertEquals(googResult.Result.State.ERROR, res.getState());
  assertEquals(value, res.getError());
}

function assertNoCall(recordFunction) {
  assertEquals(0, recordFunction.getCallCount());
}

function assertTransformerCall(recordFunction, value) {
  assertEquals(1, recordFunction.getCallCount());

  const argValue = recordFunction.popLastCall().getArgument(0);
  assertEquals(value, argValue);
}
testSuite({
  setUpPage() {
    mockClock = new MockClock();
    mockClock.install();
  },

  setUp() {
    mockClock.reset();
    result = new SimpleResult();
    resultCallback = new testingRecordFunction();
    multiplyResult = testingRecordFunction((value) => value * 2);
  },

  tearDown() {
    result = multiplyResult = null;
  },

  tearDownPage() {
    mockClock.uninstall();
    goog.dispose(mockClock);
  },

  testTransformWhenResultSuccess() {
    const transformedResult = googResult.transform(result, multiplyResult);
    googResult.wait(transformedResult, resultCallback);

    assertEquals(googResult.Result.State.PENDING, result.getState());
    result.setValue(1);
    assertTransformerCall(multiplyResult, 1);
    assertSuccessCall(resultCallback, transformedResult, 2);
  },

  testTransformWhenResultSuccessAsync() {
    const transformedResult = googResult.transform(result, multiplyResult);
    googResult.wait(transformedResult, resultCallback);

    Timer.callOnce(() => {
      result.setValue(1);
    });

    assertEquals(googResult.Result.State.PENDING, result.getState());
    mockClock.tick();
    assertTransformerCall(multiplyResult, 1);
    assertSuccessCall(resultCallback, transformedResult, 2);
  },

  testTransformWhenResultError() {
    const transformedResult = googResult.transform(result, multiplyResult);
    googResult.wait(transformedResult, resultCallback);

    assertEquals(googResult.Result.State.PENDING, result.getState());
    result.setError(4);
    assertNoCall(multiplyResult);
    assertErrorCall(resultCallback, transformedResult, 4);
  },

  testTransformWhenResultErrorAsync() {
    const transformedResult = googResult.transform(result, multiplyResult);

    googResult.wait(transformedResult, resultCallback);

    Timer.callOnce(() => {
      result.setError(5);
    });

    assertEquals(googResult.Result.State.PENDING, result.getState());
    mockClock.tick();
    assertNoCall(multiplyResult);
    assertErrorCall(resultCallback, transformedResult, 5);
  },

  testCancelParentResults() {
    const transformedResult = googResult.transform(result, multiplyResult);
    googResult.wait(transformedResult, resultCallback);

    googResult.cancelParentResults(transformedResult);

    assertTrue(result.isCanceled());
    result.setValue(1);
    assertNoCall(multiplyResult);
  },

  testDoubleTransformCancel() {
    const step1Result = googResult.transform(result, multiplyResult);
    const step2Result = googResult.transform(step1Result, multiplyResult);

    googResult.cancelParentResults(step2Result);

    assertFalse(result.isCanceled());
    assertTrue(step1Result.isCanceled());
    assertTrue(step2Result.isCanceled());
  },
});
