/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.storage.CollectableStorageTest');
goog.setTestOnly();

const CollectableStorage = goog.require('goog.storage.CollectableStorage');
const FakeMechanism = goog.require('goog.testing.storage.FakeMechanism');
const MockClock = goog.require('goog.testing.MockClock');
const collectableStorageTester = goog.require('goog.storage.collectableStorageTester');
const storageTester = goog.require('goog.storage.storageTester');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testBasicOperations() {
    const mechanism = new FakeMechanism();
    const storage = new CollectableStorage(mechanism);
    storageTester.runBasicTests(storage);
  },

  testExpiredKeyCollection() {
    const mechanism = new FakeMechanism();
    const clock = new MockClock(true);
    const storage = new CollectableStorage(mechanism);

    collectableStorageTester.runBasicTests(mechanism, clock, storage);
  },
});
