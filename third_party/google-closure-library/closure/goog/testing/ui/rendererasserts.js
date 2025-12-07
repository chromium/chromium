/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Additional asserts for testing ControlRenderers.
 */
goog.module('goog.testing.ui.rendererasserts');
goog.module.declareLegacyNamespace();
goog.setTestOnly();

const ControlRenderer = goog.require('goog.ui.ControlRenderer');
const asserts = goog.require('goog.testing.asserts');

/**
 * Assert that a control renderer constructor doesn't call getCssClass.
 * @param {function(new:ControlRenderer)} rendererClassUnderTest The renderer
 *     constructor to test.
 */
function assertNoGetCssClassCallsInConstructor(rendererClassUnderTest) {
  let getCssClassCalls = 0;

  /**
   * @extends {ControlRenderer}
   * @constructor @struct @final
   */
  function TestControlRenderer() {
    TestControlRenderer.base(this, 'constructor');
    rendererClassUnderTest.call(this);
  }
  goog.inherits(TestControlRenderer, rendererClassUnderTest);

  /**
   * @override
   * @return {string}
   */
  TestControlRenderer.prototype.getCssClass = function() {
    getCssClassCalls++;
    return TestControlRenderer.superClass_.getCssClass.call(this);
  };

  // Looking for the side-effects caused by the construction here:
  new TestControlRenderer();

  asserts.assertEquals(
      'Constructors should not call getCssClass, ' +
          'getCustomRenderer must be able to override it post construction.',
      0, getCssClassCalls);
}

exports = {
  assertNoGetCssClassCallsInConstructor,
};
