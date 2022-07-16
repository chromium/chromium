/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.streams.fullImplTest');
goog.setTestOnly();

const testSuite = goog.require('goog.testing.testSuite');
const {TestCasesWithIterator} = goog.require('goog.streams.fullTestCases');
const {newReadableStream} = goog.require('goog.streams.fullImpl');

testSuite(new TestCasesWithIterator(newReadableStream));
