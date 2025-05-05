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

goog.module('goog.storage.mechanism.iterableMechanismTests');
goog.setTestOnly('goog.storage.mechanism.iterableMechanismTests');

const IterableMechanism = goog.require('goog.storage.mechanism.IterableMechanism');
const googIter = goog.require('goog.iter');
const {assertEquals, assertNull, assertObjectEquals, assertSameElements, fail} = goog.require('goog.testing.asserts');
const {bindTests} = goog.require('goog.storage.mechanism.testhelpers');


/**
 * @param {?IterableMechanism} mechanism
 */
function testCount(mechanism) {
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
}


/**
 * @param {?IterableMechanism} mechanism
 */
function testIteratorBasics(mechanism) {
  if (!mechanism) {
    fail('Mechanism undefined for testIteratorBasics');
  }
  mechanism.set('first', 'one');
  // ES6 Iteration should only return keys
  assertSameElements(['first'], Array.from(mechanism));

  const es6Iterator = mechanism[Symbol.iterator]();
  const it = es6Iterator.next();
  assertObjectEquals({value: 'first', done: false}, it);
  assertEquals(mechanism.get(it.value), 'one');
  assertObjectEquals({value: undefined, done: true}, es6Iterator.next());

  for (const key of mechanism) {
    // Cast the key to the expected type to ensure that the types on
    // IterableMechanism are correct.
    const keyRedefined = /** @type {string} */ (key);
    // There's only one key
    assertEquals('first', keyRedefined);
  }
}


/**
 * @param {?IterableMechanism} mechanism
 */
function testIteratorWithTwoValues(mechanism) {
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
}


/**
 * @param {?IterableMechanism} mechanism
 */
function testClear(mechanism) {
  if (!mechanism) {
    fail('Mechanism undefined for testClear');
  }
  mechanism.set('first', 'one');
  mechanism.set('second', 'two');
  mechanism.clear();
  assertNull(mechanism.get('first'));
  assertNull(mechanism.get('second'));
  assertEquals(0, mechanism.getCount());
  const keyIt = mechanism[Symbol.iterator]().next();
  assertTrue(keyIt.done);
  assertEquals(keyIt.value, undefined);
  const valIt = mechanism[Symbol.iterator]().next();
  assertTrue(valIt.done);
  assertEquals(valIt.value, undefined);
}


/**
 * @param {?IterableMechanism} mechanism
 */
function testClearClear(mechanism) {
  if (!mechanism) {
    fail('Mechanism undefined for testClearClear');
  }
  mechanism.clear();
  mechanism.clear();
  assertEquals(0, mechanism.getCount());
}


/**
 * @param {?IterableMechanism} mechanism
 */
function testIteratorWithWeirdKeys(mechanism) {
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
}

/**
 * @param {{
 *    getMechanism: function(): !IterableMechanism
 * }} state
 * @return {!Object}
 */
exports.register = function(state) {
  return bindTests(
      [
        testCount, testIteratorBasics, testIteratorWithTwoValues, testClear,
        testClearClear, testIteratorWithWeirdKeys
      ],
      (testCase) => {
        testCase(state.getMechanism());
      });
};