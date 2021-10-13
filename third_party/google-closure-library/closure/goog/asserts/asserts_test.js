/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.assertsTest');
goog.setTestOnly();

const AssertionError = goog.require('goog.asserts.AssertionError');
const TagName = goog.require('goog.dom.TagName');
const asserts = goog.require('goog.asserts');
const dom = goog.require('goog.dom');
const googString = goog.require('goog.string');
const reflect = goog.require('goog.reflect');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

/**
 * Test that the function throws an error with the given message.
 * @param {function()} failFunc
 * @param {string} expectedMsg
 */
function doTestMessage(failFunc, expectedMsg) {
  const error = assertThrows('failFunc should throw.', failFunc);
  // Test error message.
  assertEquals(expectedMsg, error.message);
}

testSuite({
  testAssert() {
    // None of them may throw exception
    asserts.assert(true);
    asserts.assert(1);
    asserts.assert([]);
    asserts.assert({});

    assertThrows('assert(false)', goog.partial(asserts.assert, false));
    assertThrows('assert(0)', goog.partial(asserts.assert, 0));
    assertThrows('assert(null)', goog.partial(asserts.assert, null));
    assertThrows('assert(undefined)', goog.partial(asserts.assert, undefined));

    // Test error messages.
    doTestMessage(goog.partial(asserts.assert, false), 'Assertion failed');
    doTestMessage(
        goog.partial(asserts.assert, false, 'ouch %s', 1),
        'Assertion failed: ouch 1');
  },

  testAssertExists() {
    // None of them may throw exception
    asserts.assertExists(true);
    asserts.assertExists(false);
    asserts.assertExists(1);
    asserts.assertExists(0);
    asserts.assertExists(NaN);
    asserts.assertExists('Hello');
    asserts.assertExists('');
    asserts.assertExists(/Hello/);
    asserts.assertExists([]);
    asserts.assertExists({});

    assertThrows(
        'assertExists(null)', goog.partial(asserts.assertExists, null));
    assertThrows(
        'assertExists(undefined)',
        goog.partial(asserts.assertExists, undefined));

    // Test error messages.
    doTestMessage(
        goog.partial(asserts.assertExists, null),
        'Assertion failed: Expected to exist: null.');
    doTestMessage(
        goog.partial(asserts.assertExists, null, 'ouch %s', 1),
        'Assertion failed: ouch 1');
  },

  testAssertExists_narrowing() {
    const /** number|null|undefined */ wideValue = 0;

    const /** number */ narrowReturn = asserts.assertExists(wideValue);
    const /** number */ narrowInScope = wideValue;

    reflect.sinkValue(narrowReturn);
    reflect.sinkValue(narrowInScope);
  },

  testFail() {
    assertThrows('fail()', asserts.fail);
    // Test error messages.
    doTestMessage(goog.partial(asserts.fail, false), 'Failure');
    doTestMessage(goog.partial(asserts.fail, 'ouch %s', 1), 'Failure: ouch 1');
  },

  testNumber() {
    asserts.assertNumber(1);
    assertThrows(
        'assertNumber(null)', goog.partial(asserts.assertNumber, null));
    // Test error messages.
    doTestMessage(
        goog.partial(asserts.assertNumber, null),
        'Assertion failed: Expected number but got null: null.');
    doTestMessage(
        goog.partial(asserts.assertNumber, '1234'),
        'Assertion failed: Expected number but got string: 1234.');
    doTestMessage(
        goog.partial(asserts.assertNumber, null, 'ouch %s', 1),
        'Assertion failed: ouch 1');
  },

  testString() {
    assertEquals('1', asserts.assertString('1'));
    assertThrows(
        'assertString(null)', goog.partial(asserts.assertString, null));
    // Test error messages.
    doTestMessage(
        goog.partial(asserts.assertString, null),
        'Assertion failed: Expected string but got null: null.');
    doTestMessage(
        goog.partial(asserts.assertString, 1234),
        'Assertion failed: Expected string but got number: 1234.');
    doTestMessage(
        goog.partial(asserts.assertString, null, 'ouch %s', 1),
        'Assertion failed: ouch 1');
  },

  // jslint:ignore start
  testFunction() {
    function f() {}
    assertEquals(f, asserts.assertFunction(f));
    assertThrows(
        'assertFunction(null)', goog.partial(asserts.assertFunction, null));
    // Test error messages.
    doTestMessage(
        goog.partial(asserts.assertFunction, null),
        'Assertion failed: Expected function but got null: null.');
    doTestMessage(
        goog.partial(asserts.assertFunction, 1234),
        'Assertion failed: Expected function but got number: 1234.');
    doTestMessage(
        goog.partial(asserts.assertFunction, null, 'ouch %s', 1),
        'Assertion failed: ouch 1');
  },

  // jslint:ignore end
  testObject() {
    const o = {};
    assertEquals(o, asserts.assertObject(o));
    assertThrows(
        'assertObject(null)', goog.partial(asserts.assertObject, null));
    // Test error messages.
    doTestMessage(
        goog.partial(asserts.assertObject, null),
        'Assertion failed: Expected object but got null: null.');
    doTestMessage(
        goog.partial(asserts.assertObject, 1234),
        'Assertion failed: Expected object but got number: 1234.');
    doTestMessage(
        goog.partial(asserts.assertObject, null, 'ouch %s', 1),
        'Assertion failed: ouch 1');
  },

  testArray() {
    const a = [];
    assertEquals(a, asserts.assertArray(a));
    assertThrows('assertArray({})', goog.partial(asserts.assertArray, {}));
    // Test error messages.
    doTestMessage(
        goog.partial(asserts.assertArray, null),
        'Assertion failed: Expected array but got null: null.');
    doTestMessage(
        goog.partial(asserts.assertArray, 1234),
        'Assertion failed: Expected array but got number: 1234.');
    doTestMessage(
        goog.partial(asserts.assertArray, null, 'ouch %s', 1),
        'Assertion failed: ouch 1');
  },

  testBoolean() {
    assertEquals(true, asserts.assertBoolean(true));
    assertEquals(false, asserts.assertBoolean(false));
    assertThrows(goog.partial(asserts.assertBoolean, null));
    assertThrows(goog.partial(asserts.assertBoolean, 'foo'));

    // Test error messages.
    doTestMessage(
        goog.partial(asserts.assertBoolean, null),
        'Assertion failed: Expected boolean but got null: null.');
    doTestMessage(
        goog.partial(asserts.assertBoolean, 1234),
        'Assertion failed: Expected boolean but got number: 1234.');
    doTestMessage(
        goog.partial(asserts.assertBoolean, null, 'ouch %s', 1),
        'Assertion failed: ouch 1');
  },

  testElement() {
    assertThrows(goog.partial(asserts.assertElement, null));
    assertThrows(goog.partial(asserts.assertElement, 'foo'));
    assertThrows(
        goog.partial(asserts.assertElement, dom.createTextNode('foo')));
    const elem = dom.createElement(TagName.DIV);
    assertEquals(elem, asserts.assertElement(elem));
  },

  testInstanceof() {
    /** @constructor */
    let F = function() {};
    asserts.assertInstanceof(new F(), F);
    const error = assertThrows(
        'assertInstanceof({}, F)',
        goog.partial(asserts.assertInstanceof, {}, F));
    // IE lacks support for function.name and will fallback to toString().
    const object = /object/.test(error.message) ? '[object Object]' : 'Object';
    const name = /F/.test(error.message) ? 'F' : 'unknown type name';

    // Test error messages.
    doTestMessage(
        goog.partial(asserts.assertInstanceof, {}, F),
        `Assertion failed: Expected instanceof ${name} but got ${object}` +
            '.');
    doTestMessage(
        goog.partial(asserts.assertInstanceof, {}, F, 'a %s', 1),
        'Assertion failed: a 1');
    doTestMessage(
        goog.partial(asserts.assertInstanceof, null, F),
        `Assertion failed: Expected instanceof ${name} but got null.`);
    doTestMessage(
        goog.partial(asserts.assertInstanceof, 5, F),
        'Assertion failed: ' +
            'Expected instanceof ' + name + ' but got number.');

    // Test a constructor a with a name (IE does not support function.name).
    if (!userAgent.IE) {
      F = function foo() {};
      doTestMessage(
          goog.partial(asserts.assertInstanceof, {}, F),
          `Assertion failed: Expected instanceof foo but got ${object}.`);
    }

    // Test a constructor with a displayName.
    F.displayName = 'bar';
    doTestMessage(
        goog.partial(asserts.assertInstanceof, {}, F),
        `Assertion failed: Expected instanceof bar but got ${object}.`);
  },

  testAssertionError() {
    const error = new AssertionError('foo %s %s', [1, 'two']);
    assertEquals('Wrong message', 'foo 1 two', error.message);
    assertEquals('Wrong messagePattern', 'foo %s %s', error.messagePattern);
  },

  testFailWithCustomErrorHandler() {
    try {
      let handledException;
      asserts.setErrorHandler((e) => {
        handledException = e;
      });

      const expectedMessage = 'Failure: Gevalt!';

      asserts.fail('Gevalt!');
      assertTrue('handledException is null.', handledException != null);
      assertTrue(
          `Message check failed.  Expected: ${expectedMessage} Actual: ` +
              handledException.message,
          googString.startsWith(expectedMessage, handledException.message));
    } finally {
      asserts.setErrorHandler(asserts.DEFAULT_ERROR_HANDLER);
    }
  },

  testAssertWithCustomErrorHandler() {
    try {
      let handledException;
      asserts.setErrorHandler((e) => {
        handledException = e;
      });

      const expectedMessage = 'Assertion failed: Gevalt!';

      asserts.assert(false, 'Gevalt!');
      assertTrue('handledException is null.', handledException != null);
      assertTrue(
          `Message check failed.  Expected: ${expectedMessage} Actual: ` +
              handledException.message,
          googString.startsWith(expectedMessage, handledException.message));
    } finally {
      asserts.setErrorHandler(asserts.DEFAULT_ERROR_HANDLER);
    }
  },

  testAssertFinite() {
    assertEquals(9, asserts.assertFinite(9));
    assertEquals(0, asserts.assertFinite(0));
    assertThrows(goog.partial(asserts.assertFinite, NaN));
    assertThrows(goog.partial(asserts.assertFinite, Infinity));
    assertThrows(goog.partial(asserts.assertFinite, -Infinity));
    assertThrows(goog.partial(asserts.assertFinite, 'foo'));
    assertThrows(goog.partial(asserts.assertFinite, true));

    // Test error messages.
    doTestMessage(
        goog.partial(asserts.assertFinite, NaN),
        'Assertion failed: Expected NaN to be a finite number but it is not.');
    doTestMessage(
        goog.partial(asserts.assertFinite, NaN, 'ouch %s', 1),
        'Assertion failed: ouch 1');
  },
});
