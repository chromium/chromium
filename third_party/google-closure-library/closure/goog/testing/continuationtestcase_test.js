/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.ContinuationTestCaseTest');
goog.setTestOnly();

const ContinuationTestCase = goog.require('goog.testing.ContinuationTestCase');
const GoogEventTarget = goog.require('goog.events.EventTarget');
const MockClock = goog.require('goog.testing.MockClock');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const TestCase = goog.require('goog.testing.TestCase');
const events = goog.require('goog.events');
/** @suppress {extraRequire} */
const jsunit = goog.require('goog.testing.jsunit');

/**
 * @fileoverview This test file uses the ContinuationTestCase to test itself,
 * which is a little confusing. It's also difficult to write a truly effective
 * test, since testing a failure causes an actual failure in the test runner.
 * All tests have been manually verified using a sophisticated combination of
 * alerts and false assertions.
 */

const clock = new MockClock();
let count = 0;
const stubs = new PropertyReplacer();

/**
 * Installs the Mock Clock and replaces the Step timeouts with the mock
 * implementations.
 */
function installMockClock() {
  clock.install();

  // Overwrite the "protected" setTimeout and clearTimeout with the versions
  // replaced by MockClock. Normal tests should never do this, but we need to
  // test the ContinuationTest itself.
  stubs.set(
      ContinuationTestCase.Step, 'protectedClearTimeout_', window.clearTimeout);
  stubs.set(
      ContinuationTestCase.Step, 'protectedSetTimeout_', window.setTimeout);
}

/**
 * @return {!ContinuationTestCase.Step} A generic step in a continuation test.
 */
function getSampleStep() {
  return new ContinuationTestCase.Step('test', () => {});
}

/**
 * @return {!ContinuationTestCase.ContinuationTest} A simple continuation test
 *     with generic setUp, test, and tearDown functions.
 */
function getSampleTest() {
  const setupStep = new TestCase.Test('setup', () => {});
  const testStep = new TestCase.Test('test', () => {});
  const teardownStep = new TestCase.Test('teardown', () => {});

  return new ContinuationTestCase.ContinuationTest(
      setupStep, testStep, teardownStep);
}

/*
 * Any of the test functions below (except the condition check passed into
 * waitForCondition) can raise an assertion successfully. Any level of nested
 * test steps should be possible, in any configuration.
 */

let testObj;

/** @suppress {undefinedVars} waitForTimeout is an exported function */
function handleTimeout() {
  testObj.steps++;
  assertEquals('handleTimeout should be called first.', 1, testObj.steps);
  waitForTimeout(fireEvent, 10);
}

function fireEvent() {
  testObj.steps++;
  assertEquals('fireEvent should be called second.', 2, testObj.steps);
  testObj.et.dispatchEvent('test');
}

function handleEvent() {
  testObj.steps++;
  assertEquals('handleEvent should be called third.', 3, testObj.steps);
  testObj.lock = false;
}

function condition() {
  return !testObj.lock;
}

function handleCondition() {
  assertFalse(testObj.lock);
  testObj.steps++;
  assertEquals('handleCondition should be called last.', 4, testObj.steps);
}

