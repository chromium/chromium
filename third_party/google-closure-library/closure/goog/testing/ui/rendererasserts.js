/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Additional asserts for testing ControlRenderers.
 */

goog.setTestOnly('goog.testing.ui.rendererasserts');
goog.provide('goog.testing.ui.rendererasserts');

goog.require('goog.testing.asserts');
goog.require('goog.ui.ControlRenderer');


/**
 * Assert that a control renderer constructor doesn't call getCssClass.
 *
 * @param {function(new:goog.ui.ControlRenderer)} rendererClassUnderTest The
 *     renderer constructor to test.
 */
goog.testing.ui.rendererasserts.assertNoGetCssClassCallsInConstructor =
    function(rendererClassUnderTest) {
  'use strict';
  let getCssClassCalls = 0;

  /**
   * @constructor
   * @extends {goog.ui.ControlRenderer}
   * @final
   */
  function TestControlRenderer() {
    rendererClassUnderTest.call(this);
  }
  goog.inherits(TestControlRenderer, rendererClassUnderTest);

  /** @override */
  TestControlRenderer.prototype.getCssClass = function() {
    'use strict';
    getCssClassCalls++;
    return TestControlRenderer.superClass_.getCssClass.call(this);
  };

  // Looking for the side-effects caused by the construction here:
  new TestControlRenderer();

  assertEquals(
      'Constructors should not call getCssClass, ' +
          'getCustomRenderer must be able to override it post construction.',
      0, getCssClassCalls);
};
