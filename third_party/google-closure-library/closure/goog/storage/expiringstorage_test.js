/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.storage.ExpiringStorageTest');
goog.setTestOnly();

const ExpiringStorage = goog.require('goog.storage.ExpiringStorage');
const FakeMechanism = goog.require('goog.testing.storage.FakeMechanism');
const MockClock = goog.require('goog.testing.MockClock');
const storageTester = goog.require('goog.storage.storageTester');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testBasicOperations() {
    const mechanism = new FakeMechanism();
    const storage = new ExpiringStorage(mechanism);
    storageTester.runBasicTests(storage);
  },

  testExpiration() {
    const mechanism = new FakeMechanism();
    const clock = new MockClock(true);
    const storage = new ExpiringStorage(mechanism);

    // No expiration.
    storage.set('first', 'one second', 1000);
    storage.set('second', 'permanent');
    storage.set('third', 'two seconds', 2000);
    storage.set('fourth', 'permanent');
    clock.tick(100);
    assertEquals('one second', storage.get('first'));
    assertEquals('permanent', storage.get('second'));
    assertEquals('two seconds', storage.get('third'));
    assertEquals('permanent', storage.get('fourth'));

    // A key has expired.
    clock.tick(1000);
    assertUndefined(storage.get('first'));
    assertEquals('permanent', storage.get('second'));
    assertEquals('two seconds', storage.get('third'));
    assertEquals('permanent', storage.get('fourth'));
    assertNull(mechanism.get('first'));

    // Add an already expired key.
    storage.set('fourth', 'one second again', 1000);
    assertNull(mechanism.get('fourth'));
    assertUndefined(storage.get('fourth'));

    // Another key has expired.
    clock.tick(1000);
    assertEquals('permanent', storage.get('second'));
    assertUndefined(storage.get('third'));
    assertNull(mechanism.get('third'));

    // Clean up.
    storage.remove('second');
    assertNull(mechanism.get('second'));
    assertUndefined(storage.get('second'));
    clock.uninstall();
  },

  testClockSkew() {
    const mechanism = new FakeMechanism();
    const storage = new ExpiringStorage(mechanism);
    const clock = new MockClock(true);

    // Simulate clock skew.
    clock.tick(100);
    storage.set('first', 'one second', 1000);
    clock.reset();
    assertUndefined(storage.get('first'));
    assertNull(mechanism.get('first'));

    clock.uninstall();
  },
});
