/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Unit tests for the storage interface. */

goog.module('goog.storage.storage_test');
goog.setTestOnly();

const ErrorCode = goog.require('goog.storage.ErrorCode');
const FakeMechanism = goog.require('goog.testing.storage.FakeMechanism');
const StorageStorage = goog.require('goog.storage.Storage');
const asserts = goog.require('goog.testing.asserts');
const functions = goog.require('goog.functions');
const storageTester = goog.require('goog.storage.storageTester');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testBasicOperations() {
    const mechanism = new FakeMechanism();
    const storage = new StorageStorage(mechanism);
    storageTester.runBasicTests(storage);
  },

  testMechanismCommunication() {
    const mechanism = new FakeMechanism();
    const storage = new StorageStorage(mechanism);

    // Invalid JSON.
    mechanism.set('first', '');
    assertEquals(ErrorCode.INVALID_VALUE, assertThrows(() => {
                   storage.get('first');
                 }));
    mechanism.set('second', '(');
    assertEquals(ErrorCode.INVALID_VALUE, assertThrows(() => {
                   storage.get('second');
                 }));

    // Cleaning up.
    storage.remove('first');
    storage.remove('second');
    assertUndefined(storage.get('first'));
    assertUndefined(storage.get('second'));
    assertNull(mechanism.get('first'));
    assertNull(mechanism.get('second'));
  },

  testMechanismFailsGracefullyOnInvalidValue() {
    const mechanism = {
      get: functions.error('Invalid value'),
    };
    /** @suppress {checkTypes} suppression added to enable type checking */
    const storage = new StorageStorage(mechanism);
    assertUndefined(storage.get('foobar'));
  },
});
