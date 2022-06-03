// Copyright 2012 The Closure Library Authors. All Rights Reserved.
// Use of this source code is governed by the Apache License, Version 2.0.

goog.module('goog.result.waitTest');
goog.setTestOnly();

const MockClock = goog.require('goog.testing.MockClock');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const SimpleResult = goog.require('goog.result.SimpleResult');
const Timer = goog.require('goog.Timer');
const googResult = goog.require('goog.result');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

let result;
let waitCallback;
let waitOnErrorCallback;
let waitOnSuccessCallback;

let mockClock;
let propertyReplacer;

/**
 * Assert that a callback function stubbed out with goog.recordFunction was
 * called with the expected arguments by googResult.waitOnSuccess/Error.
 * @param {Function} recordedFunction The callback function.
 * @param {?} value The value stored in the result.
 * @param {!googResult.Result} result The result that was resolved to SUCCESS or
 *     ERROR.
 * @param {Object=} opt_scope Optional scope that the test function should be
 *     called in. By default, it is goog.global.
 */
function assertCall(recordedFunction, value, result, opt_scope) {
  const scope = opt_scope || goog.global;
  assertEquals(1, recordedFunction.getCallCount());
  const call = recordedFunction.popLastCall();
  assertEquals(2, call.getArguments().length);
  assertEquals(value, call.getArgument(0));
  assertEquals(result, call.getArgument(1));
  assertEquals(scope, call.getThis());
}

/**
 * Assert that a callback function stubbed out with goog.recordFunction was
 * called with the expected arguments by googResult.wait.
 * @param {Function} recordedFunction The callback function.
 * @param {!googResult.Result} result The result that was resolved to SUCCESS or
 *     ERROR.
 * @param {Object=} opt_scope Optional scope that the test function should be
 *     called in. By default, it is goog.global.
 */
function assertWaitCall(recordedFunction, result, opt_scope) {
  const scope = opt_scope || goog.global;
  assertEquals(1, recordedFunction.getCallCount());
  const call = recordedFunction.popLastCall();
  assertEquals(1, call.getArguments().length);
  assertEquals(result, call.getArgument(0));
  assertEquals(scope, call.getThis());
}

function assertNoCall(recordedFunction) {
  assertEquals(0, recordedFunction.getCallCount());
}
testSuite({
  setUpPage() {
    mockClock = new MockClock();
    mockClock.install();
  },

  setUp() {
    mockClock.reset();
    result = new SimpleResult();
    propertyReplacer = new PropertyReplacer();
    waitCallback = new recordFunction();
    waitOnSuccessCallback = new recordFunction();
    waitOnErrorCallback = new recordFunction();
  },

  tearDown() {
    result = waitCallback = waitOnSuccessCallback = waitOnErrorCallback = null;
    propertyReplacer.reset();
  },

  tearDownPage() {
    mockClock.uninstall();
  },

  testSynchronousSuccess() {
    assertEquals(googResult.Result.State.PENDING, result.getState());
    assertUndefined(result.getValue());

    googResult.wait(result, waitCallback);
    googResult.waitOnSuccess(result, waitOnSuccessCallback);
    googResult.waitOnError(result, waitOnErrorCallback);

    result.setValue(1);

    assertEquals(googResult.Result.State.SUCCESS, result.getState());
    assertEquals(1, result.getValue());

    assertWaitCall(waitCallback, result);
    assertCall(waitOnSuccessCallback, 1, result);
    assertNoCall(waitOnErrorCallback);
  },

  testAsynchronousSuccess() {
    googResult.wait(result, waitCallback);
    googResult.waitOnSuccess(result, waitOnSuccessCallback);
    googResult.waitOnError(result, waitOnErrorCallback);

    Timer.callOnce(() => {
      result.setValue(1);
    });

    assertUndefined(result.getValue());
    assertEquals(googResult.Result.State.PENDING, result.getState());

    assertNoCall(waitCallback);
    assertNoCall(waitOnSuccessCallback);
    assertNoCall(waitOnErrorCallback);

    mockClock.tick();

    assertEquals(googResult.Result.State.SUCCESS, result.getState());
    assertEquals(1, result.getValue());

    assertWaitCall(waitCallback, result);
    assertCall(waitOnSuccessCallback, 1, result);
    assertNoCall(waitOnErrorCallback);
  },

  testSynchronousError() {
    assertEquals(googResult.Result.State.PENDING, result.getState());
    assertUndefined(result.getValue());

    googResult.wait(result, waitCallback);
    googResult.waitOnSuccess(result, waitOnSuccessCallback);
    googResult.waitOnError(result, waitOnErrorCallback);

    result.setError();

    assertEquals(googResult.Result.State.ERROR, result.getState());
    assertUndefined(result.getValue());

    assertWaitCall(waitCallback, result);
    assertNoCall(waitOnSuccessCallback);
    assertCall(waitOnErrorCallback, undefined, result);
  },

  testAsynchronousError() {
    googResult.wait(result, waitCallback);
    googResult.waitOnSuccess(result, waitOnSuccessCallback);
    googResult.waitOnError(result, waitOnErrorCallback);

    Timer.callOnce(() => {
      result.setError();
    });

    assertEquals(googResult.Result.State.PENDING, result.getState());
    assertUndefined(result.getValue());

    assertNoCall(waitCallback);
    assertNoCall(waitOnSuccessCallback);
    assertNoCall(waitOnErrorCallback);

    mockClock.tick();

    assertEquals(googResult.Result.State.ERROR, result.getState());
    assertUndefined(result.getValue());

    assertWaitCall(waitCallback, result);
    assertNoCall(waitOnSuccessCallback);
    assertCall(waitOnErrorCallback, undefined, result);
  },

  testCustomScope() {
    const scope = {};
    googResult.wait(result, waitCallback, scope);
    result.setValue(1);
    assertEquals(scope, waitCallback.popLastCall().getThis());
  },

  testDefaultScope() {
    googResult.wait(result, waitCallback);
    result.setValue(1);
    assertEquals(goog.global, waitCallback.popLastCall().getThis());
  },

  testOnSuccessScope() {
    const scope = {};
    googResult.waitOnSuccess(result, waitOnSuccessCallback, scope);
    result.setValue(1);
    assertCall(waitOnSuccessCallback, 1, result, scope);
  },

  testOnErrorScope() {
    const scope = {};
    googResult.waitOnError(result, waitOnErrorCallback, scope);
    result.setError();
    assertCall(waitOnErrorCallback, undefined, result, scope);
  },
});
