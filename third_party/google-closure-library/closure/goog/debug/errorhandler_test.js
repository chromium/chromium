/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.debug.ErrorHandlerTest');
goog.setTestOnly();

const ErrorHandler = goog.require('goog.debug.ErrorHandler');
const MockControl = goog.require('goog.testing.MockControl');
const NativeResolver = goog.require('goog.promise.NativeResolver');
const TestCase = goog.require('goog.testing.TestCase');
const dispose = goog.require('goog.dispose');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

const PROTECTED_FUNCTION_ERROR_PREFIX =
    ErrorHandler.ProtectedFunctionError.MESSAGE_PREFIX;
const ERROR_HANDLER_TEST_ERROR = 'ERROR_HANDLER_TEST_ERROR';

let errorHandler;
let mockControl;

let state = {};

const JSUnitOnError = window.onerror;
// Here we set up an `onerror` handler to be able to catch the re-thrown errors
// at the end of the implementation of
// `goog.debug.ErrorHandler.prototype.handleError_`.
/**
 * A function that will record expected caught errors in the state object.
 * Unexpected errors will be relayed to the previously installed handler and
 * re-thrown.
 * @param {string} error the error
 */
window.onerror = (error) => {
  if (!error.includes(ERROR_HANDLER_TEST_ERROR)) {
    JSUnitOnError(error);
    fail(`An unexpected error reached the \`onerror\` handler: ${error}`);
    throw error;
  }
  state.lastUncaughtError = error;
};

/**
 * A function that constructs functions that throw errors and call resolvers.
 * @param {!Function} resolve the resolver to call when.
 * @returns {Function!} the error thrower.
 */
function errorThrowerFactory(resolve) {
  return function() {
    resolve && resolve();
    throw ERROR_HANDLER_TEST_ERROR;
  };
}

/**
 * @suppress {strictMissingProperties} suppression added to enable type
 * checking
 */
function assertMethodCalledHelper(method) {
  assertTrue('An error was not re-thrown', Boolean(state.lastUncaughtError));
  assertTrue(
      'The re-thrown error does not include the "Protected Function" prefix',
      state.lastUncaughtError.includes(PROTECTED_FUNCTION_ERROR_PREFIX));
  const errorSansPrefix = state.lastUncaughtError.replace(
      new RegExp(`.*${PROTECTED_FUNCTION_ERROR_PREFIX}`), '');
  assertEquals(
      'The Error Handler did not catch the error', errorSansPrefix,
      errorHandler.ex);
  assertTrue(
      `The protected function "${method}" was not called`,
      state.fake[method].getCallCount() >= 1);
  assertTrue(
      `"this" not passed to original ${method}`,
      state.fake[method].getLastCall().getThis() === window);
}

