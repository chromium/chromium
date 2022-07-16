/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.ColorMenuButtonTest');
goog.setTestOnly();

const ColorMenuButton = goog.require('goog.ui.ColorMenuButton');
const ColorMenuButtonRenderer = goog.require('goog.ui.ColorMenuButtonRenderer');
const RendererHarness = goog.require('goog.testing.ui.RendererHarness');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const rendererasserts = goog.require('goog.testing.ui.rendererasserts');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

let harness;

testSuite({
  setUp() {
    harness = new RendererHarness(
        ColorMenuButtonRenderer.getInstance(), dom.getElement('parent'),
        dom.getElement('decoratedButton'));
  },

  tearDown() {
    harness.dispose();
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testEquality() {
    harness.attachControlAndRender(new ColorMenuButton('Foo'));
    harness.attachControlAndDecorate(new ColorMenuButton());
    harness.assertDomMatches();
  },

  testWrapCaption() {
    const caption = dom.createDom(TagName.DIV, null, 'Foo');
    const wrappedCaption =
        ColorMenuButtonRenderer.wrapCaption(caption, dom.getDomHelper());
    assertNotEquals(
        'Caption should have been wrapped', caption, wrappedCaption);
    assertEquals(
        'Wrapped caption should have indicator css class',
        'goog-color-menu-button-indicator', wrappedCaption.className);
  },

  testSetCaptionValue() {
    const caption = dom.createDom(TagName.DIV, null, 'Foo');
    const wrappedCaption =
        ColorMenuButtonRenderer.wrapCaption(caption, dom.getDomHelper());
    ColorMenuButtonRenderer.setCaptionValue(wrappedCaption, 'red');

    const expectedColor = userAgent.IE && !userAgent.isDocumentModeOrHigher(9) ?
        '#ff0000' :
        'rgb(255, 0, 0)';
    assertEquals(expectedColor, caption.style.borderBottomColor);
  },

  testDoesntCallGetCssClassInConstructor() {
    rendererasserts.assertNoGetCssClassCallsInConstructor(
        ColorMenuButtonRenderer);
  },
});
