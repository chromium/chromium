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

goog.provide('goog.storage.mechanism.mechanismSeparationTester');

goog.require('goog.iter.StopIteration');
/** @suppress {extraRequire} */
goog.require('goog.storage.mechanism.mechanismTestDefinition');
goog.require('goog.testing.asserts');

goog.setTestOnly('goog.storage.mechanism.mechanismSeparationTester');


function testSeparateSet() {
  if (!mechanism || !mechanism_separate) {
    return;
  }
  mechanism.set('first', 'one');
  assertNull(mechanism_separate.get('first'));
  assertEquals(0, mechanism_separate.getCount());
  assertEquals(
      goog.iter.StopIteration,
      assertThrows(mechanism_separate.__iterator__().next));
}


function testSeparateSetInverse() {
  if (!mechanism || !mechanism_separate) {
    return;
  }
  mechanism.set('first', 'one');
  mechanism_separate.set('first', 'two');
  assertEquals('one', mechanism.get('first'));
  assertEquals(1, mechanism.getCount());
  var iterator = mechanism.__iterator__();
  assertEquals('one', iterator.next());
  assertEquals(goog.iter.StopIteration, assertThrows(iterator.next));
}


function testSeparateRemove() {
  if (!mechanism || !mechanism_separate) {
    return;
  }
  mechanism.set('first', 'one');
  mechanism_separate.remove('first');
  assertEquals('one', mechanism.get('first'));
  assertEquals(1, mechanism.getCount());
  var iterator = mechanism.__iterator__();
  assertEquals('one', iterator.next());
  assertEquals(goog.iter.StopIteration, assertThrows(iterator.next));
}


function testSeparateClean() {
  if (!mechanism || !mechanism_separate) {
    return;
  }
  mechanism_separate.set('first', 'two');
  mechanism.clear();
  assertEquals('two', mechanism_separate.get('first'));
  assertEquals(1, mechanism_separate.getCount());
  var iterator = mechanism_separate.__iterator__();
  assertEquals('two', iterator.next());
  assertEquals(goog.iter.StopIteration, assertThrows(iterator.next));
}
