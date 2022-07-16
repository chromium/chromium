/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.ui.rendererassertsTest');
goog.setTestOnly();

const ControlRenderer = goog.require('goog.ui.ControlRenderer');
const asserts = goog.require('goog.testing.asserts');
const rendererasserts = goog.require('goog.testing.ui.rendererasserts');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testSuccess() {
    function GoodRenderer() {}

    rendererasserts.assertNoGetCssClassCallsInConstructor(GoodRenderer);
  },

  testFailure() {
    function BadRenderer() {
      ControlRenderer.call(this);
      this.myClass = this.getCssClass();
    }
    goog.inherits(BadRenderer, ControlRenderer);

    // Expected assertNoGetCssClassCallsInConstructor to fail.
    const ex = assertThrowsJsUnitException(() => {
      rendererasserts.assertNoGetCssClassCallsInConstructor(BadRenderer);
    });
    assertTrue(
        'Expected assertNoGetCssClassCallsInConstructor to throw a' +
            ' jsunit exception',
        ex.isJsUnitException);
    assertContains('getCssClass', ex.message);
  },
});
