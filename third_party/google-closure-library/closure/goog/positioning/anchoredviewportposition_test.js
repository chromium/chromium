/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.positioning.AnchoredViewportPositionTest');
goog.setTestOnly();

const AnchoredViewportPosition = goog.require('goog.positioning.AnchoredViewportPosition');
const Box = goog.require('goog.math.Box');
const Corner = goog.require('goog.positioning.Corner');
const OverflowStatus = goog.require('goog.positioning.OverflowStatus');
const googDom = goog.require('goog.dom');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');

let anchor;
let doc;
let dom;
let frame;
let popup;
let viewportSize;

// The frame has enough space at the bottom of the anchor.

// No enough space at the bottom, but at the top.

// Not enough space either at the bottom or right but there is enough space at
// top left.

// Enough space at neither the bottom nor the top.  Adjustment flag is false.

// Enough space at neither the bottom nor the top.  Adjustment flag is true.

// No space to fit, so uses fallback.

// Initially no space to fit above, then changes to have room.

testSuite({
  setUp() {
    frame = document.getElementById('frame1');
    doc = googDom.getFrameContentDocument(frame);
    dom = googDom.getDomHelper(doc);
    viewportSize = dom.getViewportSize();
    anchor = dom.getElement('anchor');
    popup = dom.getElement('popup');
    style.setSize(popup, 20, 20);
  },

  testRepositionBottom() {
    const avp = new AnchoredViewportPosition(anchor, Corner.BOTTOM_LEFT, false);
    style.setSize(anchor, 100, 100);
    style.setPosition(anchor, 0, 0);
    assertTrue(viewportSize.height >= 100 + 20);

    avp.reposition(popup, Corner.TOP_LEFT);
    const anchorRect = style.getBounds(anchor);
    assertEquals(
        anchorRect.top + anchorRect.height, style.getPageOffset(popup).y);
  },

  testRepositionTop() {
    const avp = new AnchoredViewportPosition(anchor, Corner.BOTTOM_LEFT, false);
    const newTop = viewportSize.height - 100;
    style.setSize(anchor, 100, 100);
    style.setPosition(anchor, 50, newTop);
    assertTrue(newTop >= 20);

    avp.reposition(popup, Corner.TOP_LEFT);
    const anchorRect = style.getBounds(anchor);
    const popupRect = style.getBounds(popup);
    assertEquals(anchorRect.top, popupRect.top + popupRect.height);
  },

  testRepositionBottomRight() {
    const avp =
        new AnchoredViewportPosition(anchor, Corner.BOTTOM_RIGHT, false);
    style.setSize(anchor, 100, 100);
    style.setPosition(
        anchor, viewportSize.width - 110, viewportSize.height - 110);

    avp.reposition(popup, Corner.TOP_LEFT);
    const anchorRect = style.getBounds(anchor);
    const popupRect = style.getBounds(popup);
    assertEquals(anchorRect.top, popupRect.top + popupRect.height);
    assertEquals(anchorRect.left, popupRect.left + popupRect.width);
  },

  testRepositionNoSpaceWithoutAdjustment() {
    const avp = new AnchoredViewportPosition(anchor, Corner.BOTTOM_LEFT, false);
    style.setPosition(anchor, 50, 10);
    style.setSize(anchor, 100, viewportSize.height - 20);

    avp.reposition(popup, Corner.TOP_LEFT);
    const anchorRect = style.getBounds(anchor);
    const popupRect = style.getBounds(popup);
    assertEquals(anchorRect.top + anchorRect.height, popupRect.top);
    assertTrue(popupRect.top + popupRect.height > viewportSize.height);
  },

  testRepositionNoSpaceWithAdjustment() {
    const avp = new AnchoredViewportPosition(anchor, Corner.BOTTOM_LEFT, true);
    style.setPosition(anchor, 50, 10);
    style.setSize(anchor, 100, viewportSize.height - 20);

    avp.reposition(popup, Corner.TOP_LEFT);
    const anchorRect = style.getBounds(anchor);
    const popupRect = style.getBounds(popup);
    assertTrue(anchorRect.top + anchorRect.height > popupRect.top);
    assertEquals(viewportSize.height, popupRect.top + popupRect.height);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testAdjustCorner() {
    const avp = new AnchoredViewportPosition(anchor, Corner.BOTTOM_LEFT);
    assertEquals(Corner.BOTTOM_LEFT, avp.adjustCorner(0, Corner.BOTTOM_LEFT));
    assertEquals(
        Corner.BOTTOM_RIGHT,
        avp.adjustCorner(OverflowStatus.FAILED_HORIZONTAL, Corner.BOTTOM_LEFT));
    assertEquals(
        Corner.TOP_LEFT,
        avp.adjustCorner(OverflowStatus.FAILED_VERTICAL, Corner.BOTTOM_LEFT));
    assertEquals(
        Corner.TOP_RIGHT,
        avp.adjustCorner(
            OverflowStatus.FAILED_VERTICAL | OverflowStatus.FAILED_HORIZONTAL,
            Corner.BOTTOM_LEFT));
  },

  testOverflowConstraint() {
    const tinyBox = new Box(0, 0, 0, 0);
    const avp = new AnchoredViewportPosition(
        anchor, Corner.BOTTOM_LEFT, false, tinyBox);
    assertEquals(tinyBox, avp.getOverflowConstraint());

    style.setSize(anchor, 50, 50);
    style.setPosition(anchor, 80, 80);
    avp.reposition(popup, Corner.TOP_LEFT);

    const anchorRect = style.getBounds(anchor);
    const popupRect = style.getBounds(popup);
    assertEquals(anchorRect.left, popupRect.left);
    assertEquals(anchorRect.top + anchorRect.height, popupRect.top);
  },

  testChangeOverflowConstraint() {
    const tinyBox = new Box(0, 0, 0, 0);
    const avp = new AnchoredViewportPosition(
        anchor, Corner.BOTTOM_LEFT, false, tinyBox);
    assertEquals(tinyBox, avp.getOverflowConstraint());

    style.setSize(anchor, 50, 50);
    style.setPosition(anchor, 80, 80);

    avp.reposition(popup, Corner.TOP_LEFT);
    let popupRect = style.getBounds(popup);
    assertNotEquals(60, popupRect.top);

    const movedBox = new Box(60, 100, 100, 60);
    avp.setOverflowConstraint(movedBox);
    assertEquals(movedBox, avp.getOverflowConstraint());

    avp.reposition(popup, Corner.TOP_LEFT);
    popupRect = style.getBounds(popup);
    assertEquals(80, popupRect.left);
    assertEquals(60, popupRect.top);
  },
});
