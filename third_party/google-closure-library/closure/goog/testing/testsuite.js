/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */
goog.module('goog.testing.testSuite');
goog.module.declareLegacyNamespace();
goog.setTestOnly('goog.testing.testSuite');

const Environment = goog.require('goog.labs.testing.Environment');
const TestCase = goog.require('goog.testing.TestCase');

/** @record */
class TestSuiteOptions {
  constructor() {
    /** @type {!TestCase.Order|undefined} */
    this.order;
  }
}

/**
 * Runs the lifecycle methods (setUp, tearDown, etc.) and test* methods from
 * the given object. For use in tests that are written as JavaScript modules
 * or goog.modules.
 *
 * @param {!Object} obj An object with one or more test methods, and optional
 *     setUp, tearDown and getTestName methods. The object may also have nested
 *     Objects (named like tests, i.e. `testNestedSuite: {}`) that will be
 *     treated as nested testSuites. Any additional setUp will run after parent
 *     setUps, any additional tearDown will run before parent tearDowns. The
 *     this object refers to the object that the functions were defined on, not
 *     the full testSuite object.
 * @param {!TestSuiteOptions=} options Optional options object which can
 *     be used to set the sort order for running tests.
 */
function testSuite(obj, options) {
  if (typeof obj === 'function') {
    throw new Error(
        'testSuite should be called with an object. ' +
        'Did you forget to initialize a class?');
  }

  if (initialized) {
    throw new Error('Only one TestSuite can be active');
  }
  initialized = true;

  const testCase =
      Environment.getTestCaseIfActive() || new TestCase(document.title);
  testCase.setTestObj(obj);

  if (options && options.order) {
    testCase.setOrder(options.order);
  }
  TestCase.initializeTestRunner(testCase);
}

/**
 * True iff the testSuite has been created.
 * @type {boolean}
 */
let initialized = false;

/**
 * Reset the initialized flag so that we can test testSuite.
 * Should not be called outside Closure.  This should be package-private,
 * but it's called from environment_test in labs.testing package.
 */
testSuite.resetForTesting = function() {
  const name = TestCase.getActiveTestCase().getName();
  if (name !== 'environment_test' && name !== 'testsuite_test') {
    throw new Error(name);
  }
  initialized = false;
};

exports = testSuite;
