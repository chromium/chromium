/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.positioning.MenuAnchoredPositionTest');
goog.setTestOnly();

const Corner = goog.require('goog.positioning.Corner');
const MenuAnchoredPosition = goog.require('goog.positioning.MenuAnchoredPosition');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');

let offscreenAnchor;
let onscreenAnchor;
let customAnchor;
let menu;
let savedMenuHtml;

testSuite({
  setUp() {
    offscreenAnchor = dom.getElement('offscreen-anchor');
    onscreenAnchor = dom.getElement('onscreen-anchor');
    customAnchor = dom.getElement('custom-anchor');
    customAnchor.style.left = '0';
    customAnchor.style.top = '0';

    menu = dom.getElement('menu');
    savedMenuHtml = menu.innerHTML;
    menu.style.left = '20px';
    menu.style.top = '20px';
  },

  tearDown() {
    menu.innerHTML = savedMenuHtml;
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testRepositionWithAdjustAndOnscreenAnchor() {
    // Add so many children that it can't possibly fit onscreen.
    for (let i = 0; i < 200; i++) {
      menu.appendChild(dom.createDom(TagName.DIV, null, `New Item ${i}`));
    }

    const pos = new MenuAnchoredPosition(onscreenAnchor, Corner.TOP_LEFT, true);
    pos.reposition(menu, Corner.TOP_LEFT);

    const offset = 0;
    assertEquals(offset, menu.offsetTop);
    assertEquals(5, menu.offsetLeft);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testRepositionWithAdjustAndOffscreenAnchor() {
    // This does not get adjusted because it's too far offscreen.
    const pos =
        new MenuAnchoredPosition(offscreenAnchor, Corner.TOP_LEFT, true);
    pos.reposition(menu, Corner.TOP_LEFT);

    assertEquals(-1000, menu.offsetTop);
    assertEquals(-1000, menu.offsetLeft);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testRespositionFailoverEvenWhenResizeHeightIsOn() {
    const pos =
        new MenuAnchoredPosition(onscreenAnchor, Corner.TOP_LEFT, true, true);
    pos.reposition(menu, Corner.TOP_RIGHT);

    // The menu should not get positioned offscreen.
    assertEquals(5, menu.offsetTop);
    assertEquals(5, menu.offsetLeft);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testRepositionToBottomLeftWhenBottomFailsAndRightFailsAndResizeOn() {
    const pageSize = dom.getViewportSize();
    customAnchor.style.left = (pageSize.width - 10) + 'px';

    // Add so many children that it can't possibly fit onscreen.
    for (let i = 0; i < 200; i++) {
      menu.appendChild(dom.createDom(TagName.DIV, null, `New Item ${i}`));
    }

    const pos =
        new MenuAnchoredPosition(customAnchor, Corner.TOP_LEFT, true, true);
    pos.reposition(menu, Corner.TOP_LEFT);
    assertEquals(menu.offsetLeft + menu.offsetWidth, customAnchor.offsetLeft);
  },
});
