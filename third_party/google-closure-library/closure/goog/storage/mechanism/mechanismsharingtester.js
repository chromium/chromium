/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Unit tests for storage mechanism sharing.
 *
 * These tests should be included in tests of any storage mechanism in which
 * separate mechanism instances share the same underlying storage. Most (if
 * not all) storage mechanisms should have this property. If the mechanism
 * employs namespaces, make sure the same namespace is used for both objects.
 */

goog.provide('goog.storage.mechanism.mechanismSharingTester');

goog.require('goog.iter.StopIteration');
/** @suppress {extraRequire} */
goog.require('goog.storage.mechanism.mechanismTestDefinition');
goog.require('goog.testing.asserts');


goog.setTestOnly('goog.storage.mechanism.mechanismSharingTester');

function testSharedSet() {
  if (!mechanism || !mechanism_shared) {
    return;
  }
  mechanism.set('first', 'one');
  assertEquals('one', mechanism_shared.get('first'));
  assertEquals(1, mechanism_shared.getCount());
  var iterator = mechanism_shared.__iterator__();
  assertEquals('one', iterator.next());
  assertEquals(goog.iter.StopIteration, assertThrows(iterator.next));
}


function testSharedSetInverse() {
  if (!mechanism || !mechanism_shared) {
    return;
  }
  mechanism_shared.set('first', 'two');
  assertEquals('two', mechanism.get('first'));
  assertEquals(1, mechanism.getCount());
  var iterator = mechanism.__iterator__();
  assertEquals('two', iterator.next());
  assertEquals(goog.iter.StopIteration, assertThrows(iterator.next));
}


function testSharedRemove() {
  if (!mechanism || !mechanism_shared) {
    return;
  }
  mechanism_shared.set('first', 'three');
  mechanism.remove('first');
  assertNull(mechanism_shared.get('first'));
  assertEquals(0, mechanism_shared.getCount());
  assertEquals(
      goog.iter.StopIteration,
      assertThrows(mechanism_shared.__iterator__().next));
}


function testSharedClean() {
  if (!mechanism || !mechanism_shared) {
    return;
  }
  mechanism.set('first', 'four');
  mechanism_shared.clear();
  assertEquals(0, mechanism.getCount());
  assertEquals(
      goog.iter.StopIteration, assertThrows(mechanism.__iterator__().next));
}
