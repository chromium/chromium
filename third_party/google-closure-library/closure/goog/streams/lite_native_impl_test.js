/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.streams.liteNativeImplTest');
goog.setTestOnly();

const testSuite = goog.require('goog.testing.testSuite');
const {TestCases} = goog.require('goog.streams.liteTestCases');
const {newReadableStream} = goog.require('goog.streams.liteNativeImpl');

let nativeImplementation = false;

try {
  new ReadableStream();
  nativeImplementation = true;
} catch (e) {
}

if (nativeImplementation) {
  testSuite(new TestCases(newReadableStream));
} else {
  testSuite({
    testNotEnabledForNonNativeReadableStreamBrowsers() {},
  });
}
