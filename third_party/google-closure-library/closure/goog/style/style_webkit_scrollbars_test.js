/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.style.webkitScrollbarsTest');
goog.setTestOnly();

const ExpectedFailures = goog.require('goog.testing.ExpectedFailures');
const asserts = goog.require('goog.asserts');
const style = goog.require('goog.style');
/** @suppress {extraRequire} */
const styleScrollbarTester = goog.require('goog.styleScrollbarTester');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

let expectedFailures;

testSuite({
  setUpPage() {
    expectedFailures = new ExpectedFailures();
  },

  tearDown() {
    expectedFailures.handleTearDown();

    // Assert that the test loaded.
    asserts.assert(testScrollbarWidth);
  },

  testScrollBarWidth_webkitScrollbar() {
    expectedFailures.expectFailureFor(!userAgent.WEBKIT);

    try {
      const width = style.getScrollbarWidth();
      assertEquals('Scrollbar width should be 16', 16, width);
    } catch (e) {
      expectedFailures.handleException(e);
    }
  },

  testScrollBarWidth_webkitScrollbarWithCustomClass() {
    expectedFailures.expectFailureFor(!userAgent.WEBKIT);

    try {
      const customWidth = style.getScrollbarWidth('otherScrollBar');
      assertEquals('Custom width should be 10', 10, customWidth);
    } catch (e) {
      expectedFailures.handleException(e);
    }
  },
});