testSuite({
  setUpPage() {
    state.real = {setTimeout, setInterval, requestAnimationFrame};
  },
  setUp() {
    state.fake = {
      setTimeout: recordFunction(state.real.setTimeout.bind(null)),
      // Here we mock `setInterval` with `setTimeout` as we only want it to
      // run once.
      setInterval: recordFunction(state.real.setTimeout.bind(null)),
      requestAnimationFrame: recordFunction(state.real.setTimeout.bind(null)),
    };

    window.setTimeout = state.fake.setTimeout;
    window.setInterval = state.fake.setInterval;
    window.requestAnimationFrame = state.fake.requestAnimationFrame;

    mockControl = new MockControl();

    // On IE, globalEval happens async. So make it synchronous.
    goog.globalEval = (str) => {
      eval(str);
    };

    // just record the exception in the error handler when it happens
    errorHandler = new ErrorHandler(function(ex) {
      this.ex = ex;
    });
  },

  tearDown() {
    // Clean our state.
    delete state.lastUncaughtError;
    state.fake = {};

    // Clean our window.
    window.setTimeout = state.real.setTimeout;
    window.setInterval = state.real.setInterval;
    window.requestAnimationFrame = state.real.requestAnimationFrame;

    // Tear down and dispose.
    mockControl.$tearDown();
    dispose(errorHandler);
    errorHandler = null;
  },

  async testWrapSetTimeout() {
    TestCase.getActiveTestCase().promiseTimeout = 10000;

    errorHandler.protectWindowSetTimeout();

    const resolver = new NativeResolver();
    window.setTimeout(errorThrowerFactory(resolver.resolve), 30);
    await resolver.promise;

    assertMethodCalledHelper('setTimeout');
  },

  testWrapSetTimeoutWithoutException() {
    errorHandler.protectWindowSetTimeout();

    window.setTimeout((x, y) => {
      assertEquals('test', x);
      assertEquals(7, y);
    }, 3, 'test', 7);
  },

  async testWrapSetTimeoutWithString() {
    errorHandler.protectWindowSetTimeout();

    const resolver = new NativeResolver();
    const errorThrower = errorThrowerFactory(resolver.resolve);
    /** @suppress {strictMissingProperties} */  // We want the `errorThrower` to
                                                // be readable by the `eval`ed
                                                // string below.
    window.errorThrower = errorThrower;
    window.setTimeout('window.errorThrower()', 3);
    await resolver.promise;
    delete window.errorThrower;

    assertMethodCalledHelper('setTimeout');
  },

  async testWrapSetInterval() {
    errorHandler.protectWindowSetInterval();

    const resolver = new NativeResolver();
    window.setInterval(errorThrowerFactory(resolver.resolve), 30);
    await resolver.promise;

    assertMethodCalledHelper('setInterval');
  },

  testWrapSetIntervalWithoutException() {
    errorHandler.protectWindowSetInterval();

    window.setInterval((x, y) => {
      assertEquals('test', x);
      assertEquals(7, y);
    }, 3, 'test', 7);
  },

  async testWrapSetIntervalWithString() {
    errorHandler.protectWindowSetInterval();

    const resolver = new NativeResolver();
    const errorThrower = errorThrowerFactory(resolver.resolve);
    /** @suppress {strictMissingProperties} */  // We want the `errorThrower` to
                                                // be readable by the `eval`ed
                                                // string below.
    window.errorThrower = errorThrower;
    window.setInterval('window.errorThrower()', 3);
    await resolver.promise;
    delete window.errorThrower;

    assertMethodCalledHelper('setInterval');
  },

  async testWrapRequestAnimationFrame() {
    errorHandler.protectWindowRequestAnimationFrame();

    const resolver = new NativeResolver();
    window.requestAnimationFrame(errorThrowerFactory(resolver.resolve));
    await resolver.promise;

    assertMethodCalledHelper('requestAnimationFrame');
  },

  testDisposal() {
    errorHandler.protectWindowSetTimeout();
    errorHandler.protectWindowSetInterval();

    assertNotEquals(window.setTimeout, state.fake.setTimeout);
    assertNotEquals(window.setInterval, state.fake.setInterval);

    errorHandler.dispose();

    assertEquals(window.setTimeout, state.fake.setTimeout);
    assertEquals(window.setInterval, state.fake.setInterval);
  },

  testUnwrap() {
    const fn = () => {};
    const wrappedFn = errorHandler.wrap(fn);
    assertNotEquals(wrappedFn, fn);

    assertEquals(fn, errorHandler.unwrap(fn));
    assertEquals(fn, errorHandler.unwrap(wrappedFn));
  },

  testStackPreserved() {
    let e;
    let hasStacks;
    function specialFunctionName() {
      const e = Error();
      hasStacks = !!e.stack;
      throw e;
    }
    const wrappedFn = errorHandler.wrap(specialFunctionName);
    try {
      wrappedFn();
    } catch (exception) {
      e = exception;
    }
    assertTrue(!!e);
    if (hasStacks) {
      assertContains('specialFunctionName', e.stack);
    }
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testGetProtectedFunction() {
    const fn = () => {
      throw new Error('Foo');
    };
    /** @suppress {visibility} suppression added to enable type checking */
    const protectedFn = errorHandler.getProtectedFunction(fn);
    const e = assertThrows(protectedFn);
    assertTrue(e instanceof ErrorHandler.ProtectedFunctionError);
    assertEquals('Foo', e.cause.message);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testGetProtectedFunctionNullError() {
    const fn = () => {
      throw null;
    };
    /** @suppress {visibility} suppression added to enable type checking */
    const protectedFn = errorHandler.getProtectedFunction(fn);
    const e = assertThrows(protectedFn);
    assertTrue(e instanceof ErrorHandler.ProtectedFunctionError);
    assertNull(e.cause);
  },

  testGetProtectedFunction_withoutWrappedErrors() {
    const shouldCallErrorLog = !!Error.captureStackTrace;
    if (shouldCallErrorLog) {
      mockControl.createMethodMock(globalThis.console, 'error');
    }
    errorHandler.setWrapErrors(false);
    const fn = () => {
      const e = new Error('Foo');
      e.stack = 'STACK';
      throw e;
    };
    /** @suppress {visibility} suppression added to enable type checking */
    const protectedFn = errorHandler.getProtectedFunction(fn);
    if (shouldCallErrorLog) {
      globalThis.console.error('Foo', 'STACK');
    }
    mockControl.$replayAll();
    const e = assertThrows(protectedFn);
    mockControl.$verifyAll();
    assertTrue(e instanceof Error);
    assertEquals('Foo', e.message);
    assertEquals(e.stack, 'STACK');
  },

  testGetProtectedFunction_withoutWrappedErrorsWithMessagePrefix() {
    errorHandler.setWrapErrors(false);
    errorHandler.setPrefixErrorMessages(true);
    const fn = () => {
      throw new Error('Foo');
    };
    /** @suppress {visibility} suppression added to enable type checking */
    let protectedFn = errorHandler.getProtectedFunction(fn);
    let e = assertThrows(protectedFn);
    assertTrue(e instanceof Error);
    assertEquals(
        ErrorHandler.ProtectedFunctionError.MESSAGE_PREFIX + 'Foo', e.message);

    const stringError = () => {
      throw 'String';
    };
    /** @suppress {visibility} suppression added to enable type checking */
    protectedFn = errorHandler.getProtectedFunction(stringError);
    e = assertThrows(protectedFn);
    assertEquals('string', typeof e);
    assertEquals(
        ErrorHandler.ProtectedFunctionError.MESSAGE_PREFIX + 'String', e);
  },

  async testProtectedFunction_infiniteLoop() {
    let numErrors = 0;
    const errorHandler = new ErrorHandler((ex) => {
      numErrors++;
    });
    errorHandler.protectWindowSetTimeout();

    const resolver = new NativeResolver();
    window.setTimeout(() => {
      window.setTimeout(errorThrowerFactory(resolver.resolve), 3);
    }, 3);
    await resolver.promise;

    assertEquals(
        'Error handler should only have been executed once.', 1, numErrors);
  },
});
