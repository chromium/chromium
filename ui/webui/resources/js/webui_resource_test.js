// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Tests that an observation matches the expected value.
 * @param {*} expected The expected value.
 * @param {*} observed The actual value.
 * @param {string=} opt_message Optional message to include with a test
 *     failure.
 */
function assertEquals(expected, observed, opt_message) {
  if (observed !== expected) {
    let message = 'Assertion Failed\n  Observed: ' + observed +
        '\n  Expected: ' + expected;
    if (opt_message) {
      message = message + '\n  ' + opt_message;
    }
    throw new Error(message);
  }
}

/**
 * Verifies that a test result is true.
 * @param {boolean} observed The observed value.
 * @param {string=} opt_message Optional message to include with a test
 *     failure.
 */
function assertTrue(observed, opt_message) {
  assertEquals(true, observed, opt_message);
}

/**
 * Verifies that a test result is false.
 * @param {boolean} observed The observed value.
 * @param {string=} opt_message Optional message to include with a test
 *     failure.
 */
function assertFalse(observed, opt_message) {
  assertEquals(false, observed, opt_message);
}

/**
 * Verifies that the observed and reference values differ.
 * @param {*} reference The target value for comparison.
 * @param {*} observed The test result.
 * @param {string=} opt_message Optional message to include with a test
 *     failure.
 */
function assertNotEqual(reference, observed, opt_message) {
  if (observed === reference) {
    let message = 'Assertion Failed\n  Observed: ' + observed +
        '\n  Reference: ' + reference;
    if (opt_message) {
      message = message + '\n  ' + opt_message;
    }
    throw new Error(message);
  }
}

/**
 * Verifies that a test evaluation results in an exception.
 * @param {!Function} f The test function.
 */
function assertThrows(f) {
  let triggeredError = false;
  try {
    f();
  } catch (err) {
    triggeredError = true;
  }
  if (!triggeredError) {
    throw new Error('Assertion Failed: throw expected.');
  }
}

/**
 * Verifies that the contents of the expected and observed arrays match.
 * @param {!Array} expected The expected result.
 * @param {!Array} observed The actual result.
 */
function assertArrayEquals(expected, observed) {
  const v1 = Array.prototype.slice.call(expected);
  const v2 = Array.prototype.slice.call(observed);
  let equal = v1.length === v2.length;
  if (equal) {
    for (let i = 0; i < v1.length; i++) {
      if (v1[i] !== v2[i]) {
        equal = false;
        break;
      }
    }
  }
  if (!equal) {
    const message =
        ['Assertion Failed', 'Observed: ' + v2, 'Expected: ' + v1].join('\n  ');
    throw new Error(message);
  }
}

/**
 * Verifies that the expected and observed result have the same content.
 * @param {*} expected The expected result.
 * @param {*} observed The actual result.
 */
function assertDeepEquals(expected, observed, opt_message) {
  if (typeof expected === 'object' && expected !== null) {
    assertNotEqual(null, observed);
    for (const key in expected) {
      assertTrue(key in observed, opt_message);
      assertDeepEquals(expected[key], observed[key], opt_message);
    }
    for (const key in observed) {
      assertTrue(key in expected, opt_message);
    }
  } else {
    assertEquals(expected, observed, opt_message);
  }
}

/**
 * Decorates |window| with runTests() and endTests().
 *
 * @param {{
 *   runTests: (function(Object=):void|undefined),
 *   endTests: (function(boolean):void|undefined)
 * }} exports
 */
