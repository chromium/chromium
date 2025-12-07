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

goog.module('goog.storage.mechanism.mechanismSharingTests');
goog.setTestOnly('goog.storage.mechanism.mechanismSharingTests');

const IterableMechanism = goog.require('goog.storage.mechanism.IterableMechanism');
const {assertEquals, assertNull, assertTrue} = goog.require('goog.testing.asserts');
const {bindTests} = goog.require('goog.storage.mechanism.testhelpers');


/**
 * @param {{
 *  getMechanism: function(): !IterableMechanism,
 *  getMechanismShared: function(): !IterableMechanism,
 * }} state
 * @return {!Object}
 */
exports.register = function(state) {
  return bindTests(
      [
        testSharedSet,
        testSharedSetInverse,
        testSharedRemove,
        testSharedClean,
      ],
      (testCase) => {
        testCase(state.getMechanism(), state.getMechanismShared());
      });
};


/**
 * @param {!IterableMechanism} mechanism
 * @param {!IterableMechanism} mechanismShared
 */
function testSharedSet(mechanism, mechanismShared) {
  mechanism.set('first', 'one');
  assertEquals('one', mechanismShared.get('first'));
  assertEquals(1, mechanismShared.getCount());
  const iterator = mechanismShared[Symbol.iterator]();
  let it = iterator.next();
  assertFalse(it.done);
  assertEquals(mechanism.get(it.value), 'one');
  it = iterator.next();
  assertTrue(it.done);
  assertEquals(it.value, undefined);
}


/**
 * @param {!IterableMechanism} mechanism
 * @param {!IterableMechanism} mechanismShared
 */
function testSharedSetInverse(mechanism, mechanismShared) {
  mechanismShared.set('first', 'two');
  assertEquals('two', mechanism.get('first'));
  assertEquals(1, mechanism.getCount());
  const iterator = mechanism[Symbol.iterator]();
  let it = iterator.next();
  assertFalse(it.done);
  assertEquals(mechanism.get(it.value), 'two');
  it = iterator.next();
  assertTrue(it.done);
  assertEquals(it.value, undefined);
}


/**
 * @param {!IterableMechanism} mechanism
 * @param {!IterableMechanism} mechanismShared
 */
function testSharedRemove(mechanism, mechanismShared) {
  mechanismShared.set('first', 'three');
  mechanism.remove('first');
  assertNull(mechanismShared.get('first'));
  assertEquals(0, mechanismShared.getCount());
  const iterator = mechanismShared[Symbol.iterator]();
  const it = iterator.next();
  assertTrue(it.done);
  assertEquals(it.value, undefined);
}


/**
 * @param {!IterableMechanism} mechanism
 * @param {!IterableMechanism} mechanismShared
 */
function testSharedClean(mechanism, mechanismShared) {
  mechanism.set('first', 'four');
  mechanismShared.clear();
  assertEquals(0, mechanism.getCount());
  const iterator = mechanism[Symbol.iterator]();
  let it = iterator.next();
  assertTrue(it.done);
  assertEquals(it.value, undefined);
}
