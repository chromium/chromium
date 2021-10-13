/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Unit tests for the iterable storage mechanism interface.
 *
 * These tests should be included in tests of any class extending
 * goog.storage.mechanism.IterableMechanism.
 */

goog.module('goog.storage.mechanism.iterableMechanismTester');
goog.setTestOnly('goog.storage.mechanism.iterableMechanismTester');

const IterableMechanism = goog.require('goog.storage.mechanism.IterableMechanism');
const StopIteration = goog.require('goog.iter.StopIteration');
const googIter = goog.require('goog.iter');


/**
 * @param {?IterableMechanism} mechanism
 */
exports.testCount = function(mechanism) {
  if (!mechanism) {
    fail('Mechanism undefined for testCount');
  }
  assertEquals(0, mechanism.getCount());
  mechanism.set('first', 'one');
  assertEquals(1, mechanism.getCount());
  mechanism.set('second', 'two');
  assertEquals(2, mechanism.getCount());
  mechanism.set('first', 'three');
  assertEquals(2, mechanism.getCount());
};


/**
 * @param {?IterableMechanism} mechanism
 */
exports.testIteratorBasics = function(mechanism) {
  if (!mechanism) {
    fail('Mechanism undefined for testIteratorBasics');
  }
  mechanism.set('first', 'one');
  assertEquals('first', mechanism.__iterator__(true).nextValueOrThrow());
  assertEquals('one', mechanism.__iterator__(false).nextValueOrThrow());
  // ES6 Iteration should only return keys
  assertSameElements(['first'], Array.from(mechanism));
  const iterator = mechanism.__iterator__();
  assertEquals('one', iterator.nextValueOrThrow());
  assertEquals(StopIteration, assertThrows(iterator.nextValueOrThrow));

  const es6Iterator = mechanism[Symbol.iterator]();
  assertObjectEquals({value: 'first', done: false}, es6Iterator.next());
  assertObjectEquals({value: undefined, done: true}, es6Iterator.next());
};


/**
 * @param {?IterableMechanism} mechanism
 */
exports.testIteratorWithTwoValues = function(mechanism) {
  if (!mechanism) {
    fail('Mechanism undefined for testIteratorWithTwoValues');
  }
  mechanism.set('first', 'one');
  mechanism.set('second', 'two');
  assertSameElements(['one', 'two'], googIter.toArray(mechanism));
  // ES6 Iteration should only return keys
  assertSameElements(['first', 'second'], Array.from(mechanism));
  assertSameElements(
      ['first', 'second'], googIter.toArray(mechanism.__iterator__(true)));
};


/**
 * @param {?IterableMechanism} mechanism
 */
exports.testClear = function(mechanism) {
  if (!mechanism) {
    fail('Mechanism undefined for testClear');
  }
  mechanism.set('first', 'one');
  mechanism.set('second', 'two');
  mechanism.clear();
  assertNull(mechanism.get('first'));
  assertNull(mechanism.get('second'));
  assertEquals(0, mechanism.getCount());
  assertEquals(
      StopIteration,
      assertThrows(mechanism.__iterator__(true).nextValueOrThrow));
  assertEquals(
      StopIteration,
      assertThrows(mechanism.__iterator__(false).nextValueOrThrow));
};


/**
 * @param {?IterableMechanism} mechanism
 */
exports.testClearClear = function(mechanism) {
  if (!mechanism) {
    fail('Mechanism undefined for testClearClear');
  }
  mechanism.clear();
  mechanism.clear();
  assertEquals(0, mechanism.getCount());
};


/**
 * @param {?IterableMechanism} mechanism
 */
exports.testIteratorWithWeirdKeys = function(mechanism) {
  if (!mechanism) {
    fail('Mechanism undefined for testIteratorWithWeirdKeys');
  }
  mechanism.set(' ', 'space');
  mechanism.set('=+!@#$%^&*()-_\\|;:\'",./<>?[]{}~`', 'control');
  mechanism.set(
      '\u4e00\u4e8c\u4e09\u56db\u4e94\u516d\u4e03\u516b\u4e5d\u5341', 'ten');
  assertEquals(3, mechanism.getCount());
  assertSameElements(
      [
        ' ', '=+!@#$%^&*()-_\\|;:\'",./<>?[]{}~`',
        '\u4e00\u4e8c\u4e09\u56db\u4e94\u516d\u4e03\u516b\u4e5d\u5341'
      ],
      googIter.toArray(mechanism.__iterator__(true)));
  mechanism.clear();
  assertEquals(0, mechanism.getCount());
};
