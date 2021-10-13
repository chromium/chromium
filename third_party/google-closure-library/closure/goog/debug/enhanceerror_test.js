/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.debugEnhanceErrorTest');
goog.setTestOnly();

const googDebug = goog.require('goog.debug');
const testSuite = goog.require('goog.testing.testSuite');

const THROW_STRING = 1;
const THROW_NPE = 2;
const THROW_ERROR = 3;
const THROW_ENHANCED_ERROR = 4;
const THROW_ENHANCED_STRING = 5;
const THROW_OBJECT = 6;

if (typeof debug == 'undefined') {
  function debug(str) {
    if (window.console) window.console.log(str);
  }
}

function foo(testNum) {
  bar(testNum);
}

function bar(testNum) {
  baz(testNum);
}

function baz(testNum) {
  try {
    switch (testNum) {
      case THROW_STRING:
        throwString();
        break;
      case THROW_NPE:
        throwNpe();
        break;
      case THROW_ERROR:
        throwError();
        break;
      case THROW_ENHANCED_ERROR:
        throwEnhancedError();
        break;
      case THROW_ENHANCED_STRING:
        throwEnhancedString();
        break;
      case THROW_OBJECT:
        throwObject();
        break;
    }
  } catch (e) {
    throw googDebug.enhanceError(e, 'message from baz');
  }
}

function throwString() {
  throw 'a string';
}

function throwNpe() {
  const nada = null;
  nada.noSuchFunction();
}

function throwError() {
  throw new Error('an error');
}

function throwEnhancedError() {
  throw googDebug.enhanceError(Error('dang!'), 'an enhanced error');
}

function throwEnhancedString() {
  throw googDebug.enhanceError('oh nos!');
}

/** @throws {*} */
function throwObject() {
  throw {property: 'value'};
}
testSuite({
  /** @suppress {missingProperties} suppression added to enable type checking */
  testEnhanceError() {
    // Tests are like this:
    // [test num, expect something in the stack, expect an extra message]
    const tests = [
      [THROW_STRING],
      [THROW_OBJECT],
      [THROW_NPE],
      [THROW_ERROR],
      [THROW_ENHANCED_ERROR, 'throwEnhancedError', 'an enhanced error'],
      [THROW_ENHANCED_STRING, 'throwEnhancedString'],
    ];
    for (let i = 0; i < tests.length; ++i) {
      const test = tests[i];
      const testNum = test[0];
      const testInStack = test[1];
      const testExtraMessage = test[2] || null;
      try {
        foo(testNum);
      } catch (e) {
        debug(googDebug.expose(e));
        const s = e.stack.split('\n');
        for (let j = 0; j < s.length; ++j) {
          debug(s[j]);
        }
        // 'baz' is always in the stack
        assertTrue('stack should contain "baz"', e.stack.indexOf('baz') != -1);

        if (testInStack) {
          assertTrue(
              `stack should contain "${testInStack}"`,
              e.stack.indexOf(testInStack) != -1);
        }
        if (testExtraMessage) {
          // 2 messages
          assertTrue(
              `message0 should contain "${testExtraMessage}"`,
              e.message0.indexOf(testExtraMessage) != -1);
          assertTrue(
              'message1 should contain "message from baz"',
              e.message1.indexOf('message from baz') != -1);
        } else {
          // 1 message
          assertTrue(
              'message0 should contain "message from baz"',
              e.message0.indexOf('message from baz') != -1);
        }
        continue;
      }
      fail('expected to catch an exception');
    }
  },
});
