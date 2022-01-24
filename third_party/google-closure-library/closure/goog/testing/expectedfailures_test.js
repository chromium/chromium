/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.ExpectedFailuresTest');
goog.setTestOnly();

const ExpectedFailures = goog.require('goog.testing.ExpectedFailures');
const JsUnitException = goog.require('goog.testing.JsUnitException');
const Level = goog.require('goog.log.Level');
const log = goog.require('goog.log');
const testSuite = goog.require('goog.testing.testSuite');

let count;
let expectedFailures;
let lastLevel;
let lastMessage;

// Individual test methods.

testSuite({
  setUpPage() {
    log.addHandler(log.getLogger('goog.testing.ExpectedFailures'), (record) => {
      lastLevel = record.getLevel();
      lastMessage = record.getMessage();
      count++;
    });
  },

  setUp() {
    expectedFailures = new ExpectedFailures();
    count = 0;
    lastLevel = lastMessage = '';
  },

  testNoExpectedFailure() {
    expectedFailures.handleTearDown();
  },

  testPreventExpectedFailure() {
    expectedFailures.expectFailureFor(true);

    expectedFailures.handleException(new JsUnitException('', ''));
    assertEquals('Should have logged a message', 1, count);
    assertEquals('Should have logged an info message', Level.INFO, lastLevel);
    assertContains(
        'Should log a suppression message', 'Suppressing test failure',
        lastMessage);

    expectedFailures.handleTearDown();
    assertEquals('Should not have logged another message', 1, count);
  },

  testDoNotPreventException() {
    const ex = 'exception';
    expectedFailures.expectFailureFor(false);
    const e = assertThrows('Should have rethrown exception', () => {
      expectedFailures.handleException(ex);
    });
    assertEquals('Should rethrow same exception', ex, e);
  },

  testExpectedFailureDidNotOccur() {
    expectedFailures.expectFailureFor(true);

    expectedFailures.handleTearDown();
    assertEquals('Should have logged a message', 1, count);
    assertEquals('Should have logged a warning', Level.WARNING, lastLevel);
    assertContains(
        'Should log a suppression message', 'Expected a test failure',
        lastMessage);
  },

  testRun() {
    expectedFailures.expectFailureFor(true);

    expectedFailures.run(() => {
      fail('Expected failure');
    });

    assertEquals('Should have logged a message', 1, count);
    assertEquals('Should have logged an info message', Level.INFO, lastLevel);
    assertContains(
        'Should log a suppression message', 'Suppressing test failure',
        lastMessage);

    expectedFailures.handleTearDown();
    assertEquals('Should not have logged another message', 1, count);
  },

  testRunStrict() {
    expectedFailures.expectFailureFor(true);

    const ex = assertThrowsJsUnitException(() => {
      expectedFailures.run(
          () => {
              // Doesn't fail!
          });
    });
    assertContains(
        'Expected a test failure in \'testRunStrict\' but the test passed.',
        ex.message);
  },

  testRunLenient() {
    expectedFailures.expectFailureFor(true);

    expectedFailures.run(
        () => {
            // Doesn't fail!
        },
        true);
    expectedFailures.handleTearDown();
    assertEquals('Should have logged a message', 1, count);
    assertEquals('Should have logged a warning', Level.WARNING, lastLevel);
    assertContains(
        'Should log a suppression message', 'Expected a test failure',
        lastMessage);
  },
});
