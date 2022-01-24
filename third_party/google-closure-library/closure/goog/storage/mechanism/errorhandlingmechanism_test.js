/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.storage.mechanism.ErrorHandlingMechanismTest');
goog.setTestOnly();

const ErrorHandlingMechanism = goog.require('goog.storage.mechanism.ErrorHandlingMechanism');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

const error = new Error();

const submechanism = {
  get: function() {
    throw error;
  },
  set: function() {
    throw error;
  },
  remove: function() {
    throw error;
  },
};

const handler = recordFunction(goog.nullFunction);
let mechanism;

testSuite({
  setUp() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    mechanism = new ErrorHandlingMechanism(submechanism, handler);
  },

  tearDown() {
    handler.reset();
  },

  testSet() {
    mechanism.set('foo', 'bar');
    assertEquals(1, handler.getCallCount());
    assertArrayEquals(
        [
          error,
          ErrorHandlingMechanism.Operation.SET,
          'foo',
          'bar',
        ],
        handler.getLastCall().getArguments());
  },

  testGet() {
    mechanism.get('foo');
    assertEquals(1, handler.getCallCount());
    assertArrayEquals(
        [
          error,
          ErrorHandlingMechanism.Operation.GET,
          'foo',
        ],
        handler.getLastCall().getArguments());
  },

  testRemove() {
    mechanism.remove('foo');
    assertEquals(1, handler.getCallCount());
    assertArrayEquals(
        [
          error,
          ErrorHandlingMechanism.Operation.REMOVE,
          'foo',
        ],
        handler.getLastCall().getArguments());
  },
});