(function(exports) {

/**
 * Optional setup and teardown hooks that can be defined in a test scope.
 * |setUpPage| is invoked once. |setUp|/|tearDown| are invoked before/after each
 * test*() declared in the test scope.
 *
 * @typedef {{
 *   setUpPage: (function(): void|undefined),
 *   setUp: (function(): void|undefined),
 *   tearDown: (function(): void|undefined),
 * }}
 */
let WebUiTestHarness;

/**
 * Scope containing testXXX functions.
 * @type {!Object}
 */
let testScope = {};

/**
 * Test harness entrypoints on |testScope|.
 * @type {!WebUiTestHarness}
 */
let testHarness = {};

/**
 * List of test cases.
 * @type {Array<string>} List of function names for tests to run.
 */
const testCases = [];

/**
 * Indicates if all tests have run successfully.
 * @type {boolean}
 */
let cleanTestRun = true;

/**
 * Armed during setup of a test to call the matching tear down code.
 * @type {Function}
 */
let pendingTearDown = null;

/**
 * Name of current test.
 * @type {?string}
 */
let testName = null;

/**
 * Time current test started.
 * @type {number}
 */
let testStartTime = 0;

/**
 * Time first test started.
 * @type {number}
 */
let runnerStartTime = 0;

/**
 * Runs all functions starting with test and reports success or
 * failure of the test suite.
 * @param {Object=} opt_testScope optional scope containing testXXX functions.
 *   Uses global 'window' by default.
 */
function runTests(opt_testScope) {
  runnerStartTime = performance.now();
  testScope = opt_testScope || window;
  testHarness = /** @type{!WebUiTestHarness} */ (testScope);
  for (const name in testScope) {
    // To avoid unnecessary getting properties, test name first.
    if (/^test/.test(name) && typeof testScope[name] === 'function') {
      testCases.push(name);
    }
  }
  if (!testCases.length) {
    console.error('Failed to find test cases.');
    cleanTestRun = false;
  }
  try {
    if (testHarness.setUpPage) {
      testHarness.setUpPage();
    }
  } catch (err) {
    cleanTestRun = false;
  }
  startTesting();
}

/**
 * @suppress {missingProperties}
 */
function startTesting() {
  if (window.waitUser) {
    setTimeout(startTesting, 1000);
    return;
  }
  continueTesting();
}

/**
 * Runs the next test in the queue. Reports the test results if the queue is
 * empty.
 * @param {boolean=} opt_asyncTestFailure Optional parameter indicated if the
 *     last asynchronous test failed.
 */
async function continueTesting(opt_asyncTestFailure) {
  const now = performance.now();
  if (testName) {
    console.info(
        'TEST ' + testName +
        ' complete, status=' + (opt_asyncTestFailure ? 'FAIL' : 'PASS') +
        ', duration=' + Math.round(now - testStartTime) + 'ms');
  }
  if (opt_asyncTestFailure) {
    cleanTestRun = false;
  }
  let done = false;
  if (pendingTearDown) {
    pendingTearDown();
    pendingTearDown = null;
  }
  if (testCases.length > 0) {
    testStartTime = now;
    testName = testCases.pop();
    console.info('TEST ' + testName + ' starting...');
    const isAsyncTest = testScope[testName].length;
    let testError = false;
    try {
      if (testHarness.setUp) {
        testHarness.setUp();
      }
      pendingTearDown = testHarness.tearDown || null;
      await testScope[testName](continueTesting);
    } catch (err) {
      console.error('Failure in test ' + testName + '\n' + err);
      console.info(err.stack);
      cleanTestRun = false;
      testError = true;
    }
    // Asynchronous tests must manually call continueTesting when complete
    // unless they throw an exception.
    if (!isAsyncTest || testError) {
      continueTesting();
    }
  } else {
    done = true;
    endTests(cleanTestRun);
  }
  if (!done) {
    window.domAutomationController.send('PENDING');
  }
}

/**
 * Signals completion of a test.
 * @param {boolean} success Indicates if the test completed successfully.
 */
function endTests(success) {
  const duration =
      runnerStartTime === 0 ? 0 : performance.now() - runnerStartTime;
  console.info(
      'TEST all complete, status=' + (success ? 'PASS' : 'FAIL') +
      ', duration=' + Math.round(duration) + 'ms');
  testName = null;
  runnerStartTime = 0;
  window.domAutomationController.send(success ? 'SUCCESS' : 'FAILURE');
}

exports.runTests = runTests;
exports.endTests = endTests;
})(window);

/**
 * @type {!function(Object=):void}
 */
window.runTests;

/**
 * @type {!function(boolean):void}
 */
window.endTests;

window.onerror = function() {
  window.endTests(false);
};
