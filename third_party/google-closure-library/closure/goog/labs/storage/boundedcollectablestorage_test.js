/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.labs.storage.BoundedCollectableStorageTest');
goog.setTestOnly();

const BoundedCollectableStorage = goog.require('goog.labs.storage.BoundedCollectableStorage');
const FakeMechanism = goog.require('goog.testing.storage.FakeMechanism');
const MockClock = goog.require('goog.testing.MockClock');
const collectableStorageTester = goog.require('goog.storage.collectableStorageTester');
const storageTester = goog.require('goog.storage.storageTester');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testBasicOperations() {
    const mechanism = new FakeMechanism();
    const storage = new BoundedCollectableStorage(mechanism, 5);
    storageTester.runBasicTests(storage);
  },

  testExpiredKeyCollection() {
    const mechanism = new FakeMechanism();
    const clock = new MockClock(true);
    const storage = new BoundedCollectableStorage(mechanism, 15);

    collectableStorageTester.runBasicTests(mechanism, clock, storage);
  },

  testLimitingNumberOfItems() {
    const mechanism = new FakeMechanism();
    const clock = new MockClock(true);
    const storage = new BoundedCollectableStorage(mechanism, 2);

    // First item should fit.
    storage.set('item-1', 'one', 10000);
    clock.tick(100);
    assertEquals('one', storage.get('item-1'));

    // Second item should fit.
    storage.set('item-2', 'two', 10000);
    assertEquals('one', storage.get('item-1'));
    assertEquals('two', storage.get('item-2'));

    // Third item is too much, 'item-1' should be removed.
    storage.set('item-3', 'three', 5000);
    clock.tick(100);
    assertUndefined(storage.get('item-1'));
    assertEquals('two', storage.get('item-2'));
    assertEquals('three', storage.get('item-3'));

    clock.tick(5000);
    // 'item-3' item has expired, should be removed instead an older 'item-2'.
    storage.set('item-4', 'four', 10000);
    assertUndefined(storage.get('item-1'));
    assertUndefined(storage.get('item-3'));
    assertEquals('two', storage.get('item-2'));
    assertEquals('four', storage.get('item-4'));

    storage.remove('item-2');
    storage.remove('item-4');

    clock.uninstall();
  },
});
