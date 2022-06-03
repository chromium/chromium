/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Tests for `ClientPosition`
 */

goog.module('goog.positioning.clientPositionTest');
goog.setTestOnly();

const ClientPosition = goog.require('goog.positioning.ClientPosition');
const Corner = goog.require('goog.positioning.Corner');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');

/**
 * Prefabricated popup element for convenient. This is created during
 * setUp and is not attached to the document at the beginning of the
 * test.
 * @type {Element}
 */
let popupElement;
let testArea;
const POPUP_HEIGHT = 100;
const POPUP_WIDTH = 150;

function assertPageOffset(expectedX, expectedY, el) {
  const offsetCoordinate = style.getPageOffset(el);
  assertEquals('x-coord page offset is wrong.', expectedX, offsetCoordinate.x);
  assertEquals('y-coord page offset is wrong.', expectedY, offsetCoordinate.y);
}
testSuite({
  setUp() {
    testArea = dom.getElement('test-area');

    // Enlarges the test area to 5000x5000px so that we can be confident
    // that scrolling the document to some small (x,y) value would work.
    style.setSize(testArea, 5000, 5000);

    window.scrollTo(0, 0);

    popupElement = dom.createDom(TagName.DIV);
    style.setSize(popupElement, POPUP_WIDTH, POPUP_HEIGHT);
    popupElement.style.position = 'absolute';

    // For ease of debugging.
    popupElement.style.background = 'blue';
  },

  tearDown() {
    popupElement = null;
    dom.removeChildren(testArea);
    testArea.setAttribute('style', '');
  },

  testClientPositionWithZeroViewportOffset() {
    dom.appendChild(testArea, popupElement);

    const x = 300;
    const y = 200;
    const pos = new ClientPosition(x, y);

    pos.reposition(popupElement, Corner.TOP_LEFT);
    assertPageOffset(x, y, popupElement);

    pos.reposition(popupElement, Corner.TOP_RIGHT);
    assertPageOffset(x - POPUP_WIDTH, y, popupElement);

    pos.reposition(popupElement, Corner.BOTTOM_LEFT);
    assertPageOffset(x, y - POPUP_HEIGHT, popupElement);

    pos.reposition(popupElement, Corner.BOTTOM_RIGHT);
    assertPageOffset(x - POPUP_WIDTH, y - POPUP_HEIGHT, popupElement);
  },

  testClientPositionWithSomeViewportOffset() {
    dom.appendChild(testArea, popupElement);

    const x = 300;
    const y = 200;
    const scrollX = 135;
    const scrollY = 270;
    window.scrollTo(scrollX, scrollY);

    const pos = new ClientPosition(x, y);
    pos.reposition(popupElement, Corner.TOP_LEFT);
    assertPageOffset(scrollX + x, scrollY + y, popupElement);
  },

  testClientPositionWithPositionContext() {
    const contextAbsoluteX = 90;
    const contextAbsoluteY = 110;
    const x = 300;
    const y = 200;

    const contextElement = dom.createDom(TagName.DIV, undefined, popupElement);
    style.setPosition(contextElement, contextAbsoluteX, contextAbsoluteY);
    contextElement.style.position = 'absolute';
    dom.appendChild(testArea, contextElement);

    const pos = new ClientPosition(x, y);
    pos.reposition(popupElement, Corner.TOP_LEFT);
    assertPageOffset(x, y, popupElement);
  },
});
