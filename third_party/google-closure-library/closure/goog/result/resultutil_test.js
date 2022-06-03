// Copyright 2012 The Closure Library Authors. All Rights Reserved.
// Use of this source code is governed by the Apache License, Version 2.0.

goog.module('goog.resultTest');
goog.setTestOnly();

const googResult = goog.require('goog.result');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testSuccessfulResult() {
    const value = 'some-value';
    const result = googResult.successfulResult(value);
    assertEquals(googResult.Result.State.SUCCESS, result.getState());
    assertEquals(value, result.getValue());
  },

  testFailedResult() {
    const error = new Error('something-failed');
    const result = googResult.failedResult(error);
    assertEquals(googResult.Result.State.ERROR, result.getState());
    assertEquals(error, result.getError());
  },

  testCanceledResult() {
    const result = googResult.canceledResult();
    assertEquals(googResult.Result.State.ERROR, result.getState());

    const error = result.getError();
    assertTrue(error instanceof googResult.Result.CancelError);
  },
});