const testCase = new ContinuationTestCase('Continuation Test Case');
testCase.setTestObj({
  setUpPage() {
    count = testCase.getCount();
  },

  /**
   * Resets the mock clock. Includes a wait step to verify that setUp routines
   * can contain continuations.
   */
  setUp() {
    waitForTimeout(() => {
      // Pointless assertion to verify that setUp methods can contain waits.
      assertEquals(count, testCase.getCount());
    }, 0);

    clock.reset();
  },

  /**
   * Uninstalls the mock clock if it was installed, and restores the Step
   * timeout functions to the default window implementations.
   */
  tearDown() {
    clock.uninstall();
    stubs.reset();

    waitForTimeout(/**
                      @suppress {visibility} suppression added to enable type
                      checking
                    */
                   () => {
                     // Pointless assertion to verify that tearDown methods can
                     // contain waits.
                     assertTrue(testCase.now() >= testCase.startTime_);
                   },
                   0);
  },

  testStepWaiting() {
    const step = getSampleStep();
    assertTrue(step.waiting);
  },

  testStepSetTimeout() {
    installMockClock();
    const step = getSampleStep();

    let timeoutReached = false;
    step.setTimeout(() => {
      timeoutReached = true;
    }, 100);

    clock.tick(50);
    assertFalse(timeoutReached);
    clock.tick(50);
    assertTrue(timeoutReached);
  },

  testStepClearTimeout() {
    const step = new ContinuationTestCase.Step('test', () => {});

    let timeoutReached = false;
    step.setTimeout(() => {
      timeoutReached = true;
    }, 100);

    clock.tick(50);
    assertFalse(timeoutReached);
    step.clearTimeout();
    clock.tick(50);
    assertFalse(timeoutReached);
  },

  testTestPhases() {
    const test = getSampleTest();

    assertEquals('setup', test.getCurrentPhase()[0].name);
    test.cancelCurrentPhase();

    assertEquals('test', test.getCurrentPhase()[0].name);
    test.cancelCurrentPhase();

    assertEquals('teardown', test.getCurrentPhase()[0].name);
    test.cancelCurrentPhase();

    assertNull(test.getCurrentPhase());
  },

  testTestSetError() {
    const test = getSampleTest();

    const error1 = new Error('Oh noes!');
    const error2 = new Error('B0rken.');

    assertNull(test.getError());
    test.setError(error1);
    assertEquals(error1, test.getError());
    test.setError(error2);
    assertEquals(
        'Once an error has been set, it should not be overwritten.', error1,
        test.getError());
  },

  testAddStep() {
    const test = getSampleTest();
    const step = getSampleStep();

    // Try adding a step to each phase and then cancelling the phase.
    for (let i = 0; i < 3; i++) {
      assertEquals(1, test.getCurrentPhase().length);
      test.addStep(step);

      assertEquals(2, test.getCurrentPhase().length);
      assertEquals(step, test.getCurrentPhase()[1]);
      test.cancelCurrentPhase();
    }

    assertNull(test.getCurrentPhase());
  },

  testCancelTestPhase() {
    let test = getSampleTest();

    test.cancelTestPhase();
    assertEquals('teardown', test.getCurrentPhase()[0].name);

    test = getSampleTest();
    test.cancelCurrentPhase();
    test.cancelTestPhase();
    assertEquals('teardown', test.getCurrentPhase()[0].name);

    test = getSampleTest();
    test.cancelTestPhase();
    test.cancelTestPhase();
    assertEquals('teardown', test.getCurrentPhase()[0].name);
  },

  /** @suppress {undefinedVars} waitForTimeout is an exported function */
  testWaitForTimeout() {
    let reachedA = false;
    let reachedB = false;
    let reachedC = false;

    waitForTimeout(() => {
      reachedA = true;

      assertTrue('A must be true at callback a.', reachedA);
      assertFalse('B must be false at callback a.', reachedB);
      assertFalse('C must be false at callback a.', reachedC);
    }, 10);

    waitForTimeout(() => {
      reachedB = true;

      assertTrue('A must be true at callback b.', reachedA);
      assertTrue('B must be true at callback b.', reachedB);
      assertFalse('C must be false at callback b.', reachedC);
    }, 20);

    waitForTimeout(() => {
      reachedC = true;

      assertTrue('A must be true at callback c.', reachedA);
      assertTrue('B must be true at callback c.', reachedB);
      assertTrue('C must be true at callback c.', reachedC);
    }, 20);

    assertFalse('a', reachedA);
    assertFalse('b', reachedB);
    assertFalse('c', reachedC);
  },

  /** @suppress {undefinedVars} waitForCondition is exported */
  testWaitForEvent() {
    const et = new GoogEventTarget();

    let eventFired = false;
    events.listen(et, 'testPrefire', () => {
      eventFired = true;
      et.dispatchEvent('test');
    });

    waitForEvent(et, 'test', () => {
      assertTrue(eventFired);
    });

    et.dispatchEvent('testPrefire');
  },

  /** @suppress {undefinedVars} waitForCondition is exported */
  testWaitForCondition() {
    let counter = 0;

    waitForCondition(() => ++counter >= 2, () => {
      assertEquals(2, counter);
    }, 10, 200);
  },

  testOutOfOrderWaits() {
    let counter = 0;

    // Note that if the delta between the timeout is too small, two
    // continuation may be invoked at the same timer tick, using the
    // registration order.
    waitForTimeout(() => {
      assertEquals(3, ++counter);
    }, 200);
    waitForTimeout(() => {
      assertEquals(1, ++counter);
    }, 0);
    waitForTimeout(() => {
      assertEquals(2, ++counter);
    }, 100);
  },

  testCrazyNestedWaitFunction() {
    testObj = {lock: true, et: new GoogEventTarget(), steps: 0};

    waitForTimeout(handleTimeout, 10);
    waitForEvent(testObj.et, 'test', handleEvent);
    waitForCondition(condition, handleCondition, 1);
  },
});
TestCase.initializeTestRunner(testCase);
