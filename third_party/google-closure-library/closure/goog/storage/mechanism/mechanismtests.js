/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Unit tests for the abstract storage mechanism interface.
 *
 * These tests should be included in tests of any class extending
 * goog.storage.mechanism.Mechanism.
 */

goog.module('goog.storage.mechanism.mechanismTests');
goog.setTestOnly();

const ErrorCode = goog.require('goog.storage.mechanism.ErrorCode');
const Mechanism = goog.require('goog.storage.mechanism.Mechanism');
const userAgent = goog.require('goog.userAgent');
const {assertEquals, assertNull, assertTrue} = goog.require('goog.testing.asserts');
const {bindTests} = goog.require('goog.storage.mechanism.testhelpers');

/**
 * @param {{
 *  getMechanism: function(): !Mechanism,
 *  getMinimumQuota: function(): number,
 * }} state
 * @return {!Object}
 */
exports.register = function(state) {
  return {...bindTests(
      [
        testSetGet,
        testChange,
        testRemove,
        testSetRemoveSet,
        testRemoveRemove,
        testSetTwo,
        testChangeTwo,
        testSetRemoveThree,
        testEmptyValue,
        testWeirdKeys,
      ],
      (testCase) => {
        testCase(state.getMechanism());
      }),
      ...bindTests([testQuota], (testCase) => {
        testCase(state.getMechanism(), state.getMinimumQuota());
      }),
  };
};


/**
 * @param {!Mechanism} mechanism
 */
function testSetGet(mechanism) {
  mechanism.set('first', 'one');
  assertEquals('one', mechanism.get('first'));
}


/**
 * @param {!Mechanism} mechanism
 */
function testChange(mechanism) {
  mechanism.set('first', 'one');
  mechanism.set('first', 'two');
  assertEquals('two', mechanism.get('first'));
}


/**
 * @param {!Mechanism} mechanism
 */
function testRemove(mechanism) {
  mechanism.set('first', 'one');
  mechanism.remove('first');
  assertNull(mechanism.get('first'));
}


/**
 * @param {!Mechanism} mechanism
 */
function testSetRemoveSet(mechanism) {
  mechanism.set('first', 'one');
  mechanism.remove('first');
  mechanism.set('first', 'one');
  assertEquals('one', mechanism.get('first'));
}


/**
 * @param {!Mechanism} mechanism
 */
function testRemoveRemove(mechanism) {
  mechanism.remove('first');
  mechanism.remove('first');
  assertNull(mechanism.get('first'));
}


/**
 * @param {!Mechanism} mechanism
 */
function testSetTwo(mechanism) {
  mechanism.set('first', 'one');
  mechanism.set('second', 'two');
  assertEquals('one', mechanism.get('first'));
  assertEquals('two', mechanism.get('second'));
}


/**
 * @param {!Mechanism} mechanism
 */
function testChangeTwo(mechanism) {
  mechanism.set('first', 'one');
  mechanism.set('second', 'two');
  mechanism.set('second', 'three');
  mechanism.set('first', 'four');
  assertEquals('four', mechanism.get('first'));
  assertEquals('three', mechanism.get('second'));
}


/**
 * @param {!Mechanism} mechanism
 */
function testSetRemoveThree(mechanism) {
  mechanism.set('first', 'one');
  mechanism.set('second', 'two');
  mechanism.set('third', 'three');
  mechanism.remove('second');
  assertNull(mechanism.get('second'));
  assertEquals('one', mechanism.get('first'));
  assertEquals('three', mechanism.get('third'));
  mechanism.remove('first');
  assertNull(mechanism.get('first'));
  assertEquals('three', mechanism.get('third'));
  mechanism.remove('third');
  assertNull(mechanism.get('third'));
}


/**
 * @param {!Mechanism} mechanism
 */
function testEmptyValue(mechanism) {
  mechanism.set('third', '');
  assertEquals('', mechanism.get('third'));
}


/**
 * @param {!Mechanism} mechanism
 */
function testWeirdKeys(mechanism) {
  // Some weird keys. We leave out some tests for some browsers where they
  // trigger browser bugs, and where the keys are too obscure to prepare a
  // workaround.
  mechanism.set(' ', 'space');
  mechanism.set('=+!@#$%^&*()-_\\|;:\'",./<>?[]{}~`', 'control');
  mechanism.set(
      '\u4e00\u4e8c\u4e09\u56db\u4e94\u516d\u4e03\u516b\u4e5d\u5341', 'ten');
  mechanism.set('\0', 'null');
  mechanism.set('\0\0', 'double null');
  mechanism.set('\0A', 'null A');
  mechanism.set('', 'zero');
  assertEquals('space', mechanism.get(' '));
  assertEquals('control', mechanism.get('=+!@#$%^&*()-_\\|;:\'",./<>?[]{}~`'));
  assertEquals(
      'ten',
      mechanism.get(
          '\u4e00\u4e8c\u4e09\u56db\u4e94\u516d\u4e03\u516b\u4e5d\u5341'));
  if (!userAgent.IE) {
    // IE does not properly handle nulls in HTML5 localStorage keys (IE8, IE9).
    // https://connect.microsoft.com/IE/feedback/details/667799/
    assertEquals('null', mechanism.get('\0'));
    assertEquals('double null', mechanism.get('\0\0'));
    assertEquals('null A', mechanism.get('\0A'));
  }
  if (!userAgent.GECKO) {
    // Firefox does not properly handle the empty key (FF 3.5, 3.6, 4.0).
    // https://bugzilla.mozilla.org/show_bug.cgi?id=510849
    assertEquals('zero', mechanism.get(''));
  }
}


/**
 * @param {!Mechanism} mechanism
 * @param {number} minimumQuota
 */
function testQuota(mechanism, minimumQuota) {
  var buffer = '\u03ff';  // 2 bytes
  var savedBytes = 0;
  try {
    while (buffer.length < minimumQuota) {
      buffer = buffer + buffer;
      mechanism.set('foo', buffer);
      savedBytes = buffer.length;
    }
  } catch (ex) {
    if (ex != ErrorCode.QUOTA_EXCEEDED) {
      throw ex;
    }
  }
  mechanism.remove('foo');
  assertTrue(savedBytes >= minimumQuota);
}
