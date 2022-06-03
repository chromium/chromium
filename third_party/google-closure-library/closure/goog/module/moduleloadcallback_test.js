/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.module.ModuleLoadCallbackTest');
goog.setTestOnly();

const ErrorHandler = goog.require('goog.debug.ErrorHandler');
const ModuleLoadCallback = goog.require('goog.module.ModuleLoadCallback');
const entryPointRegistry = goog.require('goog.debug.entryPointRegistry');
const functions = goog.require('goog.functions');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testProtectEntryPoint() {
    // Test a callback created before the protect method is called.
    const callback1 = new ModuleLoadCallback(functions.error('callback1'));

    const errorFn = recordFunction();
    const errorHandler = new ErrorHandler(errorFn);
    entryPointRegistry.monitorAll(errorHandler);

    assertEquals(0, errorFn.getCallCount());
    assertThrows(goog.bind(callback1.execute, callback1));
    assertEquals(1, errorFn.getCallCount());
    assertContains(
        'callback1', errorFn.getLastCall().getArguments()[0].message);

    // Test a callback created after the protect method is called.
    const callback2 = new ModuleLoadCallback(functions.error('callback2'));
    assertThrows(goog.bind(callback1.execute, callback2));
    assertEquals(2, errorFn.getCallCount());
    assertContains(
        'callback2', errorFn.getLastCall().getArguments()[0].message);
  },
});
