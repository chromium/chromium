/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.MultiTestRunnerTest');
goog.setTestOnly('goog.testing.MultiTestRunnerTest');

const jsunit = goog.require('goog.testing.jsunit');

// Delay running the tests after page load. This test has some asynchronous
// behavior that interacts with page load detection.
/** @suppress {constantProperty} suppression added to enable type checking */
jsunit.AUTO_RUN_DELAY_IN_MS = 500;

const MockControl = goog.require('goog.testing.MockControl');
const MultiTestRunner = goog.require('goog.testing.MultiTestRunner');
const Promise = goog.require('goog.Promise');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const TestCase = goog.require('goog.testing.TestCase');
const array = goog.require('goog.array');
const asserts = goog.require('goog.testing.asserts');
const events = goog.require('goog.events');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');

const ALL_TESTS = [
  'testdata/fake_passing_test.html', 'testdata/fake_failing_test.html',
  'testdata/fake_failing_test2.html'
];
const EMPTY_TEST = 'testdata/fake_failing_test3.html';
const SKIPPED_TEST = 'testdata/fake_failing_test4.html';

let testRunner;
const mocks = new MockControl();
const stubs = new PropertyReplacer();


/**
 * Asserts string matches exactly one item in the given array. Useful for
 * matching elements in an array without guaranteed ordering.
 *
 * @param {string} string String to match in the array.
 * @param {!Array<string>} strings Array of strings find match.
 */
function assertArrayContainsString(string, strings) {
  asserts.assertEquals(
      'Expected the string "' + string +
          '" to appear exactly once in the array <' + strings.join(', ') + '>.',
      1, array.count(strings, function(str) {
        return str == string;
      }));
}


/**
 * Returns promise that resolves when eventType is dispatched from target.
 * @param {!EventTarget|!events.Listenable} target Target to listen for
 *     event on.
 * @param {string} eventType Type of event.
 * @return {!Promise} Promise that resolves with triggered event.
 */
function createEventPromise(target, eventType) {
  return new Promise(function(resolve, reject) {
    events.listen(target, eventType, resolve);
  });
}


/**
 * @typedef {{
 *   failureReports: !Array<TestCase.IResult>,
 *   testNames: !Array<string>
 * }}
 */
let TestResults;


/**
 * Processes the test results returned from MultiTestRunner and creates a
 * consolidated test result object.
 * @param {!Array<!Object<string,!Array<TestCase.IResult>>>}
 *     testResults The list of individual test results from MultiTestRunner.
 * @return {!TestResults} Consolidated test results for all individual tests.
 */
function processTestResults(testResults) {
  let failureReports = [];
  const testNames = [];

  for (let i = 0; i < testResults.length; i++) {
    for (const testName in testResults[i]) {
      testNames.push(testName);
      failureReports = failureReports.concat(testResults[i][testName]);
    }
  }

  return {failureReports: failureReports, testNames: testNames};
}

