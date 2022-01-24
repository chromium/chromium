/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.styleTest');
goog.setTestOnly();

const TagName = goog.require('goog.dom.TagName');
const googDom = goog.require('goog.dom');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');
const testingStyle = goog.require('goog.testing.style');

let div1;
let div2;

testSuite({
  setUp() {
    const createDiv = (color) => {
      const div = googDom.createDom(TagName.DIV, {
        style: 'position:absolute;top:0;left:0;' +
            'width:200px;height:100px;' +
            'background-color:' + color,
        innerHTML: 'abc',
      });
      document.body.appendChild(div);
      return div;
    };

    div1 = createDiv('#EEE');
    div2 = createDiv('#F00');
  },

  tearDown() {
    if (div1.parentNode) div1.parentNode.removeChild(div1);
    if (div2.parentNode) div2.parentNode.removeChild(div2);

    div1 = null;
    div2 = null;
  },

  testIsVisible() {
    assertTrue(
        'The div should be detected as visible.', testingStyle.isVisible(div1));

    // Tests with hidden element
    style.setElementShown(div1, false /* display */);
    assertFalse(
        'The div should be detected as not visible.',
        testingStyle.isVisible(div1));

    // Detached elements are invisible.
    const detached = document.createElement('div');
    assertFalse(testingStyle.isVisible(detached));
  },

  testIsOnScreen() {
    const el = googDom.createElement(TagName.DIV);
    document.body.appendChild(el);

    const dom = googDom.getDomHelper(el);
    const winScroll = dom.getDocumentScroll();
    const winSize = dom.getViewportSize();

    el.style.position = 'absolute';
    style.setSize(el, 100, 100);

    style.setPosition(el, winScroll.x, winScroll.y);
    assertTrue(
        'An element fully on the screen is on screen.',
        testingStyle.isOnScreen(el));

    style.setPosition(el, winScroll.x - 10, winScroll.y - 10);
    assertTrue(
        'An element partially off the top-left of the screen is on screen.',
        testingStyle.isOnScreen(el));

    style.setPosition(el, winScroll.x - 150, winScroll.y - 10);
    assertFalse(
        'An element completely off the top of the screen is off screen.',
        testingStyle.isOnScreen(el));

    style.setPosition(el, winScroll.x - 10, winScroll.y - 150);
    assertFalse(
        'An element completely off the left of the screen is off screen.',
        testingStyle.isOnScreen(el));

    style.setPosition(el, winScroll.x + winSize.width + 1, winScroll.y);
    assertFalse(
        'An element completely off the right of the screen is off screen.',
        testingStyle.isOnScreen(el));

    style.setPosition(el, winScroll.x, winScroll.y + winSize.height + 1);
    assertFalse(
        'An element completely off the bottom of the screen is off screen.',
        testingStyle.isOnScreen(el));

    style.setPosition(el, winScroll.x + winSize.width - 10, winScroll.y);
    assertTrue(
        'An element partially off the right of the screen is on screen.',
        testingStyle.isOnScreen(el));

    style.setPosition(el, winScroll.x, winScroll.y + winSize.height - 10);
    assertTrue(
        'An element partially off the bottom of the screen is on screen.',
        testingStyle.isOnScreen(el));

    const el2 = googDom.createElement(TagName.DIV);
    el2.style.position = 'absolute';
    style.setSize(el2, 100, 100);
    style.setPosition(el2, winScroll.x, winScroll.y);
    assertFalse(
        'An element not in the DOM is not on screen.',
        testingStyle.isOnScreen(el2));
  },

  testHasVisibleDimensions() {
    style.setSize(div1, 0, 0);
    assertFalse(
        '0x0 should not be considered visible dimensions.',
        testingStyle.hasVisibleDimensions(div1));
    style.setSize(div1, 10, 0);
    assertFalse(
        '10x0 should not be considered visible dimensions.',
        testingStyle.hasVisibleDimensions(div1));
    style.setSize(div1, 10, 10);
    assertTrue(
        '10x10 should be considered visible dimensions.',
        testingStyle.hasVisibleDimensions(div1));
    style.setSize(div1, 0, 10);
    assertFalse(
        '0x10 should not be considered visible dimensions.',
        testingStyle.hasVisibleDimensions(div1));
  },

  testIntersects() {
    // No intersection
    style.setPosition(div1, 0, 0);
    style.setPosition(div2, 500, 500);
    assertFalse(
        'The divs should not be determined to itersect.',
        testingStyle.intersects(div1, div2));

    // Some intersection
    style.setPosition(div1, 0, 0);
    style.setPosition(div2, 50, 50);
    assertTrue(
        'The divs should be determined to itersect.',
        testingStyle.intersects(div1, div2));

    // Completely superimposed.
    style.setPosition(div1, 0, 0);
    style.setPosition(div2, 0, 0);
    assertTrue(
        'The divs should be determined to itersect.',
        testingStyle.intersects(div1, div2));
  },
});
