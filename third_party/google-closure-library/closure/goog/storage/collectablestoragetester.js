/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Unit tests for the collectable storage interface.
 */

goog.provide('goog.storage.collectableStorageTester');
goog.setTestOnly();

goog.require('goog.testing.asserts');
goog.requireType('goog.storage.CollectableStorage');
goog.requireType('goog.storage.mechanism.IterableMechanism');
goog.requireType('goog.testing.MockClock');



/**
 * Tests basic operation: expiration and collection of collectable storage.
 *
 * @param {goog.storage.mechanism.IterableMechanism} mechanism
 * @param {goog.testing.MockClock} clock
 * @param {goog.storage.CollectableStorage} storage
  */
goog.storage.collectableStorageTester.runBasicTests = function(
    mechanism, clock, storage) {
  'use strict';
  // No expiration.
  storage.set('first', 'three seconds', 3000);
  storage.set('second', 'one second', 1000);
  storage.set('third', 'permanent');
  storage.set('fourth', 'two seconds', 2000);
  clock.tick(100);
  storage.collect();
  assertEquals('three seconds', storage.get('first'));
  assertEquals('one second', storage.get('second'));
  assertEquals('permanent', storage.get('third'));
  assertEquals('two seconds', storage.get('fourth'));

  // A key has expired.
  clock.tick(1000);
  storage.collect();
  assertNull(mechanism.get('second'));
  assertEquals('three seconds', storage.get('first'));
  assertUndefined(storage.get('second'));
  assertEquals('permanent', storage.get('third'));
  assertEquals('two seconds', storage.get('fourth'));

  // Another two keys have expired.
  clock.tick(2000);
  storage.collect();
  assertNull(mechanism.get('first'));
  assertNull(mechanism.get('fourth'));
  assertUndefined(storage.get('first'));
  assertEquals('permanent', storage.get('third'));
  assertUndefined(storage.get('fourth'));

  // Clean up.
  storage.remove('third');
  assertNull(mechanism.get('third'));
  assertUndefined(storage.get('third'));
  storage.collect();
  clock.uninstall();
};
