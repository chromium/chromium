// Copyright 2012 The Closure Library Authors. All Rights Reserved.
// Use of this source code is governed by the Apache License, Version 2.0.

goog.module('goog.result.SimpleResultTest');
goog.setTestOnly();

const GoogPromise = goog.require('goog.Promise');
const MockClock = goog.require('goog.testing.MockClock');
const Thenable = goog.require('goog.Thenable');
const Timer = goog.require('goog.Timer');
const googResult = goog.require('goog.result');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

let mockClock;
let result;
let resultCallback;

let resultCallback1;
let resultCallback2;

testSuite({
  setUpPage() {
    mockClock = new MockClock();
    mockClock.install();
  },

  setUp() {
    mockClock.reset();
    resultCallback = new recordFunction();
    resultCallback1 = new recordFunction();
    resultCallback2 = new recordFunction();
    result = new googResult.SimpleResult();
  },

  tearDown() {
    resultCallback = resultCallback1 = resultCallback2 = result = null;
  },

  tearDownPage() {
    mockClock.uninstall();
    goog.dispose(mockClock);
  },

  testHandlersCalledOnSuccess() {
    result.wait(resultCallback1);
    result.wait(resultCallback2);

    assertEquals(googResult.Result.State.PENDING, result.getState());
    assertEquals(0, resultCallback1.getCallCount());
    assertEquals(0, resultCallback2.getCallCount());

    result.setValue(2);

    assertEquals(googResult.Result.State.SUCCESS, result.getState());
    assertEquals(2, result.getValue());
    assertEquals(1, resultCallback1.getCallCount());
    assertEquals(1, resultCallback2.getCallCount());

    const res1 = resultCallback1.popLastCall().getArgument(0);
    assertObjectEquals(result, res1);

    const res2 = resultCallback2.popLastCall().getArgument(0);
    assertObjectEquals(result, res2);
  },

  testCustomHandlerScope() {
    result.wait(resultCallback1);
    const scope = {};
    result.wait(resultCallback2, scope);

    result.setValue(2);

    assertEquals(1, resultCallback1.getCallCount());
    assertEquals(1, resultCallback2.getCallCount());

    const this1 = resultCallback1.popLastCall().getThis();
    assertObjectEquals(goog.global, this1);

    const this2 = resultCallback2.popLastCall().getThis();
    assertObjectEquals(scope, this2);
  },

  testHandlersCalledOnError() {
    result.wait(resultCallback1);
    result.wait(resultCallback2);
    assertEquals(googResult.Result.State.PENDING, result.getState());

    const error = 'Network Error';
    result.setError(error);

    assertEquals(googResult.Result.State.ERROR, result.getState());
    assertEquals(error, result.getError());
    assertEquals(1, resultCallback1.getCallCount());
    assertEquals(1, resultCallback2.getCallCount());

    const res1 = resultCallback1.popLastCall().getArgument(0);
    assertObjectEquals(result, res1);
    const res2 = resultCallback2.popLastCall().getArgument(0);
    assertObjectEquals(result, res2);
  },

  testAttachingHandlerOnSuccessfulResult() {
    result.setValue(2);
    assertEquals(googResult.Result.State.SUCCESS, result.getState());
    assertEquals(2, result.getValue());
    // resultCallback should be called immediately on a resolved Result
    assertEquals(0, resultCallback.getCallCount());

    result.wait(resultCallback);

    assertEquals(1, resultCallback.getCallCount());
    const res = resultCallback.popLastCall().getArgument(0);
    assertEquals(result, res);
  },

  testAttachingHandlerOnErrorResult() {
    const error = {code: -1, errorString: 'Invalid JSON'};
    result.setError(error);
    assertEquals(googResult.Result.State.ERROR, result.getState());
    assertEquals(error, result.getError());
    // resultCallback should be called immediately on a resolved Result
    assertEquals(0, resultCallback.getCallCount());

    result.wait(resultCallback);

    assertEquals(1, resultCallback.getCallCount());
    const res = resultCallback.popLastCall().getArgument(0);
    assertEquals(result, res);
  },

  testExceptionThrownOnMultipleSuccessfulResolutionAttempts() {
    result.setValue(1);
    assertEquals(googResult.Result.State.SUCCESS, result.getState());
    assertEquals(1, result.getValue());

    // Try to set the value again
    const e = assertThrows(goog.bind(result.setValue, result, 3));
    assertTrue(e instanceof googResult.SimpleResult.StateError);
  },

  testExceptionThrownOnMultipleErrorResolutionAttempts() {
    assertEquals(googResult.Result.State.PENDING, result.getState());

    result.setError(5);

    assertEquals(googResult.Result.State.ERROR, result.getState());
    assertEquals(5, result.getError());
    // Try to set error again
    const e = assertThrows(goog.bind(result.setError, result, 4));
    assertTrue(e instanceof googResult.SimpleResult.StateError);
  },

  testExceptionThrownOnSuccessThenErrorResolutionAttempt() {
    assertEquals(googResult.Result.State.PENDING, result.getState());

    result.setValue(1);

    assertEquals(googResult.Result.State.SUCCESS, result.getState());
    assertEquals(1, result.getValue());

    // Try to set error after setting value
    const e = assertThrows(goog.bind(result.setError, result, 3));
    assertTrue(e instanceof googResult.SimpleResult.StateError);
  },

  testExceptionThrownOnErrorThenSuccessResolutionAttempt() {
    assertEquals(googResult.Result.State.PENDING, result.getState());

    const error = 'fail';
    result.setError(error);

    assertEquals(googResult.Result.State.ERROR, result.getState());
    assertEquals(error, result.getError());
    // Try to set value after setting error
    const e = assertThrows(goog.bind(result.setValue, result, 1));
    assertTrue(e instanceof googResult.SimpleResult.StateError);
  },

  testSuccessfulAsyncResolution() {
    result.wait(resultCallback);
    assertEquals(googResult.Result.State.PENDING, result.getState());

    Timer.callOnce(() => {
      result.setValue(1);
    });
    mockClock.tick();

    assertEquals(1, resultCallback.getCallCount());

    const res = resultCallback.popLastCall().getArgument(0);
    assertEquals(googResult.Result.State.SUCCESS, res.getState());
    assertEquals(1, res.getValue());
  },

  testErrorAsyncResolution() {
    result.wait(resultCallback);
    assertEquals(googResult.Result.State.PENDING, result.getState());

    const error = 'Network failure';
    Timer.callOnce(() => {
      result.setError(error);
    });
    mockClock.tick();

    assertEquals(1, resultCallback.getCallCount());
    const res = resultCallback.popLastCall().getArgument(0);
    assertEquals(googResult.Result.State.ERROR, res.getState());
    assertEquals(error, res.getError());
  },

  testCancelStateAndReturn() {
    assertFalse(result.isCanceled());
    const canceled = result.cancel();
    assertTrue(result.isCanceled());
    assertEquals(googResult.Result.State.ERROR, result.getState());
    assertTrue(result.getError() instanceof googResult.Result.CancelError);
    assertTrue(canceled);
  },

  testErrorHandlersFireOnCancel() {
    result.wait(resultCallback);
    result.cancel();

    assertEquals(1, resultCallback.getCallCount());
    const lastCall = resultCallback.popLastCall();
    const res = lastCall.getArgument(0);
    assertEquals(googResult.Result.State.ERROR, res.getState());
    assertTrue(res.getError() instanceof googResult.Result.CancelError);
  },

  testCancelAfterSetValue() {
    // cancel after setValue/setError => no-op
    result.wait(resultCallback);
    result.setValue(1);

    assertEquals(googResult.Result.State.SUCCESS, result.getState());
    assertEquals(1, result.getValue());
    assertEquals(1, resultCallback.getCallCount());

    result.cancel();

    assertEquals(googResult.Result.State.SUCCESS, result.getState());
    assertEquals(1, result.getValue());
    assertEquals(1, resultCallback.getCallCount());
  },

  testSetValueAfterCancel() {
    // setValue/setError after cancel => no-op
    result.wait(resultCallback);

    result.cancel();
    assertTrue(result.isCanceled());
    assertTrue(result.getError() instanceof googResult.Result.CancelError);

    result.setValue(1);
    assertTrue(result.isCanceled());
    assertTrue(result.getError() instanceof googResult.Result.CancelError);

    result.setError(3);
    assertTrue(result.isCanceled());
    assertTrue(result.getError() instanceof googResult.Result.CancelError);
  },

  testFromResolvedPromise() {
    const promise = GoogPromise.resolve('resolved');
    result = googResult.SimpleResult.fromPromise(promise);
    assertEquals(googResult.Result.State.PENDING, result.getState());
    mockClock.tick();
    assertEquals(googResult.Result.State.SUCCESS, result.getState());
    assertEquals('resolved', result.getValue());
    assertEquals(undefined, result.getError());
  },

  testFromRejectedPromise() {
    const promise = GoogPromise.reject('rejected');
    result = googResult.SimpleResult.fromPromise(promise);
    assertEquals(googResult.Result.State.PENDING, result.getState());
    mockClock.tick();
    assertEquals(googResult.Result.State.ERROR, result.getState());
    assertEquals(undefined, result.getValue());
    assertEquals('rejected', result.getError());
  },

  testThen() {
    let value1;
    let value2;

    result.then((val1) => value1 = val1).then((val2) => {
      value2 = val2;
    });
    result.setValue('done');
    assertUndefined(value1);
    assertUndefined(value2);
    mockClock.tick();
    assertEquals('done', value1);
    assertEquals('done', value2);
  },

  testThen_reject() {
    let reason;
    let value;

    result.then(
        (v) => {
          value = v;
        },
        (r) => {
          reason = r;
        });
    result.setError(new Error('oops'));
    assertUndefined(value);
    mockClock.tick();
    assertUndefined(value);
    assertEquals('oops', reason.message);
  },

  testPromiseAll() {
    const promise = GoogPromise.resolve('promise');
    GoogPromise.all([result, promise]).then((values) => {
      assertEquals(2, values.length);
      assertEquals('result', values[0]);
      assertEquals('promise', values[1]);
    });
    result.setValue('result');
    mockClock.tick();
  },

  testResolvingPromiseBlocksResult() {
    let value;
    GoogPromise.resolve('promise').then((value) => {
      result.setValue(value);
    });
    result.wait((r) => {
      value = r.getValue();
    });
    assertUndefined(value);
    mockClock.tick();
    assertEquals('promise', value);
  },

  testRejectingPromiseBlocksResult() {
    let err;
    GoogPromise.reject(new Error('oops'))
        .then(undefined /* opt_onResolved */, (reason) => {
          result.setError(reason);
        });
    result.wait((r) => {
      err = r.getError();
    });
    assertUndefined(err);
    mockClock.tick();
    assertEquals('oops', err.message);
  },

  testPromiseFromCanceledResult() {
    let reason;
    result.cancel();
    result.then(undefined /* opt_onResolved */, (r) => {
      reason = r;
    });
    mockClock.tick();
    assertTrue(reason instanceof GoogPromise.CancellationError);
  },

  testThenableInterface() {
    assertTrue(Thenable.isImplementedBy(result));
  },
});
