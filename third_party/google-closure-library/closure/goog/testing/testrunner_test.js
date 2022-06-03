/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.TestRunnerTest');
goog.setTestOnly();

const TestCase = goog.require('goog.testing.TestCase');
const TestRunner = goog.require('goog.testing.TestRunner');
const asserts = goog.require('goog.testing.asserts');
const testSuite = goog.require('goog.testing.testSuite');

let testRunner;
let testCase;

testSuite({
  setUp() {
    testRunner = new TestRunner();
    testCase = new TestCase();
  },

  testInitialize() {
    assert(!testRunner.isInitialized());
    testRunner.initialize(testCase);
    assert(testRunner.isInitialized());
  },

  testIsFinished() {
    testRunner.initialize(testCase);
    assert(!testRunner.isFinished());
    testRunner.logError('oops');
    assert(testRunner.isFinished());
  },

  testGetUniqueId() {
    // We only really care that this string is unique to instances.
    const anotherRunner = new TestRunner();
    assert(anotherRunner.getUniqueId() != testRunner.getUniqueId());
  },
});
