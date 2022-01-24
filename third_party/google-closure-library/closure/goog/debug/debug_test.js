/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.debugTest');
goog.setTestOnly();

const debug = goog.require('goog.debug');
const errorcontext = goog.require('goog.debug.errorcontext');
const testSuite = goog.require('goog.testing.testSuite');

/**
 * Asserts that a substring can be found in a specified text string.
 * @param {string} substring The substring to search for.
 * @param {string} text The text string to search within.
 */
function assertContainsSubstring(substring, text) {
  assertNotEquals(
      `Could not find "${substring}" in "${text}"`, -1, text.search(substring));
}

testSuite({
  testMakeWhitespaceVisible() {
    assertEquals(
        'Hello[_][_]World![r][n]\n' +
            '[r][n]\n' +
            '[f][f]I[_]am[t][t]here![r][n]\n',
        debug.makeWhitespaceVisible(
            'Hello  World!\r\n\r\n\f\fI am\t\there!\r\n'));
  },

  testGetFunctionNameOfMultilineFunction() {
    // DO NOT FORMAT THIS - it is expected that "oddlyFormatted" be on a
    // separate line from the function keyword.
    // clang-format off
    function
        oddlyFormatted() {}
    // clang-format on
    assertEquals('oddlyFormatted', debug.getFunctionName(oddlyFormatted));
  },

  testDeepExpose() {
    const a = {};
    const b = {};
    const c = {};
    a.ancestor = a;
    a.otherObject = b;
    a.otherObjectAgain = b;
    b.nextLevel = c;
    // Add Uid to a before deepExpose.
    const aUid = goog.getUid(a);

    const deepExpose = debug.deepExpose(a);

    assertContainsSubstring(
        `ancestor = ... reference loop detected .id=${aUid}. ...`, deepExpose);

    assertContainsSubstring('otherObjectAgain = {', deepExpose);

    // Make sure we've reset Uids after the deepExpose call.
    assert(goog.hasUid(a));
    assertFalse(goog.hasUid(b));
    assertFalse(goog.hasUid(c));
  },

  testEnhanceErrorWithContext() {
    const err = 'abc';
    const context = {firstKey: 'first', secondKey: 'another key'};
    const errorWithContext = debug.enhanceErrorWithContext(err, context);
    assertObjectEquals(context, errorcontext.getErrorContext(errorWithContext));
  },

  testEnhanceErrorWithContext_combinedContext() {
    const err = new Error('abc');
    errorcontext.addErrorContext(err, 'a', '123');
    const context = {b: '456', c: '789'};
    const errorWithContext = debug.enhanceErrorWithContext(err, context);
    assertObjectEquals(
        {a: '123', b: '456', c: '789'},
        errorcontext.getErrorContext(errorWithContext));
  },

  testFreeze_nonDebug() {
    if (goog.DEBUG && typeof Object.freeze == 'function') return;
    const a = {};
    assertEquals(a, debug.freeze(a));
    a.foo = 42;
    assertEquals(42, a.foo);
  },

  testFreeze_debug() {
    if (goog.DEBUG || typeof Object.freeze != 'function') return;
    const a = {};
    assertEquals(a, debug.freeze(a));
    try {
      a.foo = 42;
    } catch (expectedInStrictMode) {
    }
    assertUndefined(a.foo);
  },

  testNormalizeErrorObject_actualErrorObject() {
    const err = debug.normalizeErrorObject(new Error('abc'));

    assertEquals('Error', err.name);
    assertEquals('abc', err.message);
  },

  testNormalizeErrorObject_actualErrorObject_withNoMessage() {
    const err = debug.normalizeErrorObject(new Error());

    assertEquals('Error', err.name);
    assertEquals('', err.message);
  },

  testNormalizeErrorObject_null() {
    const err = debug.normalizeErrorObject(null);

    assertEquals('Unknown error', err.name);
    assertEquals('Unknown Error of type "null/undefined"', err.message);
  },

  testNormalizeErrorObject_undefined() {
    const err = debug.normalizeErrorObject(undefined);

    assertEquals('Unknown error', err.name);
    assertEquals('Unknown Error of type "null/undefined"', err.message);
  },

  testNormalizeErrorObject_string() {
    const err = debug.normalizeErrorObject('abc');

    assertEquals('Unknown error', err.name);
    assertEquals('abc', err.message);
  },

  testNormalizeErrorObject_number() {
    const err = debug.normalizeErrorObject(10);

    assertEquals('UnknownError', err.name);
    assertEquals('Unknown Error of type "Number": 10', err.message);
  },

  testNormalizeErrorObject_nonErrorObject() {
    const err = debug.normalizeErrorObject({foo: 'abc'});

    assertEquals('UnknownError', err.name);
    assertEquals('Unknown Error of type "Object"', err.message);
  },

  testNormalizeErrorObject_objectCreateNull() {
    const err = debug.normalizeErrorObject(Object.create(null));

    assertEquals('UnknownError', err.name);
    assertEquals('Unknown Error of unknown type', err.message);
  },

  testNormalizeErrorObject_instanceOfClass() {
    const TestClass = function(text) {
      this.text = text;
    };
    /** @suppress {checkTypes} suppression added to enable type checking */
    const instance = new TestClass('abc');
    const err = debug.normalizeErrorObject(instance);

    assertEquals('UnknownError', err.name);
    // https://www.ecma-international.org/ecma-262/6.0/#sec-assignment-operators-runtime-semantics-evaluation
    // says `instance.contstructor.name` should be "TestClass", but IE & Edge
    // don't match that spec, so get "[Anonymous]" from
    // `goog.debug.getFunctionName`.
    if (TestClass.name) {
      assertEquals('Unknown Error of type "TestClass"', err.message);
    } else {
      assertEquals('Unknown Error of type "[Anonymous]"', err.message);
    }
  },

  testNormalizeErrorObject_objectWithToString() {
    const err = debug.normalizeErrorObject({
      toString: function() {
        return 'Error Message';
      }
    });

    assertEquals('UnknownError', err.name);
    assertEquals('Unknown Error of type "Object": Error Message', err.message);
  },

  testNormalizeErrorObject_enumerable() {
    const err = debug.normalizeErrorObject(new Error());

    let properties = 0;
    for (let x in err) {
      properties++;
    }

    assertEquals(5, properties);
  },

});
