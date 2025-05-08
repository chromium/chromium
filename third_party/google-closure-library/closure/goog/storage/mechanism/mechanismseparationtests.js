/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Unit tests for storage mechanism separation.
 *
 * These tests should be included by tests of any mechanism which natively
 * implements namespaces. There is no need to include those tests for mechanisms
 * extending goog.storage.mechanism.PrefixedMechanism. Make sure a different
 * namespace is used for each object.
 */

goog.module('goog.storage.mechanism.mechanismSeparationTests');
goog.setTestOnly('goog.storage.mechanism.mechanismSeparationTests');

const IterableMechanism = goog.require('goog.storage.mechanism.IterableMechanism');
const {assertEquals, assertNull, assertTrue} = goog.require('goog.testing.asserts');
const {bindTests} = goog.require('goog.storage.mechanism.testhelpers');


/**
 * @param {!IterableMechanism} mechanism
 * @param {!IterableMechanism} mechanismSeparate
 */
function testSeparateSet(mechanism, mechanismSeparate) {
  mechanism.set('first', 'one');
  assertNull(mechanismSeparate.get('first'));
  assertEquals(0, mechanismSeparate.getCount());
  const it = mechanismSeparate[Symbol.iterator]().next();
  assertTrue(it.done);
  assertEquals(it.value, undefined);
}

/**
 * @param {!IterableMechanism} mechanism
 * @param {!IterableMechanism} mechanismSeparate
 */
function testSeparateSetInverse(mechanism, mechanismSeparate) {
  mechanism.set('first', 'one');
  mechanismSeparate.set('first', 'two');
  assertEquals('one', mechanism.get('first'));
  assertEquals(1, mechanism.getCount());
  const iterator = mechanismSeparate[Symbol.iterator]();
  let it = iterator.next();
  assertFalse(it.done);
  assertEquals(mechanism.get(it.value), 'one');
  it = iterator.next();
  assertTrue(it.done);
  assertEquals(it.value, undefined);
}

/**
 * @param {!IterableMechanism} mechanism
 * @param {!IterableMechanism} mechanismSeparate
 */
function testSeparateRemove(mechanism, mechanismSeparate) {
  mechanism.set('first', 'one');
  mechanismSeparate.remove('first');
  assertEquals('one', mechanism.get('first'));
  assertEquals(1, mechanism.getCount());
  const iterator = mechanism[Symbol.iterator]();
  let it = iterator.next();
  assertFalse(it.done);
  assertEquals(mechanism.get(it.value), 'one');
  it = iterator.next();
  assertTrue(it.done);
  assertEquals(it.value, undefined);
}

/**
 * @param {!IterableMechanism} mechanism
 * @param {!IterableMechanism} mechanismSeparate
 */
function testSeparateClean(mechanism, mechanismSeparate) {
  mechanismSeparate.set('first', 'two');
  mechanism.clear();
  assertEquals('two', mechanismSeparate.get('first'));
  assertEquals(1, mechanismSeparate.getCount());
  const iterator = mechanismSeparate[Symbol.iterator]();
  let it = iterator.next();
  assertFalse(it.done);
  assertEquals(mechanismSeparate.get(it.value), 'two');
  it = iterator.next();
  assertTrue(it.done);
  assertEquals(it.value, undefined);
}

/**
 * @param {{
 *   getMechanism: function(): !IterableMechanism,
 *   getMechanismSeparate: function(): !IterableMechanism,
 * }} state
 * @return {!Object}
 */
exports.register = function(state) {
  return bindTests(
      [
        testSeparateSet,
        testSeparateSetInverse,
        testSeparateRemove,
        testSeparateClean,
      ],
      (testCase) => {
        testCase(state.getMechanism(), state.getMechanismSeparate());
      });
};
