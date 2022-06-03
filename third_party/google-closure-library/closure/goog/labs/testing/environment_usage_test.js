/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.labs.testing.environmentUsageTest');
goog.setTestOnly();

const Environment = goog.require('goog.labs.testing.Environment');
const testSuite = goog.require('goog.testing.testSuite');

let testing = false;
const env = new Environment();

testSuite({
  setUpPage() {
    assertFalse(testing);
  },

  setUp() {
    testing = true;
  },

  testOne() {
    assertTrue(testing);
  },

  testTwo() {
    assertTrue(testing);
  },

  tearDown() {
    testing = false;
  },

  tearDownPage() {
    assertFalse(testing);
  },
});
