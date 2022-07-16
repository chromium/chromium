/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.mockmatchersTest');
goog.setTestOnly('goog.testing.mockmatchersTest');

const ArgumentMatcher = goog.require('goog.testing.mockmatchers.ArgumentMatcher');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const matchers = goog.require('goog.testing.mockmatchers');
const testSuite = goog.require('goog.testing.testSuite');

// Simple classes to test the InstanceOf matcher.
const foo = function() {};
const bar = function() {};

// Simple class to test adding error messages to
// MockExpectation objects
function MockMock() {
  /** @suppress {globalThis} suppression added to enable type checking */
  this.errorMessages = [];
}

let mockExpect = null;

MockMock.prototype.addErrorMessage = function(msg) {
  this.errorMessages.push(msg);
};


MockMock.prototype.getErrorMessageCount = function() {
  return this.errorMessages.length;
};

testSuite({
  setUp() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    mockExpect = new MockMock();
  },


  testNoMatchName() {
    // A matcher that does not fill in the match name
    const matcher = new ArgumentMatcher(x => typeof x === 'string');

    // Make sure the lack of match name doesn't affect the ability
    // to return True/False
    assertTrue(matcher.matches('hello'));
    assertFalse(matcher.matches(123));

    // Make sure we handle the lack of a match name
    assertFalse(matcher.matches(456, mockExpect));
    assertEquals(1, mockExpect.errorMessages.length);
    assertEquals(
        'Expected: missing mockmatcher description ' +
            'but was: <456> (Number)',
        mockExpect.errorMessages[0]);
  },


  /** @suppress {checkTypes} suppression added to enable type checking */
  testInstanceOf() {
    const matcher = new matchers.InstanceOf(foo);
    assertTrue(matcher.matches(new foo()));
    assertFalse(matcher.matches(new bar()));

    assertFalse(matcher.matches(new bar(), mockExpect));
    assertEquals(1, mockExpect.errorMessages.length);
    assertEquals(
        'Expected: instanceOf() ' +
            'but was: <[object Object]> (Object)',
        mockExpect.errorMessages[0]);
  },


  testTypeOf() {
    const matcher = new matchers.TypeOf('number');
    assertTrue(matcher.matches(1));
    assertTrue(matcher.matches(2));
    assertFalse(matcher.matches('test'));

    assertFalse(matcher.matches(true, mockExpect));
    assertEquals(1, mockExpect.errorMessages.length);
    assertEquals(
        'Expected: typeOf(number) but was: <true> (Boolean)',
        mockExpect.errorMessages[0]);
  },


  testRegexpMatch() {
    const matcher = new matchers.RegexpMatch(/^cho[dtp]/);
    assertTrue(matcher.matches('chodhop'));
    assertTrue(matcher.matches('chopper'));
    assertFalse(matcher.matches('chocolate'));
    assertFalse(matcher.matches(null));

    assertFalse(matcher.matches('an anger', mockExpect));
    assertEquals(1, mockExpect.errorMessages.length);
    assertEquals(
        'Expected: match(/^cho[dtp]/) but was: <an anger> (String)',
        mockExpect.errorMessages[0]);
  },


  testObjectEquals() {
    // Test a simple match.
    const simpleMatcher = new matchers.ObjectEquals({name: 'Bob', age: 42});
    assertTrue(simpleMatcher.matches({name: 'Bob', age: 42}, mockExpect));
    assertEquals(0, mockExpect.getErrorMessageCount());
    expectObjectEqualsFailure(
        simpleMatcher, {name: 'Bill', age: 42},
        'name: Expected <Bob> (String) but was <Bill> (String)');
    expectObjectEqualsFailure(
        simpleMatcher, {name: 'Bob', age: 21},
        'age: Expected <42> (Number) but was <21> (Number)');
    expectObjectEqualsFailure(
        simpleMatcher, {name: 'Bob'},
        'property age not present in actual Object');
    expectObjectEqualsFailure(
        simpleMatcher, {name: 'Bob', age: 42, country: 'USA'},
        'property country not present in expected Object');

    // Multiple mismatches should include multiple messages.
    expectObjectEqualsFailure(
        simpleMatcher, {name: 'Jim', age: 36},
        'name: Expected <Bob> (String) but was <Jim> (String)\n' +
            '   age: Expected <42> (Number) but was <36> (Number)');
  },

  testComplexObjectEquals() {
    const complexMatcher = new matchers.ObjectEquals(
        {a: 'foo', b: 2, c: ['bar', 3], d: {sub1: 'baz', sub2: -1}});
    assertTrue(complexMatcher.matches(
        {a: 'foo', b: 2, c: ['bar', 3], d: {sub1: 'baz', sub2: -1}}));
    expectObjectEqualsFailure(
        complexMatcher,
        {a: 'foo', b: 2, c: ['bar', 3], d: {sub1: 'zap', sub2: -1}},
        'sub1: Expected <baz> (String) but was <zap> (String)');
    expectObjectEqualsFailure(
        complexMatcher,
        {a: 'foo', b: 2, c: ['bar', 6], d: {sub1: 'baz', sub2: -1}},
        'c[1]: Expected <3> (Number) but was <6> (Number)');
  },


  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testSaveArgument() {
    let saveMatcher = new matchers.SaveArgument();
    assertTrue(saveMatcher.matches(42));
    assertEquals(42, saveMatcher.arg);

    saveMatcher = new matchers.SaveArgument(x => typeof x === 'string');
    assertTrue(saveMatcher.matches('test'));
    assertEquals('test', saveMatcher.arg);
    assertFalse(saveMatcher.matches(17));
    assertEquals(17, saveMatcher.arg);
    assertArrayEquals(['test', 17], saveMatcher.allArgs);

    saveMatcher =
        new matchers.SaveArgument(new matchers.ObjectEquals({value: 'value'}));
    assertTrue(saveMatcher.matches({value: 'value'}));
    assertEquals('value', saveMatcher.arg.value);
    assertFalse(saveMatcher.matches('test'));
    assertEquals('test', saveMatcher.arg);
    assertArrayEquals([{value: 'value'}, 'test'], saveMatcher.allArgs);
  },


  testIsArray() {
    assertTrue(matchers.isArray.matches([]));
    assertTrue(matchers.isArray.matches(new Array()));
    assertFalse(matchers.isArray.matches('test'));

    assertFalse(matchers.isArray.matches({}, mockExpect));
    assertEquals(1, mockExpect.errorMessages.length);
    assertEquals(
        'Expected: isArray but was: <[object Object]> (Object)',
        mockExpect.errorMessages[0]);
  },


  testIsArrayLike() {
    const nodeList = (function() {
      const div = dom.createElement(TagName.DIV);
      div.appendChild(dom.createElement(TagName.P));
      div.appendChild(dom.createElement(TagName.P));
      return dom.getElementsByTagName(TagName.DIV, div);
    })();

    assertTrue(matchers.isArrayLike.matches([]));
    assertTrue(matchers.isArrayLike.matches(new Array()));
    assertTrue(matchers.isArrayLike.matches(nodeList));
    assertFalse(matchers.isArrayLike.matches('test'));

    assertFalse(matchers.isArrayLike.matches(3, mockExpect));
    assertEquals(1, mockExpect.errorMessages.length);
    assertEquals(
        'Expected: isArrayLike but was: <3> (Number)',
        mockExpect.errorMessages[0]);
  },


  testIsDateLike() {
    assertTrue(matchers.isDateLike.matches(new Date()));
    assertFalse(matchers.isDateLike.matches('test'));

    assertFalse(matchers.isDateLike.matches('test', mockExpect));
    assertEquals(1, mockExpect.errorMessages.length);
    assertEquals(
        'Expected: isDateLike but was: <test> (String)',
        mockExpect.errorMessages[0]);
  },


  testIsString() {
    assertTrue(matchers.isString.matches('a'));
    assertTrue(matchers.isString.matches('b'));
    assertFalse(matchers.isString.matches(null));

    assertFalse(matchers.isString.matches(null, mockExpect));
    assertEquals(1, mockExpect.errorMessages.length);
    assertEquals(
        'Expected: isString but was: <null>', mockExpect.errorMessages[0]);
  },


  testIsBoolean() {
    assertTrue(matchers.isBoolean.matches(true));
    assertTrue(matchers.isBoolean.matches(false));
    assertFalse(matchers.isBoolean.matches(null));

    assertFalse(matchers.isBoolean.matches([], mockExpect));
    assertEquals(1, mockExpect.errorMessages.length);
    assertEquals(
        'Expected: isBoolean but was: <> (Array)', mockExpect.errorMessages[0]);
  },


  testIsNumber() {
    assertTrue(matchers.isNumber.matches(-1));
    assertTrue(matchers.isNumber.matches(1));
    assertTrue(matchers.isNumber.matches(1.25));
    assertFalse(matchers.isNumber.matches(null));

    assertFalse(matchers.isNumber.matches('hello', mockExpect));
    assertEquals(1, mockExpect.errorMessages.length);
    assertEquals(
        'Expected: isNumber but was: <hello> (String)',
        mockExpect.errorMessages[0]);
  },


  testIsFunction() {
    assertTrue(matchers.isFunction.matches(function() {}));
    assertFalse(matchers.isFunction.matches('test'));

    assertFalse(matchers.isFunction.matches({}, mockExpect));
    assertEquals(1, mockExpect.errorMessages.length);
    assertEquals(
        'Expected: isFunction but was: <[object Object]> (Object)',
        mockExpect.errorMessages[0]);
  },


  /** @suppress {checkTypes} suppression added to enable type checking */
  testIsObject() {
    assertTrue(matchers.isObject.matches({}));
    assertTrue(matchers.isObject.matches(new Object()));
    assertTrue(matchers.isObject.matches(new function() {}));
    assertTrue(matchers.isObject.matches([]));
    assertTrue(matchers.isObject.matches(new Array()));
    assertTrue(matchers.isObject.matches(function() {}));
    assertFalse(matchers.isObject.matches(null));

    assertFalse(matchers.isObject.matches(1234, mockExpect));
    assertEquals(1, mockExpect.errorMessages.length);
    assertEquals(
        'Expected: isObject but was: <1234> (Number)',
        mockExpect.errorMessages[0]);
  },


  testIsNodeLike() {
    assertFalse(matchers.isNodeLike.matches({}));
    assertFalse(matchers.isNodeLike.matches(1));
    assertFalse(matchers.isNodeLike.matches(function() {}));
    assertFalse(matchers.isNodeLike.matches(false));
    assertTrue(matchers.isNodeLike.matches(document.body));
    assertTrue(matchers.isNodeLike.matches(dom.getElement('someDiv')));

    assertFalse(matchers.isNodeLike.matches('test', mockExpect));
    assertEquals(1, mockExpect.errorMessages.length);
    assertEquals(
        'Expected: isNodeLike but was: <test> (String)',
        mockExpect.errorMessages[0]);
  },


  /** @suppress {checkTypes} suppression added to enable type checking */
  testIgnoreArgumentsMatcher() {
    // ignoreArgument always returns true:
    assertTrue(matchers.ignoreArgument.matches());
    assertTrue(matchers.ignoreArgument.matches(356));
    assertTrue(matchers.ignoreArgument.matches('str'));
    assertTrue(matchers.ignoreArgument.matches(['array', 123, false]));
    assertTrue(matchers.ignoreArgument.matches({'map': 1, key2: 'value2'}));
  },


  testFlexibleArrayMatcher() {
    // Test that basic lists are verified properly.
    let a1 = [1, 'test'];
    let a2 = [1, 'test'];
    let a3 = [1, 'test', 'extra'];
    assertTrue(matchers.flexibleArrayMatcher(a1, a2));
    assertFalse(matchers.flexibleArrayMatcher(a1, a3));

    // Test that basic lists with basic class instances are verified properly.
    /** @suppress {checkTypes} suppression added to enable type checking */
    const instance = new foo();
    a1 = [1, 'test', instance];
    a2 = [1, 'test', instance];
    /** @suppress {checkTypes} suppression added to enable type checking */
    a3 = [1, 'test', new foo()];
    assertTrue(matchers.flexibleArrayMatcher(a1, a2));
    assertTrue(matchers.flexibleArrayMatcher(a1, a3));

    // Create an argument verifier that returns a consistent value.
    let verifyValue = true;
    const argVerifier = function() {};
    goog.inherits(argVerifier, matchers.ArgumentMatcher);
    argVerifier.prototype.matches = function(arg) {
      return verifyValue;
    };

    // Test that the arguments are always verified when the verifier returns
    // true.
    /** @suppress {checkTypes} suppression added to enable type checking */
    a1 = [1, 'test', new argVerifier()];
    a2 = [1, 'test', 'anything'];
    a3 = [1, 'test', 12345];
    assertTrue(matchers.flexibleArrayMatcher(a1, a2));
    assertTrue(matchers.flexibleArrayMatcher(a1, a3));

    // Now test the case when then verifier returns false.
    verifyValue = false;
    assertFalse(matchers.flexibleArrayMatcher(a1, a2));
    assertFalse(matchers.flexibleArrayMatcher(a1, a3));

    // And test we report errors back up via the opt_expectation
    assertFalse(matchers.flexibleArrayMatcher(a2, a3, mockExpect));
    assertEquals(1, mockExpect.errorMessages.length);
    assertEquals(
        'Expected <anything> (String) but was <12345> (Number)',
        mockExpect.errorMessages[0]);

    // And test we report errors found via the matcher
    a1 = [1, matchers.isString];
    a2 = [1, 'test string'];
    a3 = [1, null];
    assertTrue(matchers.flexibleArrayMatcher(a1, a2, mockExpect));
    assertFalse(matchers.flexibleArrayMatcher(a1, a3, mockExpect));
    // Old error is still there
    assertEquals(2, mockExpect.errorMessages.length);
    assertEquals(
        'Expected <anything> (String) but was <12345> (Number)',
        mockExpect.errorMessages[0]);
    // plus the new error...
    assertEquals(
        'Expected: isString but was: <null>', mockExpect.errorMessages[1]);
  },

});


/**
 * Utility method for checking for an ObjectEquals match failure.  Checks that
 * the expected error message was included in the error messages appended to
 * the expectation object.
 * @param {?} matcher
 *     The matcher to test against.
 * @param {!Object} matchObject The object to compare.
 * @param {string=} opt_errorMsg The deep object comparison failure message
 *     to check for.
 */
function expectObjectEqualsFailure(matcher, matchObject, opt_errorMsg) {
  mockExpect.errorMessages = [];
  assertFalse(matcher.matches(matchObject, mockExpect));
  assertNotEquals(0, mockExpect.getErrorMessageCount());
  if (opt_errorMsg) {
    assertContains(opt_errorMsg, mockExpect.errorMessages[0]);
  }
}
