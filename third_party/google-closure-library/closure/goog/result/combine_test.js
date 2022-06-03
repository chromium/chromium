// Copyright 2012 The Closure Library Authors. All Rights Reserved.
// Use of this source code is governed by the Apache License, Version 2.0.

goog.module('goog.result.combineTest');
goog.setTestOnly();

const MockClock = goog.require('goog.testing.MockClock');
const Timer = goog.require('goog.Timer');
const googArray = goog.require('goog.array');
const googResult = goog.require('goog.result');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

let result1;
let result2;
let result3;
let result4;
let resultCallback;

let combinedResult;
let mockClock;
let successCombinedResult;

function assertSuccessCall(combinedResult, resultCallback) {
  assertEquals(googResult.Result.State.SUCCESS, combinedResult.getState());
  assertEquals(1, resultCallback.getCallCount());

  const result = resultCallback.popLastCall().getArgument(0);
  assertEquals(combinedResult, result);
  assertArgumentContainsGivenResults(result.getValue());
}

function assertErrorCall(combinedResult, resultCallback) {
  assertEquals(googResult.Result.State.ERROR, combinedResult.getState());
  assertEquals(1, resultCallback.getCallCount());

  const result = resultCallback.popLastCall().getArgument(0);
  assertEquals(combinedResult, result);
  assertArgumentContainsGivenResults(combinedResult.getError());
}

function assertArgumentContainsGivenResults(resultsArray) {
  assertEquals(4, resultsArray.length);

  googArray.forEach([result1, result2, result3, result4], (res) => {
    assertTrue(googArray.contains(resultsArray, res));
  });
}

function resolveAllGivenResultsToSuccess() {
  googArray.forEach([result1, result2, result3, result4], (res) => {
    res.setValue(1);
  });
}

function resolveAllGivenResultsToError() {
  googArray.forEach([result1, result2, result3, result4], (res) => {
    res.setError();
  });
}

function resolveSomeGivenResultsToSuccess() {
  googArray.forEach([result2, result3, result4], (res) => {
    res.setValue(1);
  });
  result1.setError();
}
testSuite({
  setUpPage() {
    mockClock = new MockClock();
    mockClock.install();
  },

  tearDownPage() {
    goog.dispose(mockClock);
  },

  setUp() {
    mockClock.reset();
    result1 = new googResult.SimpleResult();
    result2 = new googResult.SimpleResult();
    result3 = new googResult.SimpleResult();
    result4 = new googResult.SimpleResult();

    combinedResult = googResult.combine(result1, result2, result3, result4);

    successCombinedResult =
        googResult.combineOnSuccess(result1, result2, result3, result4);

    resultCallback = recordFunction();
  },

  tearDown() {
    result1 = result2 = result3 = result4 = resultCallback = null;
    combinedResult = successCombinedResult = null;
  },

  testSynchronousCombine() {
    resolveAllGivenResultsToSuccess();

    const newCombinedResult =
        googResult.combine(result1, result2, result3, result4);

    googResult.wait(newCombinedResult, resultCallback);

    assertSuccessCall(newCombinedResult, resultCallback);
  },

  testCombineWhenAllResultsSuccess() {
    googResult.wait(combinedResult, resultCallback);

    resolveAllGivenResultsToSuccess();

    assertSuccessCall(combinedResult, resultCallback);
  },

  testAsyncCombineWhenAllResultsSuccess() {
    googResult.wait(combinedResult, resultCallback);

    Timer.callOnce(() => {
      resolveAllGivenResultsToSuccess();
    });
    mockClock.tick();

    assertSuccessCall(combinedResult, resultCallback);
  },

  testCombineWhenAllResultsFail() {
    googResult.wait(combinedResult, resultCallback);

    resolveAllGivenResultsToError();

    assertSuccessCall(combinedResult, resultCallback);
  },

  testAsyncCombineWhenAllResultsFail() {
    googResult.wait(combinedResult, resultCallback);

    Timer.callOnce(() => {
      resolveAllGivenResultsToError();
    });
    mockClock.tick();

    assertSuccessCall(combinedResult, resultCallback);
  },

  testCombineWhenSomeResultsSuccess() {
    googResult.wait(combinedResult, resultCallback);

    resolveSomeGivenResultsToSuccess();

    assertSuccessCall(combinedResult, resultCallback);
  },

  testAsyncCombineWhenSomeResultsSuccess() {
    googResult.wait(combinedResult, resultCallback);

    Timer.callOnce(() => {
      resolveSomeGivenResultsToSuccess();
    });
    mockClock.tick();

    assertSuccessCall(combinedResult, resultCallback);
  },

  testCombineOnSuccessWhenAllResultsSuccess() {
    googResult.wait(successCombinedResult, resultCallback);

    resolveAllGivenResultsToSuccess();

    assertSuccessCall(successCombinedResult, resultCallback);
  },

  testAsyncCombineOnSuccessWhenAllResultsSuccess() {
    googResult.wait(successCombinedResult, resultCallback);

    Timer.callOnce(() => {
      resolveAllGivenResultsToSuccess();
    });
    mockClock.tick();

    assertSuccessCall(successCombinedResult, resultCallback);
  },

  testCombineOnSuccessWhenAllResultsFail() {
    googResult.wait(successCombinedResult, resultCallback);

    resolveAllGivenResultsToError();

    assertErrorCall(successCombinedResult, resultCallback);
  },

  testAsyncCombineOnSuccessWhenAllResultsFail() {
    googResult.wait(successCombinedResult, resultCallback);

    Timer.callOnce(() => {
      resolveAllGivenResultsToError();
    });
    mockClock.tick();

    assertErrorCall(successCombinedResult, resultCallback);
  },

  testCombineOnSuccessWhenSomeResultsSuccess() {
    googResult.wait(successCombinedResult, resultCallback);

    resolveSomeGivenResultsToSuccess();

    assertErrorCall(successCombinedResult, resultCallback);
  },

  testAsyncCombineOnSuccessWhenSomeResultsSuccess() {
    googResult.wait(successCombinedResult, resultCallback);

    Timer.callOnce(() => {
      resolveSomeGivenResultsToSuccess();
    });
    mockClock.tick();

    assertErrorCall(successCombinedResult, resultCallback);
  },

  testCancelParentResults() {
    googResult.wait(combinedResult, resultCallback);

    googResult.cancelParentResults(combinedResult);

    assertArgumentContainsGivenResults(combinedResult.getValue());
    googArray.forEach([result1, result2, result3, result4], (result) => {
      assertTrue(result.isCanceled());
    });
  },
});
