/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.TestCaseTest');
goog.setTestOnly();

const ExpectedFailures = goog.require('goog.testing.ExpectedFailures');
const FunctionMock = goog.require('goog.testing.FunctionMock');
const GoogPromise = goog.require('goog.Promise');
const JsUnitException = goog.require('goog.testing.JsUnitException');
const MethodMock = goog.require('goog.testing.MethodMock');
const MockRandom = goog.require('goog.testing.MockRandom');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const TestCase = goog.require('goog.testing.TestCase');
const Timer = goog.require('goog.Timer');
const functions = goog.require('goog.functions');
const googString = goog.require('goog.string');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

// Dual of fail().
const ok = () => {
  assertTrue(true);
};

// Native Promise-based equivalent of ok().
const okPromise = () => Promise.resolve(null);

// Native Promise-based equivalent of fail().
const failPromise = () => Promise.reject(null);

// Native Promise-based test that returns promise which never resolves.
const neverResolvedPromise = () => new Promise(() => {});

// goog.Promise-based equivalent of ok().
const okGoogPromise = () => GoogPromise.resolve(null);

// goog.Promise-based equivalent of fail().
const failGoogPromise = () => GoogPromise.reject(new Error());

// Native Promise-based test that returns promise which never resolves.
const neverResolvedGoogPromise = () => new GoogPromise(() => {});

/** @type {!Array<string>} */
let events;

/**
 * @param {string} name
 * @return {function()}
 */
function event(name) {
  return () => {
    events.push(name);
  };
}

/**
 * Verifies that:
 * <ol>
 * <li>when the `failOnUnreportedAsserts` flag is disabled, the test
 *     function passes;
 * <li>when the `failOnUnreportedAsserts` flag is enabled, the test
 *     function passes if `shouldPassWithFlagEnabled` is true and fails if
 *     it is false; and that
 * <li>when the `failOnUnreportedAsserts` flag is enabled, and in addition
 *     `invalidateAssertionException` is stubbed out to do nothing, the
 *     test function fails.
 * </ol>
 * @param {boolean} shouldPassWithFlagEnabled
 * @param {function(): !GoogPromise} testFunction
 * @return {!GoogPromise}
 */
function verifyTestOutcomeForFailOnUnreportedAssertsFlag(
    shouldPassWithFlagEnabled, testFunction) {
  return verifyWithFlagEnabledAndNoInvalidation(testFunction).then(function() {
    return verifyWithFlagEnabled(testFunction, shouldPassWithFlagEnabled);
  });
}

function verifyWithFlagEnabled(testFunction, shouldPassWithFlagEnabled) {
  // With the flag enabled, the test is expected to pass if shouldPassWithFlag
  // is true, and fail if shouldPassWithFlag is false.
  const testCase = new TestCase();
  const getTestCase = functions.constant(testCase);
  testCase.addNewTest('test', testFunction);

  const stubs = new PropertyReplacer();
  stubs.replace(window, '_getCurrentTestCase', getTestCase);
  stubs.replace(TestCase, 'getActiveTestCase', getTestCase);

  const promise =
      new GoogPromise((resolve, reject) => {
        testCase.addCompletedCallback(resolve);
      })
          .then(() => {
            assertEquals(shouldPassWithFlagEnabled, testCase.isSuccess());
            const result = testCase.getResult();
            assertTrue(result.complete);
            // Expect both the caught assertion and the failOnUnreportedAsserts
            // error.
            assertEquals(
                shouldPassWithFlagEnabled ? 0 : 2, result.errors.length);
          })
          .thenAlways(() => {
            stubs.reset();
          });

  testCase.runTests();
  return promise;
}

function verifyWithFlagEnabledAndNoInvalidation(testFunction) {
  // With the flag enabled, the test is expected to pass if shouldPassWithFlag
  // is true, and fail if shouldPassWithFlag is false.
  const testCase = new TestCase();
  const getTestCase = functions.constant(testCase);
  testCase.addNewTest('test', testFunction);

  const stubs = new PropertyReplacer();
  stubs.replace(window, '_getCurrentTestCase', getTestCase);
  stubs.replace(TestCase, 'getActiveTestCase', getTestCase);
  stubs.replace(
      TestCase.prototype, 'invalidateAssertionException', goog.nullFunction);

  const promise = new GoogPromise((resolve, reject) => {
                    testCase.addCompletedCallback(resolve);
                  })
                      .then(() => {
                        assertFalse(testCase.isSuccess());
                        const result = testCase.getResult();
                        assertTrue(result.complete);
                        // Expect both the caught assertion and the
                        // failOnUnreportedAsserts error.
                        assertEquals(2, result.errors.length);
                      })
                      .thenAlways(() => {
                        stubs.reset();
                      });

  testCase.runTests();
  return promise;
}

let testDoneTestsSeen = [];
let testDoneErrorsSeen = {};
let testDoneRuntime = {};
/**
 * @param {TestCase} test
 * @param {Array<string>} errors
 * @suppress {missingProperties} suppression added to enable type checking
 */
function storeCallsAndErrors(test, errors) {
  testDoneTestsSeen.push(test.name);
  /** @suppress {missingProperties} suppression added to enable type checking */
  testDoneErrorsSeen[test.name] = [];
  for (let i = 0; i < errors.length; i++) {
    testDoneErrorsSeen[test.name].push(errors[i].split('\n')[0]);
  }
  /** @suppress {missingProperties} suppression added to enable type checking */
  testDoneRuntime[test.name] = test.getElapsedTime();
}
/**
 * @param {Array<TestCase>} expectedTests
 * @param {Array<Array<string>>} expectedErrors
 */
