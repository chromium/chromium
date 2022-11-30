/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.positioning.ViewportClientPositionTest');
goog.setTestOnly();

const Corner = goog.require('goog.positioning.Corner');
const Overflow = goog.require('goog.positioning.Overflow');
const ViewportClientPosition = goog.require('goog.positioning.ViewportClientPosition');
const googDom = goog.require('goog.dom');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

let anchor;
let dom;
let frameRect;
let popup;
let viewportSize;

// Allow positions to be off by one in gecko as it reports scrolling
// offsets in steps of 2.
const ALLOWED_OFFSET = userAgent.GECKO ? 1 : 0;

testSuite({
  setUp() {
    const frame = document.getElementById('frame1');
    const doc = googDom.getFrameContentDocument(frame);

    dom = googDom.getDomHelper(doc);
    viewportSize = dom.getViewportSize();
    anchor = dom.getElement('anchor');
    popup = dom.getElement('popup');
    popup.style.overflowY = 'visible';
    style.setSize(popup, 20, 20);
    frameRect = style.getVisibleRectForElement(doc.body);
  },

  testPositionAtCoordinateTopLeft() {
    const pos = new ViewportClientPosition(100, 100);
    pos.reposition(popup, Corner.TOP_LEFT);

    const offset = style.getPageOffset(popup);
    assertEquals(
        'Left edge of popup should be at specified x coordinate.', 100,
        offset.x);
    assertEquals(
        'Top edge of popup should be at specified y coordinate.', 100,
        offset.y);
  },

  testPositionAtCoordinateBottomRight() {
    const pos = new ViewportClientPosition(100, 100);
    pos.reposition(popup, Corner.BOTTOM_RIGHT);

    const bounds = style.getBounds(popup);
    assertEquals(
        'Right edge of popup should be at specified x coordinate.', 100,
        bounds.left + bounds.width);
    assertEquals(
        'Bottom edge of popup should be at specified x coordinate.', 100,
        bounds.top + bounds.height);
  },

  testPositionAtCoordinateTopLeftWithScroll() {
    dom.getDocument().body.style.paddingTop = '300px';
    dom.getDocument().body.style.height = '3000px';
    dom.getDocumentScrollElement().scrollTop = 50;
    dom.getDocument().body.scrollTop = 50;

    const pos = new ViewportClientPosition(0, 0);
    pos.reposition(popup, Corner.TOP_LEFT);

    let offset = style.getPageOffset(popup);
    assertEquals(
        'Left edge of popup should be at specified x coordinate.', 0, offset.x);
    assertTrue(
        'Top edge of popup should be at specified y coordinate ' +
            'adjusted for scroll.',
        Math.abs(offset.y - 50) <= ALLOWED_OFFSET);

    dom.getDocument().body.style.paddingLeft = '1000px';
    dom.getDocumentScrollElement().scrollLeft = 500;

    pos.reposition(popup, Corner.TOP_LEFT);
    offset = style.getPageOffset(popup);
    assertTrue(
        'Left edge of popup should be at specified x coordinate ' +
            'adjusted for scroll.',
        Math.abs(offset.x - 500) <= ALLOWED_OFFSET);

    dom.getDocumentScrollElement().scrollLeft = 0;
    dom.getDocumentScrollElement().scrollTop = 0;
    dom.getDocument().body.style.paddingLeft = '';
    dom.getDocument().body.style.paddingTop = '';

    pos.reposition(popup, Corner.TOP_LEFT);
    offset = style.getPageOffset(popup);
    assertEquals(
        'Left edge of popup should be at specified x coordinate.', 0, offset.x);
    assertEquals(
        'Top edge of popup should be at specified y coordinate.', 0, offset.y);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testOverflowRightFlipHor() {
    const pos = new ViewportClientPosition(frameRect.right, 100);
    pos.reposition(popup, Corner.TOP_LEFT);

    const offset = style.getPageOffset(popup);
    assertEquals(
        'Left edge of popup should have been adjusted so that it ' +
            'fits inside the viewport.',
        frameRect.right - popup.offsetWidth, offset.x);
    assertEquals(
        'Top edge of popup should be at specified y coordinate.', 100,
        offset.y);
  },

  testOverflowTopFlipVer() {
    const pos = new ViewportClientPosition(100, 0);
    pos.reposition(popup, Corner.TOP_RIGHT);

    const offset = style.getPageOffset(popup);
    assertEquals(
        'Left edge of popup should be at specified x coordinate.', 80,
        offset.x);
    assertEquals(
        'Top edge of popup should have been adjusted so that it ' +
            'fits inside the viewport.',
        0, offset.y);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testOverflowBottomRightFlipBoth() {
    const pos = new ViewportClientPosition(frameRect.right, frameRect.bottom);
    pos.reposition(popup, Corner.TOP_LEFT);

    const offset = style.getPageOffset(popup);
    assertEquals(
        'Left edge of popup should have been adjusted so that it ' +
            'fits inside the viewport.',
        frameRect.right - popup.offsetWidth, offset.x);
    assertEquals(
        'Top edge of popup should have been adjusted so that it ' +
            'fits inside the viewport.',
        frameRect.bottom - popup.offsetHeight, offset.y);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testLastRespotOverflow() {
    const large = 2000;
    style.setSize(popup, 20, large);
    popup.style.overflowY = 'auto';

    const pos = new ViewportClientPosition(0, 0);
    pos.reposition(popup, Corner.TOP_LEFT);

    assertEquals(large, popup.offsetHeight);
    pos.setLastResortOverflow(Overflow.RESIZE_HEIGHT);
    pos.reposition(popup, Corner.TOP_LEFT);
    assertNotEquals(large, popup.offsetHeight);
  },
});
