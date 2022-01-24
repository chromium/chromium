/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.recordFunctionTest');
goog.setTestOnly();

const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const functions = goog.require('goog.functions');
const recordConstructor = goog.require('goog.testing.recordConstructor');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

const stubs = new PropertyReplacer();

testSuite({
  tearDown() {
    stubs.reset();
  },

  testNoCalls() {
    const f = recordFunction(functions.identity);
    assertEquals('call count', 0, f.getCallCount());
    assertNull('last call', f.getLastCall());
    assertArrayEquals('all calls', [], f.getCalls());
  },

  testWithoutArguments() {
    const f = recordFunction();
    assertUndefined('f(1)', f(1));
    assertEquals('call count', 1, f.getCallCount());
    const lastCall = f.getLastCall();
    assertEquals(
        'original function', goog.nullFunction, lastCall.getFunction());
    assertEquals('this context', undefined, lastCall.getThis());
    assertArrayEquals('arguments', [1], lastCall.getArguments());
    assertEquals('arguments[0]', 1, lastCall.getArgument(0));
    assertUndefined('arguments[1]', lastCall.getArgument(1));
    assertUndefined('return value', lastCall.getReturnValue());
  },

  testWithIdentityFunction() {
    const f = recordFunction(functions.identity);
    const dummyThis = {};
    assertEquals('f(1)', 1, f(1));
    assertEquals('f.call(dummyThis, 2)', 2, f.call(dummyThis, 2));

    const calls = f.getCalls();
    const firstCall = calls[0];
    const lastCall = f.getLastCall();
    assertEquals('call count', 2, f.getCallCount());
    assertEquals('last call', calls[1], lastCall);
    assertEquals(
        'original function', functions.identity, lastCall.getFunction());
    assertEquals('this context of first call', undefined, firstCall.getThis());
    assertEquals('this context of last call', dummyThis, lastCall.getThis());
    assertArrayEquals(
        'arguments of the first call', [1], firstCall.getArguments());
    assertArrayEquals(
        'arguments of the last call', [2], lastCall.getArguments());
    assertEquals(
        'return value of the first call', 1, firstCall.getReturnValue());
    assertEquals('return value of the last call', 2, lastCall.getReturnValue());
    assertNull('error thrown by the first call', firstCall.getError());
    assertNull('error thrown by the last call', lastCall.getError());
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testWithErrorFunction() {
    const f = recordFunction(functions.error('error'));

    const error = assertThrows('f(1) should throw an error', () => {
      f(1);
    });
    assertEquals('error message', 'error', error.message);
    assertEquals('call count', 1, f.getCallCount());
    const lastCall = f.getLastCall();
    assertEquals('this context', undefined, lastCall.getThis());
    assertArrayEquals('arguments', [1], lastCall.getArguments());
    assertUndefined('return value', lastCall.getReturnValue());
    assertEquals(
        'recorded error message', 'error', lastCall.getError().message);
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testWithClass() {
    const ns = {};
    /** @constructor @struct */
    ns.TestClass = function(num) {
      this.setX(ns.TestClass.identity(1) + num);
    };
    ns.TestClass.prototype.setX = function(x) {
      /**
       * @suppress {checkTypes,missingProperties} suppression added to enable
       * type checking
       */
      this.x = x;
    };
    ns.TestClass.identity = (x) => x;
    const originalNsTestClass = ns.TestClass;

    stubs.set(ns, 'TestClass', recordConstructor(ns.TestClass));
    stubs.set(
        ns.TestClass.prototype, 'setX',
        recordFunction(ns.TestClass.prototype.setX));
    stubs.set(ns.TestClass, 'identity', recordFunction(ns.TestClass.identity));

    const obj = new ns.TestClass(2);
    assertEquals('constructor is called once', 1, ns.TestClass.getCallCount());
    /**
     * @suppress {missingProperties} suppression added to enable type checking
     */
    const lastConstructorCall = ns.TestClass.getLastCall();
    assertArrayEquals(
        '... with argument 2', [2], lastConstructorCall.getArguments());
    assertEquals('the created object', obj, lastConstructorCall.getThis());
    assertEquals(
        'type of the created object', originalNsTestClass, obj.constructor);

    assertEquals('setX is called once', 1, obj.setX.getCallCount());
    assertArrayEquals(
        '... with argument 3', [3], obj.setX.getLastCall().getArguments());
    assertEquals('The x field is properly set', 3, obj.x);

    assertEquals(
        'identity is called once', 1, ns.TestClass.identity.getCallCount());
    assertArrayEquals(
        '... with argument 1', [1],
        ns.TestClass.identity.getLastCall().getArguments());
  },

  testPopLastCall() {
    const f = recordFunction();
    f(0);
    f(1);

    const firstCall = f.getCalls()[0];
    const lastCall = f.getCalls()[1];
    assertEquals('return value of popLastCall', lastCall, f.popLastCall());
    assertArrayEquals('1 call remains', [firstCall], f.getCalls());
    assertEquals(
        'next return value of popLastCall', firstCall, f.popLastCall());
    assertArrayEquals('0 calls remain', [], f.getCalls());
    assertNull('no more calls to pop', f.popLastCall());
  },

  testReset() {
    const f = recordFunction();
    f(0);
    f(1);

    assertEquals('Should be two calls.', 2, f.getCallCount());

    f.reset();

    assertEquals('Call count should reset.', 0, f.getCallCount());
  },

  testAssertCallCount() {
    const f = recordFunction(functions.identity);

    f.assertCallCount(0);
    f.assertCallCount('Unexpected failure.', 0);

    f('Poodles');
    f.assertCallCount(1);
    f.assertCallCount('Unexpected failure.', 1);

    f('Hopscotch');
    f.assertCallCount(2);

    f.reset();
    f.assertCallCount(0);

    f('Bedazzler');
    f.assertCallCount(1);

    const error = assertThrowsJsUnitException(() => {
      f.assertCallCount(11);
    });
    assertEquals(error.comment, 'Expected 11 call(s), but was 1.');

    const comment =
        'This application has requested the Runtime to terminate it ' +
        'in an unusual way.';
    const error2 = assertThrowsJsUnitException(() => {
      f.assertCallCount(comment, 12);
    });
    assertEquals(error2.comment, 'Expected 12 call(s), but was 1. ' + comment);
  },

  async testWaitForCalls() {
    const f = recordFunction(functions.identity);

    setTimeout(() => {
      f('Poodles');
    }, 0);
    f.assertCallCount(0);
    await f.waitForCalls(1);
    f.assertCallCount(1);

    setTimeout(() => {
      f('Hopscotch');
      setTimeout(() => {
        f('Bedazzler');
      }, 0);
    }, 0);
    await f.waitForCalls(3);
    f.assertCallCount(3);
    await f.waitForCalls(1);
    f.assertCallCount(3);
    f.reset();
    let resolved = false;
    const finished = new Promise((resolve) => {
      setTimeout(() => {
        assertFalse(resolved);
        resolve();
        f('Poodles');
      }, 0);
    });
    await f.waitForCalls(1);
    resolved = true;
    await finished;
    f.assertCallCount(1);
  },

  async testWaitCalls_Reset() {
    const f = recordFunction(functions.identity);

    f.waitForCalls(1).then(() => {
      fail('resolved a waitForCalls promise that was reset.');
    });

    f.reset();
    f('Poodles');
    await new Promise((resolve) => {
      setTimeout(resolve, 0);
    });
  },
});