function assertStoreCallsAndErrors(expectedTests, expectedErrors) {
  assertArrayEquals(expectedTests, testDoneTestsSeen);
  for (let i = 0; i < expectedTests.length; i++) {
    const name = expectedTests[i];
    assertArrayEquals(expectedErrors, testDoneErrorsSeen[name]);
    assertEquals(typeof testDoneRuntime[testDoneTestsSeen[i]], 'number');
  }
}

/**
 * A global test function used by `testInitializeTestCase`.
 * @suppress {strictMissingProperties} suppression added to enable type checking
 */
globalThis.mockTestName = function() {
  return failGoogPromise();
};

/**
 * A variable with the same autodiscovery prefix, used by
 * `testInitializeTestCase`. TestCase does not support auto-discovering tests
 * within Arrays, either as functions added to the array object or as a value
 * within the array (as it never recurses into the content of the array).
 * @suppress {strictMissingProperties} suppression added to enable type checking
 */
globalThis.mockTestNameValues = ['hello', 'world'];
/**
 * @return {!GoogPromise<?>}
 * @suppress {strictMissingProperties} suppression added to enable type checking
 */
globalThis.mockTestNameValues.mockTestNameTestShouldNotBeRun = function() {
  return failGoogPromise();
};

testSuite({
  setUp() {
    events = [];
  },

  testEmptyTestCase() {
    const testCase = new TestCase();
    testCase.runTests();
    assertTrue(testCase.isSuccess());
    const result = testCase.getResult();
    assertTrue(result.complete);
    assertEquals(0, result.totalCount);
    assertEquals(0, result.runCount);
    assertEquals(0, result.successCount);
    assertEquals(0, result.errors.length);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testCompletedCallbacks() {
    const callback = FunctionMock('completed');
    const testCase = new TestCase();

    testCase.addCompletedCallback(callback);
    testCase.addCompletedCallback(callback);

    callback().$times(2);

    callback.$replay();
    testCase.runTests();
    callback.$verify();
    callback.$reset();

    assertTrue(testCase.isSuccess());

    // Executing a second time should not remember the callback.
    callback.$replay();
    testCase.runTests();
    callback.$verify();
  },

  testEmptyTestCaseReturningPromise() {
    return new TestCase().runTestsReturningPromise().then((result) => {
      assertTrue(result.complete);
      assertEquals(0, result.totalCount);
      assertEquals(0, result.runCount);
      assertEquals(0, result.successCount);
      assertEquals(0, result.errors.length);
    });
  },

  testTestCase_SyncSuccess() {
    const testCase = new TestCase();
    testCase.addNewTest('foo', ok);
    testCase.runTests();
    assertTrue(testCase.isSuccess());
    const result = testCase.getResult();
    assertTrue(result.complete);
    assertEquals(1, result.totalCount);
    assertEquals(1, result.runCount);
    assertEquals(1, result.successCount);
    assertEquals(0, result.errors.length);
  },

  testTestCaseReturningPromise_SyncSuccess() {
    const testCase = new TestCase();
    testCase.addNewTest('foo', ok);
    return testCase.runTestsReturningPromise().then((result) => {
      assertTrue(result.complete);
      assertEquals(1, result.totalCount);
      assertEquals(1, result.runCount);
      assertEquals(1, result.successCount);
      assertEquals(0, result.errors.length);
    });
  },

  testTestCaseReturningPromise_GoogPromiseResolve() {
    const testCase = new TestCase();
    testCase.addNewTest('foo', okGoogPromise);
    return testCase.runTestsReturningPromise().then((result) => {
      assertTrue(result.complete);
      assertEquals(1, result.totalCount);
      assertEquals(1, result.runCount);
      assertEquals(1, result.successCount);
      assertEquals(0, result.errors.length);
    });
  },

  testTestCaseReturningPromise_PromiseResolve() {
    if (!('Promise' in globalThis)) {
      return;
    }
    const testCase = new TestCase();
    testCase.addNewTest('foo', okPromise);
    return testCase.runTestsReturningPromise().then((result) => {
      assertTrue(result.complete);
      assertEquals(1, result.totalCount);
      assertEquals(1, result.runCount);
      assertEquals(1, result.successCount);
      assertEquals(0, result.errors.length);
    });
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testTestCase_DoubleFailure() {
    let doneCount = 0;
    const testCase = new TestCase();

    testCase.setTestDoneCallback(() => {
      doneCount++;
    });

    testCase.addNewTest('foo', fail, null, [{tearDown: fail}]);
    testCase.runTests();
    assertFalse(testCase.isSuccess());
    const result = testCase.getResult();
    assertTrue(result.complete);
    assertEquals(1, result.totalCount);
    assertEquals(1, result.runCount);
    assertEquals(0, result.successCount);
    assertEquals(2, result.errors.length);

    assertEquals('testDone must be called exactly once.', 1, doneCount);

    // Make sure we strip all TestCase stack frames:
    assertNotContains('testcase.js', result.errors[0].toString());
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testTestCase_RepeatedFailure() {
    const stubs = new PropertyReplacer();
    // Prevent the mock from the inner test from forcibly failing the outer
    // test.
    stubs.replace(
        globalThis, 'G_testRunner', null, true /* opt_allowNullOrUndefined */);

    let doneCount;
    let testCase;
    try {
      doneCount = 0;
      testCase = new TestCase();

      testCase.setTestDoneCallback(() => {
        doneCount++;
      });

      const mock = FunctionMock();
      testCase.addNewTest(
          'foo', /**
                    @suppress {checkTypes} suppression added to enable type
                    checking
                  */
          () => {
            mock(1).$once();
            mock.$replay();
            // This throws a bad-parameter exception immediately.
            mock(2);
          },
          null, [{
            // This throws the recorded exception again.
            // Calling this in tearDown is a common pattern (eg, in
            // Environment).
            tearDown: goog.bind(mock.$verify, mock),
          }]);
      testCase.runTests();
    } finally {
      stubs.reset();
    }
    assertFalse(testCase.isSuccess());
    const result = testCase.getResult();
    assertTrue(result.complete);
    assertEquals(1, result.totalCount);
    assertEquals(1, result.runCount);
    assertEquals(0, result.successCount);
    assertEquals(1, result.errors.length);

    assertEquals('testDone must be called exactly once.', 1, doneCount);

    // Make sure we strip all TestCase stack frames:
    assertNotContains('testcase.js', result.errors[0].toString());
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testTestCase_SyncFailure() {
    const testCase = new TestCase();
    testCase.addNewTest('foo', fail);
    testCase.runTests();
    assertFalse(testCase.isSuccess());
    const result = testCase.getResult();
    assertTrue(result.complete);
    assertEquals(1, result.totalCount);
    assertEquals(1, result.runCount);
    assertEquals(0, result.successCount);
    assertEquals(1, result.errors.length);
    assertEquals('foo', result.errors[0].source);

    // Make sure we strip all TestCase stack frames:
    assertNotContains('testcase.js', result.errors[0].toString());
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testTestCaseReturningPromise_SyncFailure() {
    const testCase = new TestCase();
    testCase.addNewTest('foo', fail);
    return testCase.runTestsReturningPromise().then((result) => {
      assertFalse(testCase.isSuccess());
      assertTrue(result.complete);
      assertEquals(1, result.totalCount);
      assertEquals(1, result.runCount);
      assertEquals(0, result.successCount);
      assertEquals(1, result.errors.length);
      assertEquals('foo', result.errors[0].source);

      // Make sure we strip all TestCase stack frames:
      assertNotContains('testcase.js', result.errors[0].toString());
    });
  },

  testTestCaseReturningPromise_GoogPromiseReject() {
    const testCase = new TestCase();
    testCase.addNewTest('foo', failGoogPromise);
    return testCase.runTestsReturningPromise().then((result) => {
      assertFalse(testCase.isSuccess());
      assertTrue(result.complete);
      assertEquals(1, result.totalCount);
      assertEquals(1, result.runCount);
      assertEquals(0, result.successCount);
      assertEquals(1, result.errors.length);
      assertEquals('foo', result.errors[0].source);

      // Make sure we strip all TestCase stack frames:
      assertNotContains('testcase.js', result.errors[0].toString());
    });
  },

  testTestCaseReturningPromise_GoogPromiseTimeout() {
    const testCase = new TestCase();
    testCase.addNewTest('foo', neverResolvedGoogPromise);
    let startTimestamp = new Date().getTime();
    // We have to decrease timeout for the artificial 'foo' test otherwise
    // current test will timeout.
    testCase.promiseTimeout = 500;
    startTimestamp = new Date().getTime();
    return testCase.runTestsReturningPromise().then((result) => {
      const elapsedTime = new Date().getTime() - startTimestamp;
      assertFalse(testCase.isSuccess());
      assertTrue(result.complete);
      assertEquals(1, result.totalCount);
      assertEquals(1, result.runCount);
      assertEquals(0, result.successCount);
      assertEquals(1, result.errors.length);
      // Check that error message mentions test name.
      assertContains('foo', result.errors[0].toString());
      // Check that error message mentions how to change timeout.
      assertContains(
          'goog.testing.TestCase.getActiveTestCase().promiseTimeout',
          result.errors[0].toString());
      if (!userAgent.EDGE_OR_IE) {
        assertTrue(
            `Expected ${elapsedTime} to be >= ${testCase.promiseTimeout}.`,
            elapsedTime >= testCase.promiseTimeout);
      }
    });
  },

  testTestCaseReturningPromise_PromiseReject() {
    if (!('Promise' in globalThis)) {
      return;
    }
    const testCase = new TestCase();
    testCase.addNewTest('foo', failPromise);
    return testCase.runTestsReturningPromise().then((result) => {
      assertFalse(testCase.isSuccess());
      assertTrue(result.complete);
      assertEquals(1, result.totalCount);
      assertEquals(1, result.runCount);
      assertEquals(0, result.successCount);
      assertEquals(1, result.errors.length);
      assertEquals('foo', result.errors[0].source);
    });
  },

  testTestCaseReturningPromise_PromiseTimeout() {
    if (!('Promise' in globalThis)) {
      return;
    }
    const testCase = new TestCase();
    testCase.addNewTest('foo', neverResolvedPromise);
    // We have to decrease timeout for the artificial 'foo' test otherwise
    // current test will timeout.
    testCase.promiseTimeout = 500;
    const startTimestamp = new Date().getTime();
    return testCase.runTestsReturningPromise().then((result) => {
      const elapsedTime = new Date().getTime() - startTimestamp;
      assertFalse(testCase.isSuccess());
      assertTrue(result.complete);
      assertEquals(1, result.totalCount);
      assertEquals(1, result.runCount);
      assertEquals(0, result.successCount);
      assertEquals(1, result.errors.length);
      // Check that error message mentions test name.
      assertContains('foo', result.errors[0].toString());
      // Check that error message mentions how to change timeout.
      assertContains(
          'goog.testing.TestCase.getActiveTestCase().promiseTimeout',
          result.errors[0].toString());
      if (!userAgent.EDGE_OR_IE) {
        assertTrue(
            `Expected ${elapsedTime} to be >= ${testCase.promiseTimeout}.`,
            elapsedTime >= testCase.promiseTimeout);
      }
    });
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testTestCase_SyncSuccess_SyncFailure() {
    const testCase = new TestCase();
    testCase.addNewTest('foo', ok);
    testCase.addNewTest('bar', fail);
    testCase.runTests();
    assertFalse(testCase.isSuccess());
    const result = testCase.getResult();
    assertTrue(result.complete);
    assertEquals(2, result.totalCount);
    assertEquals(2, result.runCount);
    assertEquals(1, result.successCount);
    assertEquals(1, result.errors.length);
    assertEquals('bar', result.errors[0].source);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testTestCaseReturningPromise_SyncSuccess_SyncFailure() {
    const testCase = new TestCase();
    testCase.addNewTest('foo', ok);
    testCase.addNewTest('bar', fail);
    return testCase.runTestsReturningPromise().then((result) => {
      assertTrue(result.complete);
      assertEquals(2, result.totalCount);
      assertEquals(2, result.runCount);
      assertEquals(1, result.successCount);
      assertEquals(1, result.errors.length);
      assertEquals('bar', result.errors[0].source);
    });
  },

  testTestCaseReturningPromise_GoogPromiseResolve_GoogPromiseReject() {
    const testCase = new TestCase();
    testCase.addNewTest('foo', okGoogPromise);
    testCase.addNewTest('bar', failGoogPromise);
    return testCase.runTestsReturningPromise().then((result) => {
      assertTrue(result.complete);
      assertEquals(2, result.totalCount);
      assertEquals(2, result.runCount);
      assertEquals(1, result.successCount);
      assertEquals(1, result.errors.length);
      assertEquals('bar', result.errors[0].source);

      // Make sure we strip all TestCase stack frames:
      assertNotContains('testcase.js', result.errors[0].toString());
    });
  },

  testTestCaseReturningPromise_PromiseResolve_PromiseReject() {
    if (!('Promise' in globalThis)) {
      return;
    }
    const testCase = new TestCase();
    testCase.addNewTest('foo', okPromise);
    testCase.addNewTest('bar', failPromise);
    return testCase.runTestsReturningPromise().then((result) => {
      assertTrue(result.complete);
      assertEquals(2, result.totalCount);
      assertEquals(2, result.runCount);
      assertEquals(1, result.successCount);
      assertEquals(1, result.errors.length);
      assertEquals('bar', result.errors[0].source);
    });
  },

  testTestCaseReturningPromise_PromiseResolve_GoogPromiseReject() {
    if (!('Promise' in globalThis)) {
      return;
    }
    const testCase = new TestCase();
    testCase.addNewTest('foo', okPromise);
    testCase.addNewTest('bar', failGoogPromise);
    return testCase.runTestsReturningPromise().then((result) => {
      assertTrue(result.complete);
      assertEquals(2, result.totalCount);
      assertEquals(2, result.runCount);
      assertEquals(1, result.successCount);
      assertEquals(1, result.errors.length);
      assertEquals('bar', result.errors[0].source);
    });
  },

  testTestCaseReturningPromise_GoogPromiseResolve_PromiseReject() {
    if (!('Promise' in globalThis)) {
      return;
    }
    const testCase = new TestCase();
    testCase.addNewTest('foo', okGoogPromise);
    testCase.addNewTest('bar', failPromise);
    return testCase.runTestsReturningPromise().then((result) => {
      assertTrue(result.complete);
      assertEquals(2, result.totalCount);
      assertEquals(2, result.runCount);
      assertEquals(1, result.successCount);
      assertEquals(1, result.errors.length);
      assertEquals('bar', result.errors[0].source);
    });
  },

  testTestCaseReturningPromise_PromisesInSetUpAndTest() {
    if (!('Promise' in globalThis)) {
      return;
    }
    const testCase = new TestCase();
    /** @suppress {checkTypes} suppression added to enable type checking */
    testCase.setUpPage = () => {
      event('setUpPage-called')();
      return Timer.promise().then(() => {
        event('setUpPage-promiseFinished')();
      });
    };
    /** @suppress {checkTypes} suppression added to enable type checking */
    testCase.setUp = () => {
      event('setUp-called')();
      return Timer.promise().then(() => {
        event('setUp-promiseFinished')();
      });
    };
    testCase.addNewTest(
        'foo', /**
                  @suppress {checkTypes} suppression added to enable type
                  checking
                */
        () => {
          event('foo-called')();
          return Timer.promise().then(() => {
            event('foo-promiseFinished')();
          });
        });

    // Initially only setUpPage should have been called.
    return testCase.runTestsReturningPromise().then((result) => {
      assertTrue(result.complete);
      assertEquals(1, result.totalCount);
      assertEquals(1, result.runCount);
      assertEquals(1, result.successCount);
      assertEquals(0, result.errors.length);

      assertArrayEquals(
          [
            'setUpPage-called',
            'setUpPage-promiseFinished',
            'setUp-called',
            'setUp-promiseFinished',
            'foo-called',
            'foo-promiseFinished',
          ],
          events);
    });
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testTestCaseNeverRun() {
    const testCase = new TestCase();
    testCase.addNewTest('foo', fail);
    // Missing testCase.runTests()
    const result = testCase.getResult();
    assertFalse(result.complete);
    assertEquals(0, result.totalCount);
    assertEquals(0, result.runCount);
    assertEquals(0, result.successCount);
    assertEquals(0, result.errors.length);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testParseRunTests() {
    assertNull(TestCase.parseRunTests_('file://hello.html'));
    assertNull(TestCase.parseRunTests_('https://example.com?runTests='));
    assertObjectEquals(
        {'testOne': true},
        TestCase.parseRunTests_('https://example.com?runTests=testOne'));
    assertObjectEquals(
        {'testOne': true, 'testTwo': true},
        TestCase.parseRunTests_(
            'https://example.com?foo=bar&runTests=testOne,testTwo'));
    assertObjectEquals(
        {
          '1': true,
          '2': true,
          '3': true,
          'testShouting': true,
          'TESTSHOUTING': true,
        },
        TestCase.parseRunTests_(
            'https://example.com?RUNTESTS=testShouting,TESTSHOUTING,1,2,3'));
  },

  /**
     @suppress {checkTypes,visibility} suppression added to enable type
     checking
   */
  testSortOrder_natural() {
    const testCase = new TestCase();
    testCase.setOrder('natural');

    let testIndex = 0;
    testCase.addNewTest('test_c', () => {
      assertEquals(0, testIndex++);
    });
    testCase.addNewTest('test_a', () => {
      assertEquals(1, testIndex++);
    });
    testCase.addNewTest('test_b', () => {
      assertEquals(2, testIndex++);
    });
    testCase.orderTests_();
    testCase.runTests();

    assertTrue(testCase.isSuccess());
    const result = testCase.getResult();
    assertEquals(3, result.totalCount);
    assertEquals(3, result.runCount);
    assertEquals(3, result.successCount);
    assertEquals(0, result.errors.length);
  },

  /**
     @suppress {checkTypes,visibility} suppression added to enable type
     checking
   */
  testSortOrder_random() {
    const testCase = new TestCase();
    testCase.setOrder('random');

    let testIndex = 0;
    testCase.addNewTest('test_c', () => {
      assertEquals(0, testIndex++);
    });
    testCase.addNewTest('test_a', () => {
      assertEquals(2, testIndex++);
    });
    testCase.addNewTest('test_b', () => {
      assertEquals(1, testIndex++);
    });

    const mockRandom = new MockRandom([0.5, 0.5]);
    mockRandom.install();
    try {
      testCase.orderTests_();
    } finally {
      // Avoid using a global tearDown() for cleanup, since all TestCase
      // instances auto-detect and share the global life cycle functions.
      mockRandom.uninstall();
    }

    testCase.runTests();

    assertTrue(testCase.isSuccess());
    const result = testCase.getResult();
    assertEquals(3, result.totalCount);
    assertEquals(3, result.runCount);
    assertEquals(3, result.successCount);
    assertEquals(0, result.errors.length);
  },

  /**
     @suppress {checkTypes,visibility} suppression added to enable type
     checking
   */
  testSortOrder_sorted() {
    const testCase = new TestCase();
    testCase.setOrder('sorted');

    let testIndex = 0;
    testCase.addNewTest('test_c', () => {
      assertEquals(2, testIndex++);
    });
    testCase.addNewTest('test_a', () => {
      assertEquals(0, testIndex++);
    });
    testCase.addNewTest('test_b', () => {
      assertEquals(1, testIndex++);
    });
    testCase.orderTests_();
    testCase.runTests();

    assertTrue(testCase.isSuccess());
    const result = testCase.getResult();
    assertEquals(3, result.totalCount);
    assertEquals(3, result.runCount);
    assertEquals(3, result.successCount);
    assertEquals(0, result.errors.length);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testRunTests() {
    const testCase = new TestCase();
    testCase.setTestsToRun({'test_a': true, 'test_c': true});

    let testIndex = 0;
    testCase.addNewTest('test_c', () => {
      assertEquals(0, testIndex++);
    });
    testCase.addNewTest('test_a', () => {
      assertEquals(1, testIndex++);
    });
    testCase.addNewTest('test_b', fail);
    testCase.runTests();

    assertTrue(testCase.isSuccess());
    const result = testCase.getResult();
    assertEquals(3, result.totalCount);
    assertEquals(2, result.runCount);
    assertEquals(2, result.successCount);
    assertEquals(0, result.errors.length);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testRunTests_byIndex() {
    const testCase = new TestCase();
    testCase.setTestsToRun({'0': true, '2': true});

    let testIndex = 0;
    testCase.addNewTest('test_c', () => {
      assertEquals(0, testIndex++);
    });
    testCase.addNewTest('test_a', fail);
    testCase.addNewTest('test_b', () => {
      assertEquals(1, testIndex++);
    });
    testCase.runTests();

    assertTrue(testCase.isSuccess());
    const result = testCase.getResult();
    assertEquals(3, result.totalCount);
    assertEquals(2, result.runCount);
    assertEquals(2, result.successCount);
    assertEquals(0, result.errors.length);
  },

  testMaybeFailTestEarly() {
    const message = 'Error in setUpPage().';
    const testCase = new TestCase();
    testCase.setUpPage = () => {
      throw new Error(message);
    };
    testCase.addNewTest('test', ok);
    testCase.runTests();
    assertFalse(testCase.isSuccess());
    const errors = testCase.getResult().errors;
    assertEquals(1, errors.length);
    assertContains(message, errors[0].toString());
  },

  testSetUpReturnsPromiseThatTimesOut() {
    const testCase = new TestCase();
    testCase.promiseTimeout = 500;
    testCase.setUp = neverResolvedGoogPromise;
    testCase.addNewTest('test', ok);
    return testCase.runTestsReturningPromise().then((result) => {
      assertFalse(testCase.isSuccess());
      assertTrue(result.complete);
      assertEquals(1, result.errors.length);
      assertContains('setUp', result.errors[0].toString());
    });
  },

  testTearDownReturnsPromiseThatTimesOut() {
    const testCase = new TestCase();
    testCase.promiseTimeout = 500;
    testCase.tearDown = neverResolvedGoogPromise;
    testCase.addNewTest('test', ok);
    return testCase.runTestsReturningPromise().then((result) => {
      assertFalse(testCase.isSuccess());
      assertTrue(result.complete);
      assertEquals(1, result.errors.length);
      assertContains('tearDown', result.errors[0].toString());
    });
  },

  testTearDown_complexJsUnitExceptionIssue() {  // http://b/110796519
    const testCase = new TestCase();

    const getTestCase = functions.constant(testCase);
    const stubs = new PropertyReplacer();
    stubs.replace(window, '_getCurrentTestCase', getTestCase);
    stubs.replace(TestCase, 'getActiveTestCase', getTestCase);

    testCase.tearDown = () => {
      try {
        fail('First error');
      } catch (e1) {
        try {
          fail('Second error');
        } catch (e2) {
        }
        throw e1;
      }
    };
    testCase.addNewTest('test', ok);
    return testCase.runTestsReturningPromise().then((result) => {
      assertFalse(testCase.isSuccess());
      assertTrue(result.complete);

      assertNotEquals(
          'Expect the failure to be associated with the test.', 0,
          result.resultsByName['test'].length);

      assertEquals(2, result.errors.length);
      assertContains('tearDown', result.errors[0].toString());

      assertContains('First error', result.errors[1].toString());
      assertContains('Second error', result.errors[0].toString());
    });
  },

  testFailOnUnreportedAsserts_SwallowedException() {
    return verifyTestOutcomeForFailOnUnreportedAssertsFlag(
        false, /**
                  @suppress {missingReturn} suppression added to enable type
                  checking
                */
        () => {
          try {
            assertTrue(false);
          } catch (e) {
            // Swallow the exception generated by the assertion.
          }
        });
  },

  testFailOnUnreportedAsserts_SwallowedFail() {
    return verifyTestOutcomeForFailOnUnreportedAssertsFlag(
        false, /**
                  @suppress {missingReturn,checkTypes} suppression added to
                  enable type checking
                */
        () => {
          try {
            fail();
          } catch (e) {
            // Swallow the exception generated by fail.
          }
        });
  },

  testFailOnUnreportedAsserts_SwallowedAssertThrowsException() {
    return verifyTestOutcomeForFailOnUnreportedAssertsFlag(
        false, /**
                  @suppress {missingReturn} suppression added to enable type
                  checking
                */
        () => {
          try {
            assertThrows(goog.nullFunction);
          } catch (e) {
            // Swallow the exception generated by assertThrows.
          }
        });
  },

  testFailOnUnreportedAsserts_SwallowedAssertNotThrowsException() {
    return verifyTestOutcomeForFailOnUnreportedAssertsFlag(
        false, /**
                  @suppress {missingReturn,checkTypes} suppression added to
                  enable type checking
                */
        () => {
          try {
            assertNotThrows(functions.error());
          } catch (e) {
            // Swallow the exception generated by assertNotThrows.
          }
        });
  },

  testFailOnUnreportedAsserts_SwallowedExceptionViaPromise() {
    return verifyTestOutcomeForFailOnUnreportedAssertsFlag(
        false,
        () => GoogPromise.resolve()
                  .then(() => {
                    assertTrue(false);
                  })
                  .thenCatch(
                      (e) => {
                          // Swallow the exception generated by the assertion.
                      }));
  },

  testFailOnUnreportedAsserts_NotForAssertThrowsJsUnitException() {
    return verifyTestOutcomeForFailOnUnreportedAssertsFlag(
        true, /**
                 @suppress {missingReturn} suppression added to enable type
                 checking
               */
        () => {
          assertThrowsJsUnitException(() => {
            assertTrue(false);
          });
        });
  },

  testFailOnUnreportedAsserts_NotForExpectedFailures() {
    return verifyTestOutcomeForFailOnUnreportedAssertsFlag(
        true, /**
                 @suppress {missingReturn} suppression added to enable type
                 checking
               */
        () => {
          const expectedFailures = new ExpectedFailures();
          expectedFailures.expectFailureFor(true);
          try {
            assertTrue(false);
          } catch (e) {
            expectedFailures.handleException(e);
          }
        });
  },

  /**
     @suppress {checkTypes,visibility,missingProperties} suppression added to
     enable type checking
   */
  testFailOnUnreportedAsserts_ReportUnpropagatedAssertionExceptions() {
    const testCase = new TestCase();

    const e1 = new JsUnitException('foo123');
    const e2 = new JsUnitException('bar456');

    const mockRecordError = MethodMock(testCase, 'recordError');
    mockRecordError('test', e1);
    mockRecordError('test', e2);
    mockRecordError.$replay();

    testCase.thrownAssertionExceptions_.push(e1);
    testCase.thrownAssertionExceptions_.push(e2);

    /** @suppress {visibility} suppression added to enable type checking */
    const exception = testCase.reportUnpropagatedAssertionExceptions_('test');
    assertContains('One or more assertions were', exception.toString());

    mockRecordError.$verify();
    mockRecordError.$tearDown();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testUnreportedAsserts_failedTest() {
    const testCase = new TestCase();
    testCase.addNewTest('testFailSync', () => {
      try {
        assertEquals('Obi-wan', 'Qui-gon');
      } catch (e) {
      }
      assertEquals('Sidious', 'Palpatine');
    });
    testCase.addNewTest(
        'testFailAsync', () => GoogPromise.resolve().then(() => {
          try {
            assertEquals('Kirk', 'Spock');
          } catch (e) {
          }
          assertEquals('Uhura', 'Scotty');
        }));
    testCase.addNewTest(
        'testJustOneFailure', () => GoogPromise.resolve().then(() => {
          assertEquals('R2D2', 'C3PO');
        }));

    const stubs = new PropertyReplacer();
    const getTestCase = functions.constant(testCase);
    stubs.replace(window, '_getCurrentTestCase', getTestCase);

    stubs.replace(TestCase, 'getActiveTestCase', getTestCase);
    return testCase.runTestsReturningPromise()
        .then(() => {
          const errors = testCase.getResult().errors.map((e) => e.message);

          assertArrayEquals(
              [
                // Sync:
                'Expected <Obi-wan> (String) but was <Qui-gon> (String)',
                'Expected <Sidious> (String) but was <Palpatine> (String)',
                // Async:
                'Expected <Kirk> (String) but was <Spock> (String)',
                'Expected <Uhura> (String) but was <Scotty> (String)',
                // JustOneFailure:
                'Expected <R2D2> (String) but was <C3PO> (String)',
              ],
              errors);

          const extraLogMessages = testCase.getResult().messages.filter(
              (m) => googString.contains(
                  m, '1 additional exceptions were swallowed by the test'));
          assertEquals(
              'Expect an additional-exception warning only for the two tests ' +
                  'that swallowed an exception.',
              2, extraLogMessages.length);
        })
        .thenAlways(() => {
          stubs.reset();
        });
  },

  testSetObj() {
    const testCase = new TestCase();
    assertEquals(0, testCase.getCount());
    testCase.setTestObj({testOk: ok, somethingElse: fail});
    assertEquals(1, testCase.getCount());
    // Make sure test count doesn't change after initializeTestCase
    TestCase.initializeTestCase(testCase, undefined);
    assertEquals(1, testCase.getCount());
  },

  async testSetObj_Nested() {
    const testCase = new TestCase();
    assertEquals(0, testCase.getCount());
    testCase.setTestObj({
      setUp: event('setUp1'),
      testOk: event('testOk'),
      somethingElse: fail,
      testNested: {
        setUp: event('setUp2'),
        test: event('testNested'),
        tearDown: event('tearDown2'),
      },
      testNestedIgnoreTests: {
        shouldRunTests() {
          event('testNestedIgnoreTests_ShouldRunTests')();
          return false;
        },

        // 3 tests - 1 of which is nested. shouldRunTests should only be called
        // once.
        testShouldNotRun: event('SHOULD NEVER HAPPEN'),
        testAlsoShouldNotRun: event('ALSO SHOULD NEVER HAPPEN'),

        testSuperNestedIgnore: {
          testShouldNotRun: event('SHOULD NEVER HAPPEN NESTED'),
        },
      },
      testThrowShouldRunTests: {
        shouldRunTests() {
          event('throw shouldRunTests')();
          throw new Error('bar');
        },

        testShouldNotRun: event('SHOULD NEVER HAPPEN THROW'),
        testAlsoShouldNotRun: event('ALSO SHOULD NEVER HAPPEN THROW'),

        testSuperNestedIgnore: {
          testShouldNotRun: event('SHOULD NEVER HAPPEN NESTED THROW'),
        },
      },
      testNestedSuite: {
        setUp: event('setUp3'),
        testA: event('testNestedSuite_A'),
        testB: event('testNestedSuite_B'),
        testSuperNestedSuite: {
          setUp: event('setUp4'),
          testC: event('testNestedSuite_SuperNestedSuite_C'),
          tearDown: event('tearDown4'),
        },
        testSuperNestedIgnoreTests: {
          shouldRunTests() {
            event('testNestedSuite_SuperNestedIgnoreTests_ShouldRunTests')();
            return false;
          },
          testShouldNotRun: event('SHOULD NEVER HAPPEN SUPER NESTED'),
        },
        tearDown: event('tearDown3'),
      },
      tearDown: event('tearDown1'),
    });
    assertEquals(12, testCase.getCount());
    const tests = testCase.getTests();
    const names = [];
    for (let i = 0; i < tests.length; i++) {
      names.push(tests[i].name);
    }
    assertArrayEquals(
        [
          'testOk',
          'testNested',
          'testNestedIgnoreTests_ShouldNotRun',
          'testNestedIgnoreTests_AlsoShouldNotRun',
          'testNestedIgnoreTests_SuperNestedIgnore_ShouldNotRun',
          'testThrowShouldRunTests_ShouldNotRun',
          'testThrowShouldRunTests_AlsoShouldNotRun',
          'testThrowShouldRunTests_SuperNestedIgnore_ShouldNotRun',
          'testNestedSuite_A',
          'testNestedSuite_B',
          'testNestedSuite_SuperNestedSuite_C',
          'testNestedSuite_SuperNestedIgnoreTests_ShouldNotRun',
        ],
        names);
    await testCase.runTestsReturningPromise();
    assertArrayEquals(
        [
          'setUp1',
          'testOk',
          'tearDown1',
          'setUp1',
          'setUp2',
          'testNested',
          'tearDown2',
          'tearDown1',
          'testNestedIgnoreTests_ShouldRunTests',
          'throw shouldRunTests',
          'setUp1',
          'setUp3',
          'testNestedSuite_A',
          'tearDown3',
          'tearDown1',
          'setUp1',
          'setUp3',
          'testNestedSuite_B',
          'tearDown3',
          'tearDown1',
          'setUp1',
          'setUp3',
          'setUp4',
          'testNestedSuite_SuperNestedSuite_C',
          'tearDown4',
          'tearDown3',
          'tearDown1',
          'testNestedSuite_SuperNestedIgnoreTests_ShouldRunTests',
        ],
        events);
  },

  testSetObj_es6Class() {
    let FooTest;
    try {
      eval(
          'FooTest = class { testOk() {assertTrue(true); } somethingElse() {} }');
    } catch (ex) {
      // IE cannot parse ES6.
      return;
    }
    const testCase = new TestCase();
    assertEquals(0, testCase.getCount());
    testCase.setTestObj(new FooTest());
    assertEquals(1, testCase.getCount());
  },

  testSetTestObj_alreadyInitialized() {
    const testCase = new TestCase();
    testCase.setTestObj({test1: ok, test2: ok});
    try {
      testCase.setTestObj({test3: ok, test4: ok});
      fail('Overriding the test object should fail');
    } catch (e) {
      TestCase.invalidateAssertionException(e);
      assertContains(
          'Test methods have already been configured.\n' +
              'Tests previously found:\ntest1\ntest2\n' +
              'New tests found:\ntest3\ntest4',
          e.toString());
    }
  },

  testCurrentTestName() {
    const currentTestName = TestCase.currentTestName;
    assertEquals('testCurrentTestName', currentTestName);
  },

  testCurrentTestNamePromise() {
    const getAssertSameTest = () => {
      const expectedTestCase = TestCase.getActiveTestCase();
      const expectedTestName = (expectedTestCase ? expectedTestCase.getName() :
                                                   '<no active TestCase>') +
          '.' + (TestCase.currentTestName || '<no active test name>');
      const assertSameTest = () => {
        const currentTestCase = TestCase.getActiveTestCase();
        const currentTestName = (currentTestCase ? currentTestCase.getName() :
                                                   '<no active TestCase>') +
                '.' + TestCase.currentTestName ||
            '<no active test name>';
        assertEquals(expectedTestName, currentTestName);
        assertEquals(expectedTestCase, currentTestCase);
      };
      return assertSameTest;
    };
    const assertSameTest = getAssertSameTest();
    // do something asynchronously...
    return new GoogPromise((resolve, reject) => {
      // ... ensure the earlier half runs during the same test ...
      assertSameTest();
      setTimeout(() => {
        // ... and also ensure the later half runs during the same test:
        try {
          assertSameTest();
          resolve();
        } catch (assertionFailureOrResolveException) {
          reject(assertionFailureOrResolveException);
        }
      });
    });
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testCallbackToTestDoneOk() {
    testDoneTestsSeen = [];
    testDoneErrorsSeen = {};
    testDoneRuntime = {};
    const testCase = new TestCase('fooCase');
    testCase.addNewTest('foo', okGoogPromise);
    testCase.setTestDoneCallback(storeCallsAndErrors);
    return testCase.runTestsReturningPromise().then(() => {
      assertStoreCallsAndErrors(['foo'], []);
    });
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testCallbackToTestDoneFail() {
    testDoneTestsSeen = [];
    testDoneErrorsSeen = [];
    testDoneRuntime = {};
    const testCase = new TestCase('fooCase');
    testCase.addNewTest('foo', failGoogPromise);
    testCase.setTestDoneCallback(storeCallsAndErrors);
    return testCase.runTestsReturningPromise().then(() => {
      assertStoreCallsAndErrors(['foo'], ['ERROR in foo']);
    });
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testInitializeTestCase() {
    testDoneTestsSeen = [];
    testDoneErrorsSeen = [];
    const testCase = new TestCase('fooCase');
    testCase.getAutoDiscoveryPrefix = () => 'mockTestName';
    const outerTestCase = TestCase.getActiveTestCase();
    globalThis['G_testRunner'].testCase = null;
    TestCase.initializeTestCase(testCase, storeCallsAndErrors);
    const checkAfterInitialize = TestCase.getActiveTestCase();
    globalThis['G_testRunner'].testCase = outerTestCase;
    // This asserts require G_testRunner to be set.
    assertEquals(checkAfterInitialize, testCase);
    assertEquals(TestCase.getActiveTestCase(), outerTestCase);
    // If the individual test feature is used to selecte this test, erase it.
    testCase.setTestsToRun(null);
    return testCase.runTestsReturningPromise().then(() => {
      assertStoreCallsAndErrors(['mockTestName'], ['ERROR in mockTestName']);
    });
  },

  testChainSetupTestCase() {
    const objectChain = [
      {setUp: event('setUp1'), tearDown: event('tearDown1')},
      {setUp: event('setUp2'), tearDown: event('tearDown2')},
    ];

    const testCase = new TestCase('fooCase');
    testCase.addNewTest('foo', okGoogPromise, undefined, objectChain);

    return testCase.runTestsReturningPromise().then(() => {
      assertArrayEquals(['setUp1', 'setUp2', 'tearDown2', 'tearDown1'], events);
    });
  },

  testShouldRunTests_inconsistentResult() {
    const testCase = new TestCase('fooTest');
    let timesShouldRunTestsCalled = 0;
    testCase.setTestObj({
      shouldRunTests() {
        return (++timesShouldRunTestsCalled) <= 2;
      },

      testFoo() {},
      testBar() {},
      testBuzz() {},
    });

    testCase.runTests();

    assertTrue(testCase.isSuccess());
    const result = testCase.getResult();
    assertEquals(3, result.totalCount);
    assertEquals(3, result.runCount);
    assertEquals(3, result.successCount);
    assertEquals(0, result.errors.length);
    assertEquals(2, timesShouldRunTestsCalled);
  },

  testShouldRunTests_marksTestsAsSkipped() {
    const testCase = new TestCase('fooCase');
    let timesShouldSkipTestsCalled = 0;
    testCase.setTestObj({
      shouldRunTests() {
        // Let it pass once so that we don't exit early before processing any of
        // the test cases.
        return ++timesShouldSkipTestsCalled <= 1;
      },
      testFoo() {},
      testBar() {},
      testBuzz() {},
    });

    testCase.runTests();

    assertTrue(testCase.isSuccess());
    const result = testCase.getResult();
    assertEquals(3, result.totalCount);
    assertEquals(3, result.skipCount);
    assertEquals(0, result.runCount);
    assertEquals(0, result.successCount);
    assertEquals(0, result.errors.length);
  }
});
