/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.ToolbarColorMenuButtonRendererTest');
goog.setTestOnly();

const RendererHarness = goog.require('goog.testing.ui.RendererHarness');
const ToolbarColorMenuButton = goog.require('goog.ui.ToolbarColorMenuButton');
const ToolbarColorMenuButtonRenderer = goog.require('goog.ui.ToolbarColorMenuButtonRenderer');
const dom = goog.require('goog.dom');
const rendererasserts = goog.require('goog.testing.ui.rendererasserts');
const testSuite = goog.require('goog.testing.testSuite');

let harness;

testSuite({
  setUp() {
    harness = new RendererHarness(
        ToolbarColorMenuButtonRenderer.getInstance(), dom.getElement('parent'),
        dom.getElement('decoratedButton'));
  },

  tearDown() {
    harness.dispose();
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testEquality() {
    harness.attachControlAndRender(new ToolbarColorMenuButton('Foo'));
    harness.attachControlAndDecorate(new ToolbarColorMenuButton());
    harness.assertDomMatches();
  },

  testDoesntCallGetCssClassInConstructor() {
    rendererasserts.assertNoGetCssClassCallsInConstructor(
        ToolbarColorMenuButtonRenderer);
  },
});
