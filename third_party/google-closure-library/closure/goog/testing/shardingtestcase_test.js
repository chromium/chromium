/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.ShardingTestCaseTest');
goog.setTestOnly();

const ShardingTestCase = goog.require('goog.testing.ShardingTestCase');
const TestCase = goog.require('goog.testing.TestCase');
const asserts = goog.require('goog.testing.asserts');
/** @suppress {extraRequire} */
const jsunit = goog.require('goog.testing.jsunit');

const testCase = new ShardingTestCase(1, 2);
testCase.setTestObj({
  testA() {},

  testB() {
    fail('testB should not be in this shard');
  },
});
TestCase.initializeTestRunner(testCase);