testSuite({
  setUpPage: function() {
    TestCase.getActiveTestCase().promiseTimeout = 20000;
  },

  setUp: function() {
    testRunner = new MultiTestRunner().setPoolSize(3).addTests(ALL_TESTS);
  },

  tearDown: function() {
    testRunner.dispose();
    mocks.$tearDown();
    stubs.reset();
  },

  testStartButtonStartsTests: /**
                                 @suppress {checkTypes} suppression added to
                                 enable type checking
                               */
      function() {
        testRunner.createDom();
        testRunner.render(document.getElementById('runner'));
        const el = testRunner.getElement();
        const startButton = el.querySelectorAll('button')[0];
        assertEquals('Start', startButton.innerHTML);
        const mockStart =
            mocks.createMethodMock(MultiTestRunner.prototype, 'start');

        mockStart();

        mocks.$replayAll();
        testingEvents.fireClickSequence(startButton);
        mocks.$verifyAll();
      },

  testStopButtonStopsTests: function() {
    const promise = createEventPromise(testRunner, 'testsFinished');
    testRunner.createDom();
    testRunner.render(document.getElementById('runner'));
    const el = testRunner.getElement();
    const startButton = el.querySelectorAll('button')[0];
    const stopButton = el.querySelectorAll('button')[1];
    assertEquals('Stop', stopButton.innerHTML);
    stubs.replace(
        MultiTestRunner.TestFrame.prototype, 'runTest', function() { return; });

    testingEvents.fireClickSequence(startButton);
    testingEvents.fireClickSequence(stopButton);
    return promise.then(function(results) {
      // Tests should be halted and marked as "unfinished".
      assertContains(
          'These tests did not finish:\n' +
              'testdata/fake_passing_test.html\n' +
              'testdata/fake_failing_test.html\n' +
              'testdata/fake_failing_test2.html',
          el.innerHTML);
    });
  },

  testDisposeInternal: /**
                          @suppress {visibility} suppression added to enable
                          type checking
                        */
      function() {
        testRunner.dispose();

        assertTrue(testRunner.tableSorter_.isDisposed());
        assertTrue(testRunner.eh_.isDisposed());
        assertNull(testRunner.startButtonEl_);
        assertNull(testRunner.stopButtonEl_);
        assertNull(testRunner.logEl_);
        assertNull(testRunner.reportEl_);
        assertNull(testRunner.progressEl_);
        assertNull(testRunner.logTabEl_);
        assertNull(testRunner.reportTabEl_);
        assertNull(testRunner.statsTabEl_);
        assertNull(testRunner.statsEl_);
      },

  testRunsTestsAndReportsResults: function() {
    const promise = createEventPromise(testRunner, 'testsFinished');

    testRunner.render(document.getElementById('runner'));
    testRunner.start();

    return promise.then(function(results) {
      const testResults = processTestResults(results['allTestResults']);
      const testNames = testResults.testNames;
      assertEquals(3, testNames.length);
      assertArrayContainsString(
          'testdata/fake_failing_test2:testFail', testNames);
      assertArrayContainsString(
          'testdata/fake_failing_test:testFail', testNames);
      assertArrayContainsString(
          'testdata/fake_passing_test:testPass', testNames);
      const failureReports = testResults.failureReports;
      const failedTests = testRunner.getTestsThatFailed();
      assertEquals(2, failureReports.length);
      assertEquals(2, failedTests.length);
      assertArrayContainsString('testdata/fake_failing_test.html', failedTests);
      assertArrayContainsString(
          'testdata/fake_failing_test2.html', failedTests);
    });
  },

  testMissingTestResultsIsAFailure: function() {
    const promise = createEventPromise(testRunner, 'testsFinished');

    testRunner.addTests(EMPTY_TEST);
    testRunner.render(document.getElementById('runner'));
    testRunner.start();

    return promise.then(function(results) {
      const testResults = processTestResults(results['allTestResults']);
      const testNames = testResults.testNames;
      assertEquals(4, testNames.length);
      assertArrayContainsString('testdata/fake_failing_test3', testNames);
      const failureReports = testResults.failureReports;
      const failedTests = testRunner.getTestsThatFailed();
      assertEquals(3, failureReports.length);
      assertEquals(3, failedTests.length);
      assertArrayContainsString(
          'testdata/fake_failing_test3.html', failedTests);
    });
  },

  testShouldRunTestsFalseIsSuccess: function() {
    const promise = createEventPromise(testRunner, 'testsFinished');

    testRunner.addTests(SKIPPED_TEST);
    testRunner.render(document.getElementById('runner'));
    testRunner.start();

    return promise.then(function(results) {
      const testResults = processTestResults(results['allTestResults']);
      const testNames = testResults.testNames;
      assertEquals(4, testNames.length);
      assertArrayContainsString('testdata/fake_failing_test4', testNames);
      const failureReports = testResults.failureReports;
      const failedTests = testRunner.getTestsThatFailed();
      // Test should pass even though its test method is a failure.
      assertNotContains('testdata/fake_failing_test4', failedTests);
    });
  },

  testRunTestsWithEmptyTestList: function() {
    const testRunner = new MultiTestRunner().setPoolSize(3).addTests([]);
    const promise = createEventPromise(testRunner, 'testsFinished');

    testRunner.render(document.getElementById('runner'));
    testRunner.start();

    return promise.then(function(results) {
      const allTestResults = results['allTestResults'];
      assertEquals(0, allTestResults.length);
      const failureReports = processTestResults(allTestResults).failureReports;
      assertEquals(0, failureReports.length);
      assertEquals(0, testRunner.getTestsThatFailed().length);
      testRunner.dispose();
    });
  },

  testFilterFunctionFiltersTest: function() {
    const promise = createEventPromise(testRunner, 'testsFinished');

    testRunner.render(document.getElementById('runner'));
    testRunner.setFilterFunction(function(test) {
      return test.indexOf('fake_failing_test2') != -1;
    });
    testRunner.start();

    return promise.then(function(results) {
      const allTestResults = results['allTestResults'];
      assertEquals(1, allTestResults.length);
      const failureReports = processTestResults(allTestResults).failureReports;
      const failedTests = testRunner.getTestsThatFailed();
      assertEquals(1, failureReports.length);
      assertEquals(1, failedTests.length);
      assertArrayContainsString(
          'testdata/fake_failing_test2.html', failedTests);
    });
  },

  testTimeoutFailsAfterTimeout: function() {
    testRunner = new MultiTestRunner().setPoolSize(3).addTests([
      'testdata/fake_long_running_failing_test',
      'testdata/fake_long_running_passing_test'
    ]);
    const promise = createEventPromise(testRunner, 'testsFinished');

    testRunner.render(document.getElementById('runner'));
    testRunner.setTimeout(0);
    // If the fake tests complete before the timeout check, this test will fail.
    testRunner.start();

    return promise.then(function(results) {
      const testResults = processTestResults(results['allTestResults']);
      const testNames = testResults.testNames;
      // Only the filename should be the test name for timeouts.
      assertArrayContainsString(
          'testdata/fake_long_running_failing_test', testNames);
      assertArrayContainsString(
          'testdata/fake_long_running_passing_test', testNames);
      assertEquals(2, testNames.length);
      const failureReports = testResults.failureReports;
      assertContains('timed out', failureReports[0]['message']);
      assertContains('timed out', failureReports[1]['message']);
      assertEquals(2, failureReports.length);
      const failedTests = testRunner.getTestsThatFailed();
      assertArrayContainsString(
          'testdata/fake_long_running_failing_test', failedTests);
      assertArrayContainsString(
          'testdata/fake_long_running_passing_test', failedTests);
      assertEquals(2, failedTests.length);
    });
  },

  testRunsAllTestsWhenPoolSizeSmallerThanTotalTests: function() {
    const promise = createEventPromise(testRunner, 'testsFinished');

    testRunner.render(document.getElementById('runner'));
    // There are 3 tests, it should load and run the 3 serially without failing.
    testRunner.setPoolSize(1);
    testRunner.start();

    return promise.then(function(results) {
      assertEquals(3, results['allTestResults'].length);
      const testResults = processTestResults(results['allTestResults']);
      const testNames = testResults.testNames;
      assertEquals(3, testNames.length);
      assertArrayContainsString(
          'testdata/fake_failing_test2:testFail', testNames);
      assertArrayContainsString(
          'testdata/fake_failing_test:testFail', testNames);
      assertArrayContainsString(
          'testdata/fake_passing_test:testPass', testNames);
      const failureReports = testResults.failureReports;
      const failedTests = testRunner.getTestsThatFailed();
      assertEquals(2, failureReports.length);
      assertEquals(2, failedTests.length);
      assertArrayContainsString('testdata/fake_failing_test.html', failedTests);
      assertArrayContainsString(
          'testdata/fake_failing_test2.html', failedTests);
    });
  },

  testFrameGetStats: function() {
    const frame = new MultiTestRunner.TestFrame('/', 2000, false);
    /** @suppress {visibility} suppression added to enable type checking */
    frame.testFile_ = 'foo';
    /** @suppress {visibility} suppression added to enable type checking */
    frame.isSuccess_ = true;
    /** @suppress {visibility} suppression added to enable type checking */
    frame.runTime_ = 42;
    /** @suppress {visibility} suppression added to enable type checking */
    frame.totalTime_ = 9000;
    /** @suppress {visibility} suppression added to enable type checking */
    frame.numFilesLoaded_ = 4;

    assertObjectEquals(
        {
          'testFile': 'foo',
          'success': true,
          'runTime': 42,
          'totalTime': 9000,
          'numFilesLoaded': 4
        },
        frame.getStats());
  },

  testFrameDisposeInternal: /**
                               @suppress {visibility} suppression added to
                               enable type checking
                             */
      function() {
        const frame = new MultiTestRunner.TestFrame('', 2000, false);
        frame.createDom();
        frame.render();
        stubs.replace(frame, 'checkForCompletion_', function() {
          return;
        });
        frame.runTest(ALL_TESTS[0]);
        assertEquals(
            1,
            frame.getDomHelper().getElementsByTagNameAndClass('iframe').length);
        frame.dispose();
        assertTrue(frame.eh_.isDisposed());
        assertEquals(
            0,
            frame.getDomHelper().getElementsByTagNameAndClass('iframe').length);
        assertNull(frame.iframeEl_);
      }
});
