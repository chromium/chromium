/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.PerformanceTimerTest');
goog.setTestOnly();

const Deferred = goog.require('goog.async.Deferred');
const MockClock = goog.require('goog.testing.MockClock');
const PerformanceTimer = goog.require('goog.testing.PerformanceTimer');
const dom = goog.require('goog.dom');
const googMath = goog.require('goog.math');
const testSuite = goog.require('goog.testing.testSuite');

let mockClock;
let sandbox;
let timer;

/**
 * @param {boolean} useSetUp
 * @param {boolean} useTearDown
 * @param {boolean} runAsync
 * @return {!Deferred|undefined} A deferred if any of the test functions was
 *     asynchronous, otherwise, undefined.
 * @suppress {checkTypes} suppression added to enable type checking
 */
function runAndAssert(useSetUp, useTearDown, runAsync) {
  const fakeExecutionTime = [100, 95, 98, 104, 130, 101, 96, 98, 90, 103];
  let count = 0;
  const testFunction = () => {
    mockClock.tick(fakeExecutionTime[count++]);
    if (runAsync) {
      return Deferred.succeed();
    }
  };

  let setUpCount = 0;
  const setUpFunction = () => {
    // Should have no effect on total time.
    mockClock.tick(7);
    setUpCount++;
    if (runAsync) {
      return Deferred.succeed();
    }
  };

  let tearDownCount = 0;
  const tearDownFunction = () => {
    // Should have no effect on total time.
    mockClock.tick(11);
    tearDownCount++;
    if (runAsync) {
      return Deferred.succeed();
    }
  };

  // Fast test function should complete successfully in under 5 seconds...
  const task = new PerformanceTimer.Task(testFunction);
  if (useSetUp) {
    task.withSetUp(setUpFunction);
  }
  if (useTearDown) {
    task.withTearDown(tearDownFunction);
  }
  if (runAsync) {
    let assertsRan = false;
    return timer.runAsyncTask(task)
        .then((results) => {
          /**
           * @suppress {checkTypes} suppression added to enable type checking
           */
          assertsRan = assertResults(
              results, useSetUp, useTearDown, setUpCount, tearDownCount,
              fakeExecutionTime);
        })
        .then(() => assertTrue(assertsRan));
  } else {
    const results = timer.runTask(task);
    assertResults(
        results, useSetUp, useTearDown, setUpCount, tearDownCount,
        fakeExecutionTime);
  }
}

/**
 * @param {Array} results
 * @param {boolean} useSetUp
 * @param {boolean} useTearDown
 * @param {boolean} setUpCount
 * @param {boolean} tearDownCount
 * @param {boolean} fakeExecutionTime
 * @return {boolean} true
 * @suppress {checkTypes} suppression added to enable type checking
 */
function assertResults(
    results, useSetUp, useTearDown, setUpCount, tearDownCount,
    fakeExecutionTime) {
  assertNotNull('Results must be available.', results);

  assertEquals(
      'Average is wrong.', googMath.average.apply(null, fakeExecutionTime),
      results['average']);
  assertEquals(
      'Standard deviation is wrong.',
      googMath.standardDeviation.apply(null, fakeExecutionTime),
      results['standardDeviation']);

  assertEquals('Count must be as expected.', 10, results['count']);
  assertEquals('Maximum is wrong.', 130, results['maximum']);
  assertEquals('Mimimum is wrong.', 90, results['minimum']);
  assertEquals(
      'Total must be a nonnegative number.',
      googMath.sum.apply(null, fakeExecutionTime), results['total']);

  assertEquals(
      'Set up count must be as expected.', useSetUp ? 10 : 0, setUpCount);
  assertEquals(
      'Tear down count must be as expected.', useTearDown ? 10 : 0,
      tearDownCount);

  return true;
}

testSuite({
  setUpPage() {
    sandbox = document.getElementById('sandbox');
  },

  setUp() {
    mockClock = new MockClock(true);
    timer = new PerformanceTimer();
  },

  tearDown() {
    mockClock.dispose();
    timer = null;
    dom.removeChildren(sandbox);
  },

  testConstructor() {
    assertTrue(
        'Timer must be an instance of goog.testing.PerformanceTimer',
        timer instanceof PerformanceTimer);
    assertEquals(
        'Timer must collect the default number of samples', 10,
        timer.getNumSamples());
    assertEquals(
        'Timer must have the default timeout interval', 5000,
        timer.getTimeoutInterval());
  },

  testRun_noSetUpOrTearDown() {
    runAndAssert(false, false, false);
  },

  testRun_withSetup() {
    runAndAssert(true, false, false);
  },

  testRun_withTearDown() {
    runAndAssert(false, true, false);
  },

  testRun_withSetUpAndTearDown() {
    runAndAssert(true, true, false);
  },

  testRunAsync_noSetUpOrTearDown() {
    return runAndAssert(false, false, true);
  },

  testRunAsync_withSetup() {
    return runAndAssert(true, false, true);
  },

  testRunAsync_withTearDown() {
    return runAndAssert(false, true, true);
  },

  testRunAsync_withSetUpAndTearDown() {
    return runAndAssert(true, true, true);
  },

  testTimeout() {
    let count = 0;
    const testFunction = () => {
      mockClock.tick(100);
      ++count;
    };

    timer.setNumSamples(200);
    timer.setTimeoutInterval(2500);
    const results = timer.run(testFunction);

    assertNotNull('Results must be available', results);
    assertEquals('Count is wrong', count, results['count']);
    assertTrue(
        'Count must less than expected',
        results['count'] < timer.getNumSamples());
  },

  testCreateResults() {
    const samples = [53, 0, 103];
    const expectedResults = {
      'average': 52,
      'count': 3,
      'median': 53,
      'maximum': 103,
      'minimum': 0,
      'standardDeviation': googMath.standardDeviation.apply(null, samples),
      'total': 156,
    };
    assertObjectEquals(
        expectedResults, PerformanceTimer.createResults(samples));
  },
});
