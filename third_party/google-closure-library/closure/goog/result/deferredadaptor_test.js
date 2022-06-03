// Copyright 2012 The Closure Library Authors. All Rights Reserved.
// Use of this source code is governed by the Apache License, Version 2.0.

goog.module('goog.result.DeferredAdaptorTest');
goog.setTestOnly();

const Deferred = goog.require('goog.async.Deferred');
const DeferredAdaptor = goog.require('goog.result.DeferredAdaptor');
const SimpleResult = goog.require('goog.result.SimpleResult');
const googResult = goog.require('goog.result');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

let deferred;
let record;
let result;

testSuite({
  setUp() {
    result = new SimpleResult();
    deferred = new DeferredAdaptor(result);
    record = new recordFunction();
  },

  tearDown() {
    result = deferred = record = null;
  },

  testResultSuccessfulResolution() {
    deferred.addCallback(record);
    result.setValue(1);
    assertEquals(1, record.getCallCount());
    const call = record.popLastCall();
    assertEquals(1, call.getArgument(0));
  },

  testResultErrorResolution() {
    deferred.addErrback(record);
    result.setError(2);
    assertEquals(1, record.getCallCount());
    const call = record.popLastCall();
    assertEquals(2, call.getArgument(0));
  },

  testResultCancelResolution() {
    deferred.addCallback(record);
    const cancelCallback = new recordFunction();
    deferred.addErrback(cancelCallback);
    result.cancel();
    assertEquals(0, record.getCallCount());
    assertEquals(1, cancelCallback.getCallCount());
    const call = cancelCallback.popLastCall();
    assertTrue(call.getArgument(0) instanceof Deferred.CanceledError);
  },

  testAddCallbackOnResolvedResult() {
    result.setValue(1);
    assertEquals(1, result.getValue());
    deferred.addCallback(record);

    // callback should be called immediately when result is already resolved.
    assertEquals(1, record.getCallCount());
    assertEquals(1, record.popLastCall().getArgument(0));
  },

  testAddErrbackOnErroredResult() {
    result.setError(1);
    assertEquals(1, result.getError());

    // errback should be called immediately when result already errored.
    deferred.addErrback(record);
    assertEquals(1, record.getCallCount());
    assertEquals(1, record.popLastCall().getArgument(0));
  },
});
