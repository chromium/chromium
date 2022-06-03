/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Unit tests for goog.position.
 */

/** @suppress {extraProvide} */
goog.module('goog.positioningTest');
goog.setTestOnly();

const Box = goog.require('goog.math.Box');
const Coordinate = goog.require('goog.math.Coordinate');
const Corner = goog.require('goog.positioning.Corner');
const DomHelper = goog.require('goog.dom.DomHelper');
const ExpectedFailures = goog.require('goog.testing.ExpectedFailures');
const Overflow = goog.require('goog.positioning.Overflow');
const OverflowStatus = goog.require('goog.positioning.OverflowStatus');
const Size = goog.require('goog.math.Size');
const TagName = goog.require('goog.dom.TagName');
const browser = goog.require('goog.labs.userAgent.browser');
const dom = goog.require('goog.dom');
const positioning = goog.require('goog.positioning');
const product = goog.require('goog.userAgent.product');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

// Allow positions to be off by one in gecko as it reports scrolling
// offsets in steps of 2.  Otherwise, allow for subpixel difference
// as seen in IE10+
const ALLOWED_OFFSET = userAgent.GECKO ? 1 : 0.1;
// Error bar for positions since some browsers are not super accurate
// in reporting them.
const EPSILON = 2;

let expectedFailures;

const corner = Corner;
const overflow = Overflow;
let testArea;

/** This is used to round pixel values on FF3 Mac. */
function assertRoundedEquals(a, b, c) {
  function round(x) {
    return userAgent.GECKO && (userAgent.MAC || userAgent.X11) ? Math.round(x) :
                                                                 x;
  }
  if (arguments.length == 3) {
    assertRoughlyEquals(a, round(b), round(c), ALLOWED_OFFSET);
  } else {
    assertRoughlyEquals(round(a), round(b), ALLOWED_OFFSET);
  }
}

function createPopupDiv(width, height) {
  const popupDiv = dom.createElement(TagName.DIV);
  popupDiv.style.position = 'absolute';
  style.setSize(popupDiv, width, height);
  style.setPosition(popupDiv, 0 /* left */, 250 /* top */);
  return popupDiv;
}

function newCoord(x, y) {
  return new Coordinate(x, y);
}

function newSize(w, h) {
  return new Size(w, h);
}

function newBox(coord, size) {}
testSuite({
  setUpPage() {
    expectedFailures = new ExpectedFailures();
  },

  setUp() {
    window.scrollTo(0, 0);

    const viewportSize = dom.getViewportSize();
    // Some tests need enough size viewport.
    if (viewportSize.width < 600 || viewportSize.height < 600) {
      window.moveTo(0, 0);
      window.resizeTo(640, 640);
    }

    testArea = dom.getElement('test-area');
  },

  tearDown() {
    expectedFailures.handleTearDown();
    testArea.setAttribute('style', '');
    dom.removeChildren(testArea);
  },

  testPositionAtAnchorLeftToRight() {
    const anchor = document.getElementById('anchor1');
    const popup = document.getElementById('popup1');

    // Anchor top left to top left.
    positioning.positionAtAnchor(
        anchor, corner.TOP_LEFT, popup, corner.TOP_LEFT);
    let anchorRect = style.getBounds(anchor);
    let popupRect = style.getBounds(popup);
    assertRoundedEquals(
        'Left edge of popup should line up with left edge ' +
            'of anchor.',
        anchorRect.left, popupRect.left);
    assertRoundedEquals(
        'Popup should have the same y position as the anchor.', anchorRect.top,
        popupRect.top);

    // Anchor top left to bottom left.
    positioning.positionAtAnchor(
        anchor, corner.BOTTOM_LEFT, popup, corner.TOP_LEFT);
    anchorRect = style.getBounds(anchor);
    popupRect = style.getBounds(popup);
    assertRoundedEquals(
        'Left edge of popup should line up with left edge ' +
            'of anchor.',
        anchorRect.left, popupRect.left);
    assertRoundedEquals(
        'Popup should be positioned just below the anchor.',
        anchorRect.top + anchorRect.height, popupRect.top);

    // Anchor top left to top right.
    positioning.positionAtAnchor(
        anchor, corner.TOP_RIGHT, popup, corner.TOP_LEFT);
    anchorRect = style.getBounds(anchor);
    popupRect = style.getBounds(popup);
    assertRoundedEquals(
        'Popup should be positioned just right of the anchor.',
        anchorRect.left + anchorRect.width, popupRect.left);
    assertRoundedEquals(
        'Popup should have the same y position as the anchor.', anchorRect.top,
        popupRect.top);

    // Anchor top right to bottom right.
    positioning.positionAtAnchor(
        anchor, corner.BOTTOM_RIGHT, popup, corner.TOP_RIGHT);
    anchorRect = style.getBounds(anchor);
    popupRect = style.getBounds(popup);
    assertRoundedEquals(
        'Right edge of popup should line up with right edge ' +
            'of anchor.',
        anchorRect.left + anchorRect.width, popupRect.left + popupRect.width);
    assertRoundedEquals(
        'Popup should be positioned just below the anchor.',
        anchorRect.top + anchorRect.height, popupRect.top);

    // Anchor top start to bottom start.
    positioning.positionAtAnchor(
        anchor, corner.BOTTOM_START, popup, corner.TOP_START);
    anchorRect = style.getBounds(anchor);
    popupRect = style.getBounds(popup);
    assertRoundedEquals(
        'Left edge of popup should line up with left edge ' +
            'of anchor.',
        anchorRect.left, popupRect.left);
    assertRoundedEquals(
        'Popup should be positioned just below the anchor.',
        anchorRect.top + anchorRect.height, popupRect.top);

    // Anchor top center to top center.
    positioning.positionAtAnchor(
        anchor, corner.TOP_CENTER, popup, corner.TOP_CENTER);
    anchorRect = style.getBounds(anchor);
    popupRect = style.getBounds(popup);
    // We use flooring math because we consider 10 and 10.5 to be "equal".
    // IE8 doesn't allow split pixels in positioning and as such this test would
    // fail on it, as Anchor L+W/2 is round while Popup L+W/2 is .5 away.
    assertRoundedEquals(
        'The center of popup should line up with the center ' +
            'of anchor.',
        Math.floor(anchorRect.left + anchorRect.width / 2),
        Math.floor(popupRect.left + popupRect.width / 2));
    assertRoundedEquals(
        'Popup should have the same y position as the anchor.', anchorRect.top,
        popupRect.top);

    // Anchor top center to top left.
    positioning.positionAtAnchor(
        anchor, corner.TOP_CENTER, popup, corner.TOP_LEFT);
    anchorRect = style.getBounds(anchor);
    popupRect = style.getBounds(popup);
    assertRoundedEquals(
        'Left edge of popup should line up with the center ' +
            'of anchor.',
        Math.floor(anchorRect.left + anchorRect.width / 2),
        Math.floor(popupRect.left));
    assertRoundedEquals(
        'Popup should have the same y position as the anchor.', anchorRect.top,
        popupRect.top);

    // Anchor bottom center to top left.
    positioning.positionAtAnchor(
        anchor, corner.BOTTOM_CENTER, popup, corner.TOP_LEFT);
    anchorRect = style.getBounds(anchor);
    popupRect = style.getBounds(popup);
    assertRoundedEquals(
        'Left edge of popup should line up with the middle ' +
            'of anchor.',
        Math.floor(anchorRect.left + anchorRect.width / 2),
        Math.floor(popupRect.left));
    assertRoundedEquals(
        'Popup should be positioned just below the anchor.',
        anchorRect.top + anchorRect.height, popupRect.top);

    // Anchor bottom left to top center.
    positioning.positionAtAnchor(
        anchor, corner.BOTTOM_LEFT, popup, corner.TOP_CENTER);
    anchorRect = style.getBounds(anchor);
    popupRect = style.getBounds(popup);
    assertRoundedEquals(
        'Left edge of popup should line up with the middle ' +
            'of anchor.',
        anchorRect.left, Math.floor(popupRect.left + popupRect.width / 2));
    assertRoundedEquals(
        'Popup should be positioned just below the anchor.',
        anchorRect.top + anchorRect.height, popupRect.top);
  },

  testPositionAtAnchorWithOffset() {
    const anchor = document.getElementById('anchor1');
    const popup = document.getElementById('popup1');

    // Anchor top left to top left with an offset moving the popup away from the
    // anchor.
    positioning.positionAtAnchor(
        anchor, corner.TOP_LEFT, popup, corner.TOP_LEFT, newCoord(-15, -20));
    let anchorRect = style.getBounds(anchor);
    let popupRect = style.getBounds(popup);
    assertRoundedEquals(
        'Left edge of popup should be fifteen pixels from ' +
            'anchor.',
        anchorRect.left, popupRect.left + 15);
    assertRoundedEquals(
        'Top edge of popup should be twenty pixels from anchor.',
        anchorRect.top, popupRect.top + 20);

    // Anchor top left to top left with an offset moving the popup towards the
    // anchor.
    positioning.positionAtAnchor(
        anchor, corner.TOP_LEFT, popup, corner.TOP_LEFT, newCoord(3, 1));
    anchorRect = style.getBounds(anchor);
    popupRect = style.getBounds(popup);
    assertRoundedEquals(
        'Left edge of popup should be three pixels right of ' +
            'the anchor\'s left edge',
        anchorRect.left, popupRect.left - 3);
    assertRoundedEquals(
        'Top edge of popup should be one pixel below of the ' +
            'anchor\'s top edge',
        anchorRect.top, popupRect.top - 1);
  },

  /**
     @suppress {strictPrimitiveOperators} suppression added to enable type
     checking
   */
  testPositionAtAnchorOverflowLeftEdgeRightToLeft() {
    const anchor = document.getElementById('anchor5');
    const popup = document.getElementById('popup5');

    let status = positioning.positionAtAnchor(
        anchor, corner.TOP_LEFT, popup, corner.TOP_RIGHT, undefined, undefined,
        overflow.FAIL_X);
    assertFalse(
        'Positioning operation should have failed.',
        (status & OverflowStatus.FAILED) == 0);

    // Change overflow strategy to ADJUST.
    status = positioning.positionAtAnchor(
        anchor, corner.TOP_LEFT, popup, corner.TOP_RIGHT, undefined, undefined,
        overflow.ADJUST_X);

    // Fails in Chrome because of infrastructure issues, temporarily disabled.
    // See b/4274723.
    expectedFailures.expectFailureFor(product.CHROME);
    try {
      assertTrue(
          'Positioning operation should have been successful.',
          (status & OverflowStatus.FAILED) == 0);
      assertTrue(
          'Positioning operation should have been adjusted.',
          (status & OverflowStatus.ADJUSTED_X) != 0);
    } catch (e) {
      expectedFailures.handleException(e);
    }

    const anchorRect = style.getBounds(anchor);
    const popupRect = style.getBounds(popup);
    /** @suppress {checkTypes} suppression added to enable type checking */
    const parentRect = style.getBounds(anchor.parentNode);
    assertTrue(
        'Position should have been adjusted so that the left edge of ' +
            'the popup is left of the anchor but still within the bounding ' +
            'box of the parent container.',
        anchorRect.left <= popupRect.left <= parentRect.left);
  },

  testPositionAtAnchorWithMargin() {
    const anchor = document.getElementById('anchor1');
    const popup = document.getElementById('popup1');

    // Anchor top left to top left.
    positioning.positionAtAnchor(
        anchor, corner.TOP_LEFT, popup, corner.TOP_LEFT, undefined,
        new Box(1, 2, 3, 4));
    let anchorRect = style.getBounds(anchor);
    let popupRect = style.getBounds(popup);
    assertRoundedEquals(
        'Left edge of popup should be four pixels from anchor.',
        anchorRect.left, popupRect.left - 4);
    assertRoundedEquals(
        'Top edge of popup should be one pixels from anchor.', anchorRect.top,
        popupRect.top - 1);

    // Anchor top right to bottom right.
    positioning.positionAtAnchor(
        anchor, corner.BOTTOM_RIGHT, popup, corner.TOP_RIGHT, undefined,
        new Box(1, 2, 3, 4));
    anchorRect = style.getBounds(anchor);
    popupRect = style.getBounds(popup);

    /** @suppress {visibility} suppression added to enable type checking */
    const visibleAnchorRect = positioning.getVisiblePart_(anchor);

    assertRoundedEquals(
        'Right edge of popup should line up with right edge ' +
            'of anchor.',
        visibleAnchorRect.left + visibleAnchorRect.width,
        popupRect.left + popupRect.width + 2);

    assertRoundedEquals(
        'Popup should be positioned just below the anchor.',
        visibleAnchorRect.top + visibleAnchorRect.height, popupRect.top - 1);
  },

  testPositionAtAnchorRightToLeft() {
    if (userAgent.IE) {
      // These tests fails with IE6.
      // TODO(user): Investigate the reason.
      return;
    }
    if (product.SAFARI) {
      // TODO(user): Disabled so we can get the rest of the Closure test
      // suite running in a continuous build. Will investigate later.
      return;
    }

    const anchor = document.getElementById('anchor2');
    const popup = document.getElementById('popup2');

    // Anchor top left to top left.
    positioning.positionAtAnchor(
        anchor, corner.TOP_LEFT, popup, corner.TOP_LEFT);
    let anchorRect = style.getBounds(anchor);
    let popupRect = style.getBounds(popup);

    assertRoundedEquals(
        'Left edge of popup should line up with left edge ' +
            'of anchor.',
        anchorRect.left, popupRect.left);
    assertRoundedEquals(
        'Popup should have the same y position as the anchor.', anchorRect.top,
        popupRect.top);

    // Anchor top start to bottom start.
    positioning.positionAtAnchor(
        anchor, corner.BOTTOM_START, popup, corner.TOP_START);
    anchorRect = style.getBounds(anchor);
    popupRect = style.getBounds(popup);
    assertRoundedEquals(
        'Right edge of popup should line up with right edge ' +
            'of anchor.',
        anchorRect.left + anchorRect.width, popupRect.left + popupRect.width);
    assertRoundedEquals(
        'Popup should be positioned just below the anchor.',
        anchorRect.top + anchorRect.height, popupRect.top);
  },

  testPositionAtAnchorRightToLeftWithScroll() {
    if (userAgent.IE) {
      // These tests fails with IE6.
      // TODO(user): Investigate the reason.
      return;
    }
    if (product.SAFARI) {
      // TODO(user): Disabled so we can get the rest of the Closure test
      // suite running in a continuous build. Will investigate later.
      return;
    }

    const anchor = document.getElementById('anchor8');
    const popup = document.getElementById('popup8');

    // Anchor top left to top left.
    positioning.positionAtAnchor(
        anchor, corner.TOP_LEFT, popup, corner.TOP_LEFT);
    let anchorRect = style.getBounds(anchor);
    let popupRect = style.getBounds(popup);

    // TODO(joeltine): Chrome 47 has issues with RTL scroll positioning. Remove
    // chrome check when
    // https://code.google.com/p/chromium/issues/detail?id=568706 is resolved.
    if (!browser.isChrome()) {
      assertRoundedEquals(
          'Left edge of popup should line up with left edge ' +
              'of anchor.',
          anchorRect.left, popupRect.left);
    }
    assertRoundedEquals(
        'Popup should have the same y position as the anchor.', anchorRect.top,
        popupRect.top);

    // Anchor top start to bottom start.
    positioning.positionAtAnchor(
        anchor, corner.BOTTOM_START, popup, corner.TOP_START);
    anchorRect = style.getBounds(anchor);
    popupRect = style.getBounds(popup);

    /** @suppress {visibility} suppression added to enable type checking */
    const visibleAnchorRect = positioning.getVisiblePart_(anchor);
    const visibleAnchorBox = visibleAnchorRect.toBox();

    // TODO(joeltine): Chrome 47 has issues with RTL scroll positioning. Remove
    // chrome check when
    // https://code.google.com/p/chromium/issues/detail?id=568706 is resolved.
    if (!browser.isChrome()) {
      assertRoundedEquals(
          'Right edge of popup should line up with right edge ' +
              'of anchor.',
          anchorRect.left + anchorRect.width, popupRect.left + popupRect.width);
    }
    assertRoundedEquals(
        'Popup should be positioned just below the anchor.',
        visibleAnchorBox.bottom, popupRect.top);
  },

  testPositionAtAnchorBodyViewport() {
    const anchor = document.getElementById('anchor1');
    const popup = document.getElementById('popup3');

    // Anchor top left to top left.
    positioning.positionAtAnchor(
        anchor, corner.TOP_LEFT, popup, corner.TOP_LEFT);
    let anchorRect = style.getBounds(anchor);
    let popupRect = style.getBounds(popup);
    assertEquals(
        'Left edge of popup should line up with left edge of anchor.',
        anchorRect.left, popupRect.left);
    assertRoughlyEquals(
        'Popup should have the same y position as the anchor.', anchorRect.top,
        popupRect.top, 1);

    // Anchor top start to bottom right.
    positioning.positionAtAnchor(
        anchor, corner.BOTTOM_RIGHT, popup, corner.TOP_RIGHT);
    anchorRect = style.getBounds(anchor);
    popupRect = style.getBounds(popup);
    assertEquals(
        'Right edge of popup should line up with right edge of anchor.',
        anchorRect.left + anchorRect.width, popupRect.left + popupRect.width);
    assertRoughlyEquals(
        'Popup should be positioned just below the anchor.',
        anchorRect.top + anchorRect.height, popupRect.top, 1);

    // Anchor top right to top left.
    positioning.positionAtAnchor(
        anchor, corner.TOP_LEFT, popup, corner.TOP_RIGHT);
    anchorRect = style.getBounds(anchor);
    popupRect = style.getBounds(popup);
    assertEquals(
        'Right edge of popup should line up with left edge of anchor.',
        anchorRect.left, popupRect.left + popupRect.width);
    assertRoughlyEquals(
        'Popup should have the same y position as the anchor.', anchorRect.top,
        popupRect.top, 1);
  },

  testPositionAtAnchorSpecificViewport() {
    const anchor = document.getElementById('anchor1');
    const popup = document.getElementById('popup3');

    // Anchor top right to top left within outerbox.
    let status = positioning.positionAtAnchor(
        anchor, corner.TOP_LEFT, popup, corner.TOP_RIGHT, undefined, undefined,
        overflow.FAIL_X);
    let anchorRect = style.getBounds(anchor);
    let popupRect = style.getBounds(popup);
    assertTrue(
        'Positioning operation should have been successful.',
        (status & OverflowStatus.FAILED) == 0);
    assertTrue(
        'X position should not have been adjusted.',
        (status & OverflowStatus.ADJUSTED_X) == 0);
    assertTrue(
        'Y position should not have been adjusted.',
        (status & OverflowStatus.ADJUSTED_Y) == 0);
    assertEquals(
        'Right edge of popup should line up with left edge of anchor.',
        anchorRect.left, popupRect.left + popupRect.width);
    assertRoughlyEquals(
        'Popup should have the same y position as the anchor.', anchorRect.top,
        popupRect.top, 1);

    // position again within box1.
    const box = document.getElementById('box1');
    const viewport = style.getBounds(box);
    /** @suppress {checkTypes} suppression added to enable type checking */
    status = positioning.positionAtAnchor(
        anchor, corner.TOP_LEFT, popup, corner.TOP_RIGHT, undefined, undefined,
        overflow.FAIL_X, undefined, viewport);
    assertFalse(
        'Positioning operation should have failed.',
        (status & OverflowStatus.FAILED) == 0);

    // Change overflow strategy to adjust.
    /** @suppress {checkTypes} suppression added to enable type checking */
    status = positioning.positionAtAnchor(
        anchor, corner.TOP_LEFT, popup, corner.TOP_RIGHT, undefined, undefined,
        overflow.ADJUST_X, undefined, viewport);
    anchorRect = style.getBounds(anchor);
    popupRect = style.getBounds(popup);
    assertTrue(
        'Positioning operation should have been successful.',
        (status & OverflowStatus.FAILED) == 0);
    assertFalse(
        'X position should have been adjusted.',
        (status & OverflowStatus.ADJUSTED_X) == 0);
    assertTrue(
        'Y position should not have been adjusted.',
        (status & OverflowStatus.ADJUSTED_Y) == 0);
    assertRoughlyEquals(
        'Left edge of popup should line up with left edge of viewport.',
        viewport.left, popupRect.left, EPSILON);
    assertRoughlyEquals(
        'Popup should have the same y position as the anchor.', anchorRect.top,
        popupRect.top, 1);
  },

  testPositionAtAnchorOutsideViewport() {
    const anchor = document.getElementById('anchor4');
    const popup = document.getElementById('popup1');

    let status = positioning.positionAtAnchor(
        anchor, corner.TOP_LEFT, popup, corner.TOP_RIGHT);
    let anchorRect = style.getBounds(anchor);
    let popupRect = style.getBounds(popup);
    assertTrue(
        'Positioning operation should have been successful.',
        (status & OverflowStatus.FAILED) == 0);
    assertTrue(
        'X position should not have been adjusted.',
        (status & OverflowStatus.ADJUSTED_X) == 0);
    assertTrue(
        'Y position should not have been adjusted.',
        (status & OverflowStatus.ADJUSTED_Y) == 0);

    assertEquals(
        'Right edge of popup should line up with left edge of anchor.',
        anchorRect.left, popupRect.left + popupRect.width);

    // Change overflow strategy to fail.
    status = positioning.positionAtAnchor(
        anchor, corner.BOTTOM_RIGHT, popup, corner.TOP_RIGHT, undefined,
        undefined, overflow.FAIL_X);
    assertFalse(
        'Positioning operation should have failed.',
        (status & OverflowStatus.FAILED) == 0);

    // Change overflow strategy to adjust.
    status = positioning.positionAtAnchor(
        anchor, corner.BOTTOM_RIGHT, popup, corner.TOP_RIGHT, undefined,
        undefined, overflow.ADJUST_X);
    anchorRect = style.getBounds(anchor);
    popupRect = style.getBounds(popup);
    assertTrue(
        'Positioning operation should have been successful.',
        (status & OverflowStatus.FAILED) == 0);
    assertFalse(
        'X position should have been adjusted.',
        (status & OverflowStatus.ADJUSTED_X) == 0);
    assertTrue(
        'Y position should not have been adjusted.',
        (status & OverflowStatus.ADJUSTED_Y) == 0);
    assertRoughlyEquals(
        'Left edge of popup should line up with left edge of viewport.', 0,
        popupRect.left, EPSILON);
    assertEquals(
        'Popup should be positioned just below the anchor.',
        anchorRect.top + anchorRect.height, popupRect.top);
  },

  testAdjustForViewportFailIgnore() {
    /** @suppress {visibility} suppression added to enable type checking */
    const f = positioning.adjustForViewport_;
    const viewport = new Box(100, 200, 200, 100);
    const overflow = Overflow.IGNORE;

    let pos = newCoord(150, 150);
    let size = newSize(50, 50);
    assertEquals(
        'Viewport overflow should be ignored.', OverflowStatus.NONE,
        f(pos, size, viewport, overflow));

    pos = newCoord(150, 150);
    size = newSize(100, 50);
    assertEquals(
        'Viewport overflow should be ignored.', OverflowStatus.NONE,
        f(pos, size, viewport, overflow));

    pos = newCoord(50, 50);
    size = newSize(50, 50);
    assertEquals(
        'Viewport overflow should be ignored.', OverflowStatus.NONE,
        f(pos, size, viewport, overflow));
  },

  testAdjustForViewportFailXY() {
    /** @suppress {visibility} suppression added to enable type checking */
    const f = positioning.adjustForViewport_;
    const viewport = new Box(100, 200, 200, 100);
    const overflow = Overflow.FAIL_X | Overflow.FAIL_Y;

    let pos = newCoord(150, 150);
    let size = newSize(50, 50);
    assertEquals(
        'Element should not overflow viewport.', OverflowStatus.NONE,
        f(pos, size, viewport, overflow));

    pos = newCoord(150, 150);
    size = newSize(100, 50);
    assertEquals(
        'Element should overflow the right edge of viewport.',
        OverflowStatus.FAILED_RIGHT, f(pos, size, viewport, overflow));

    pos = newCoord(150, 150);
    size = newSize(50, 100);
    assertEquals(
        'Element should overflow the bottom edge of viewport.',
        OverflowStatus.FAILED_BOTTOM, f(pos, size, viewport, overflow));

    pos = newCoord(50, 150);
    size = newSize(50, 50);
    assertEquals(
        'Element should overflow the left edge of viewport.',
        OverflowStatus.FAILED_LEFT, f(pos, size, viewport, overflow));

    pos = newCoord(150, 50);
    size = newSize(50, 50);
    assertEquals(
        'Element should overflow the top edge of viewport.',
        OverflowStatus.FAILED_TOP, f(pos, size, viewport, overflow));

    pos = newCoord(50, 50);
    size = newSize(50, 50);
    assertEquals(
        'Element should overflow the left & top edges of viewport.',
        OverflowStatus.FAILED_LEFT | OverflowStatus.FAILED_TOP,
        f(pos, size, viewport, overflow));
  },

  testAdjustForViewportAdjustXFailY() {
    /** @suppress {visibility} suppression added to enable type checking */
    const f = positioning.adjustForViewport_;
    const viewport = new Box(100, 200, 200, 100);
    const overflow = Overflow.ADJUST_X | Overflow.FAIL_Y;

    let pos = newCoord(150, 150);
    let size = newSize(50, 50);
    assertEquals(
        'Element should not overflow viewport.', OverflowStatus.NONE,
        f(pos, size, viewport, overflow));
    assertEquals('X Position should not have been changed.', 150, pos.x);
    assertEquals('Y Position should not have been changed.', 150, pos.y);

    pos = newCoord(150, 150);
    size = newSize(100, 50);
    assertEquals(
        'Element position should be adjusted not to overflow right ' +
            'edge of viewport.',
        OverflowStatus.ADJUSTED_X, f(pos, size, viewport, overflow));
    assertEquals('X Position should be adjusted to 100.', 100, pos.x);
    assertEquals('Y Position should not have been changed.', 150, pos.y);

    pos = newCoord(50, 150);
    size = newSize(100, 50);
    assertEquals(
        'Element position should be adjusted not to overflow left ' +
            'edge of viewport.',
        OverflowStatus.ADJUSTED_X, f(pos, size, viewport, overflow));
    assertEquals('X Position should be adjusted to 100.', 100, pos.x);
    assertEquals('Y Position should not have been changed.', 150, pos.y);

    pos = newCoord(50, 50);
    size = newSize(100, 50);
    assertEquals(
        'Element position should be adjusted not to overflow left ' +
            'edge of viewport, should overflow bottom edge.',
        OverflowStatus.ADJUSTED_X | OverflowStatus.FAILED_TOP,
        f(pos, size, viewport, overflow));
    assertEquals('X Position should be adjusted to 100.', 100, pos.x);
    assertEquals('Y Position should not have been changed.', 50, pos.y);
  },

  testAdjustForViewportResizeHeight() {
    /** @suppress {visibility} suppression added to enable type checking */
    const f = positioning.adjustForViewport_;
    const viewport = new Box(0, 200, 200, 0);
    const overflow = Overflow.RESIZE_HEIGHT;

    let pos = newCoord(150, 150);
    let size = newSize(25, 100);
    assertEquals(
        'Viewport height should be resized.', OverflowStatus.HEIGHT_ADJUSTED,
        f(pos, size, viewport, overflow));
    assertEquals('Height should be resized to 50.', 50, size.height);
    assertTrue(
        'Output box is within viewport',
        viewport.contains(
            new Box(pos.y, pos.x + size.width, pos.y + size.height, pos.x)));

    pos = newCoord(0, 0);
    size = newSize(50, 250);
    assertEquals(
        'Viewport height should be resized.', OverflowStatus.HEIGHT_ADJUSTED,
        f(pos, size, viewport, overflow));
    assertEquals('Height should be resized to 200.', 200, size.height);
    assertTrue(
        'Output box is within viewport',
        viewport.contains(
            new Box(pos.y, pos.x + size.width, pos.y + size.height, pos.x)));

    pos = newCoord(0, -50);
    size = newSize(50, 240);
    assertEquals(
        'Viewport height should be resized.', OverflowStatus.HEIGHT_ADJUSTED,
        f(pos, size, viewport, overflow));
    assertEquals('Height should be resized to 190.', 190, size.height);
    assertTrue(
        'Output box is within viewport',
        viewport.contains(
            new Box(pos.y, pos.x + size.width, pos.y + size.height, pos.x)));

    pos = newCoord(0, -50);
    size = newSize(50, 300);
    assertEquals(
        'Viewport height should be resized.', OverflowStatus.HEIGHT_ADJUSTED,
        f(pos, size, viewport, overflow));
    assertEquals('Height should be resized to 200.', 200, size.height);
    assertTrue(
        'Output box is within viewport',
        viewport.contains(
            new Box(pos.y, pos.x + size.width, pos.y + size.height, pos.x)));

    pos = newCoord(150, 150);
    size = newSize(50, 50);
    assertEquals(
        'No Viewport overflow.', OverflowStatus.NONE,
        f(pos, size, viewport, overflow));
    assertTrue(
        'Output box is within viewport',
        viewport.contains(
            new Box(pos.y, pos.x + size.width, pos.y + size.height, pos.x)));

    const offsetViewport = new Box(100, 200, 300, 0);
    pos = newCoord(0, 50);
    size = newSize(50, 240);
    assertEquals(
        'Viewport height should be resized.', OverflowStatus.HEIGHT_ADJUSTED,
        f(pos, size, offsetViewport, overflow));
    assertEquals('Height should be resized to 190.', 190, size.height);
    assertTrue(
        'Output box is within viewport',
        offsetViewport.contains(
            new Box(pos.y, pos.x + size.width, pos.y + size.height, pos.x)));
  },

  testAdjustForViewportResizeWidth() {
    /** @suppress {visibility} suppression added to enable type checking */
    const f = positioning.adjustForViewport_;
    const viewport = new Box(0, 200, 200, 0);
    const overflow = Overflow.RESIZE_WIDTH;

    let pos = newCoord(150, 150);
    let size = newSize(100, 25);
    assertEquals(
        'Viewport width should be resized.', OverflowStatus.WIDTH_ADJUSTED,
        f(pos, size, viewport, overflow));
    assertEquals('Width should be resized to 50.', 50, size.width);
    assertTrue(
        'Output box is within viewport',
        viewport.contains(
            new Box(pos.y, pos.x + size.width, pos.y + size.height, pos.x)));

    pos = newCoord(0, 0);
    size = newSize(250, 50);
    assertEquals(
        'Viewport width should be resized.', OverflowStatus.WIDTH_ADJUSTED,
        f(pos, size, viewport, overflow));
    assertEquals('Width should be resized to 200.', 200, size.width);
    assertTrue(
        'Output box is within viewport',
        viewport.contains(
            new Box(pos.y, pos.x + size.width, pos.y + size.height, pos.x)));

    pos = newCoord(-50, 0);
    size = newSize(240, 50);
    assertEquals(
        'Viewport width should be resized.', OverflowStatus.WIDTH_ADJUSTED,
        f(pos, size, viewport, overflow));
    assertEquals('Width should be resized to 190.', 190, size.width);
    assertTrue(
        'Output box is within viewport',
        viewport.contains(
            new Box(pos.y, pos.x + size.width, pos.y + size.height, pos.x)));

    pos = newCoord(-50, 0);
    size = newSize(300, 50);
    assertEquals(
        'Viewport width should be resized.', OverflowStatus.WIDTH_ADJUSTED,
        f(pos, size, viewport, overflow));
    assertEquals('Width should be resized to 200.', 200, size.width);
    assertTrue(
        'Output box is within viewport',
        viewport.contains(
            new Box(pos.y, pos.x + size.width, pos.y + size.height, pos.x)));

    pos = newCoord(150, 150);
    size = newSize(50, 50);
    assertEquals(
        'No Viewport overflow.', OverflowStatus.NONE,
        f(pos, size, viewport, overflow));
    assertTrue(
        'Output box is within viewport',
        viewport.contains(
            new Box(pos.y, pos.x + size.width, pos.y + size.height, pos.x)));

    const offsetViewport = new Box(0, 300, 200, 100);
    pos = newCoord(50, 0);
    size = newSize(240, 50);
    assertEquals(
        'Viewport width should be resized.', OverflowStatus.WIDTH_ADJUSTED,
        f(pos, size, offsetViewport, overflow));
    assertEquals('Width should be resized to 190.', 190, size.width);
    assertTrue(
        'Output box is within viewport',
        offsetViewport.contains(
            new Box(pos.y, pos.x + size.width, pos.y + size.height, pos.x)));
  },

  testPositionAtAnchorWithResizeHeight() {
    const anchor = document.getElementById('anchor9');
    const popup = document.getElementById('popup9');
    const box = document.getElementById('box9');
    const viewport = style.getBounds(box);

    const status = positioning.positionAtAnchor(
        anchor, corner.TOP_START, popup, corner.TOP_START,
        new Coordinate(0, -20), null, Overflow.RESIZE_HEIGHT, null,
        viewport.toBox());
    assertEquals(
        'Status should be HEIGHT_ADJUSTED.', OverflowStatus.HEIGHT_ADJUSTED,
        status);

    const TOLERANCE = 0.1;
    // Adjust the viewport to allow some tolerance for subpixel positioning,
    // this is required for this test to pass on IE10,11
    viewport.top -= TOLERANCE;
    viewport.left -= TOLERANCE;

    assertTrue(
        'Popup ' + style.getBounds(popup) + ' not is within viewport' +
            viewport,
        viewport.contains(style.getBounds(popup)));
  },

  testPositionAtCoordinateResizeHeight() {
    const f = positioning.positionAtCoordinate;
    const viewport = new Box(0, 50, 50, 0);
    const overflow = Overflow.RESIZE_HEIGHT | Overflow.ADJUST_Y;
    const popup = document.getElementById('popup1');
    const corner = Corner.BOTTOM_LEFT;

    const pos = newCoord(100, 100);

    assertEquals(
        'Viewport height should be resized.',
        OverflowStatus.HEIGHT_ADJUSTED | OverflowStatus.ADJUSTED_Y,
        f(pos, popup, corner, undefined, viewport, overflow));
    const bounds = style.getSize(popup);
    assertEquals(
        'Height should be resized to the size of the viewport.', 50,
        bounds.height);
  },

  testGetPositionAtCoordinateResizeHeight() {
    const f = positioning.getPositionAtCoordinate;
    const viewport = new Box(0, 50, 50, 0);
    const overflow = Overflow.RESIZE_HEIGHT | Overflow.ADJUST_Y;
    const popup = document.getElementById('popup1');
    const corner = Corner.BOTTOM_LEFT;

    const pos = newCoord(100, 100);
    const size = style.getSize(popup);

    const result = f(pos, size, corner, undefined, viewport, overflow);
    assertEquals(
        'Viewport height should be resized.',
        OverflowStatus.HEIGHT_ADJUSTED | OverflowStatus.ADJUSTED_Y,
        result.status);
    assertEquals(
        'Height should be resized to the size of the viewport.', 50,
        result.rect.height);
  },

  testGetEffectiveCornerLeftToRight() {
    const f = positioning.getEffectiveCorner;
    const el = document.getElementById('ltr');

    assertEquals(
        'TOP_LEFT should be unchanged for ltr.', corner.TOP_LEFT,
        f(el, corner.TOP_LEFT));
    assertEquals(
        'TOP_RIGHT should be unchanged for ltr.', corner.TOP_RIGHT,
        f(el, corner.TOP_RIGHT));
    assertEquals(
        'BOTTOM_LEFT should be unchanged for ltr.', corner.BOTTOM_LEFT,
        f(el, corner.BOTTOM_LEFT));
    assertEquals(
        'BOTTOM_RIGHT should be unchanged for ltr.', corner.BOTTOM_RIGHT,
        f(el, corner.BOTTOM_RIGHT));

    assertEquals(
        'TOP_START should be TOP_LEFT for ltr.', corner.TOP_LEFT,
        f(el, corner.TOP_START));
    assertEquals(
        'TOP_END should be TOP_RIGHT for ltr.', corner.TOP_RIGHT,
        f(el, corner.TOP_END));
    assertEquals(
        'BOTTOM_START should be BOTTOM_LEFT for ltr.', corner.BOTTOM_LEFT,
        f(el, corner.BOTTOM_START));
    assertEquals(
        'BOTTOM_END should be BOTTOM_RIGHT for ltr.', corner.BOTTOM_RIGHT,
        f(el, corner.BOTTOM_END));
  },

  testGetEffectiveCornerRightToLeft() {
    const f = positioning.getEffectiveCorner;
    const el = document.getElementById('rtl');

    assertEquals(
        'TOP_LEFT should be unchanged for rtl.', corner.TOP_LEFT,
        f(el, corner.TOP_LEFT));
    assertEquals(
        'TOP_RIGHT should be unchanged for rtl.', corner.TOP_RIGHT,
        f(el, corner.TOP_RIGHT));
    assertEquals(
        'BOTTOM_LEFT should be unchanged for rtl.', corner.BOTTOM_LEFT,
        f(el, corner.BOTTOM_LEFT));
    assertEquals(
        'BOTTOM_RIGHT should be unchanged for rtl.', corner.BOTTOM_RIGHT,
        f(el, corner.BOTTOM_RIGHT));

    assertEquals(
        'TOP_START should be TOP_RIGHT for rtl.', corner.TOP_RIGHT,
        f(el, corner.TOP_START));
    assertEquals(
        'TOP_END should be TOP_LEFT for rtl.', corner.TOP_LEFT,
        f(el, corner.TOP_END));
    assertEquals(
        'BOTTOM_START should be BOTTOM_RIGHT for rtl.', corner.BOTTOM_RIGHT,
        f(el, corner.BOTTOM_START));
    assertEquals(
        'BOTTOM_END should be BOTTOM_LEFT for rtl.', corner.BOTTOM_LEFT,
        f(el, corner.BOTTOM_END));
  },

  testFlipCornerHorizontal() {
    const f = positioning.flipCornerHorizontal;

    assertEquals(
        'TOP_LEFT should be flipped to TOP_RIGHT.', corner.TOP_RIGHT,
        f(corner.TOP_LEFT));
    assertEquals(
        'TOP_RIGHT should be flipped to TOP_LEFT.', corner.TOP_LEFT,
        f(corner.TOP_RIGHT));
    assertEquals(
        'BOTTOM_LEFT should be flipped to BOTTOM_RIGHT.', corner.BOTTOM_RIGHT,
        f(corner.BOTTOM_LEFT));
    assertEquals(
        'BOTTOM_RIGHT should be flipped to BOTTOM_LEFT.', corner.BOTTOM_LEFT,
        f(corner.BOTTOM_RIGHT));

    assertEquals(
        'TOP_START should be flipped to TOP_END.', corner.TOP_END,
        f(corner.TOP_START));
    assertEquals(
        'TOP_END should be flipped to TOP_START.', corner.TOP_START,
        f(corner.TOP_END));
    assertEquals(
        'BOTTOM_START should be flipped to BOTTOM_END.', corner.BOTTOM_END,
        f(corner.BOTTOM_START));
    assertEquals(
        'BOTTOM_END should be flipped to BOTTOM_START.', corner.BOTTOM_START,
        f(corner.BOTTOM_END));
  },

  testFlipCornerVertical() {
    const f = positioning.flipCornerVertical;

    assertEquals(
        'TOP_LEFT should be flipped to BOTTOM_LEFT.', corner.BOTTOM_LEFT,
        f(corner.TOP_LEFT));
    assertEquals(
        'TOP_RIGHT should be flipped to BOTTOM_RIGHT.', corner.BOTTOM_RIGHT,
        f(corner.TOP_RIGHT));
    assertEquals(
        'BOTTOM_LEFT should be flipped to TOP_LEFT.', corner.TOP_LEFT,
        f(corner.BOTTOM_LEFT));
    assertEquals(
        'BOTTOM_RIGHT should be flipped to TOP_RIGHT.', corner.TOP_RIGHT,
        f(corner.BOTTOM_RIGHT));

    assertEquals(
        'TOP_START should be flipped to BOTTOM_START.', corner.BOTTOM_START,
        f(corner.TOP_START));
    assertEquals(
        'TOP_END should be flipped to BOTTOM_END.', corner.BOTTOM_END,
        f(corner.TOP_END));
    assertEquals(
        'BOTTOM_START should be flipped to TOP_START.', corner.TOP_START,
        f(corner.BOTTOM_START));
    assertEquals(
        'BOTTOM_END should be flipped to TOP_END.', corner.TOP_END,
        f(corner.BOTTOM_END));
  },

  testFlipCorner() {
    const f = positioning.flipCorner;

    assertEquals(
        'TOP_LEFT should be flipped to BOTTOM_RIGHT.', corner.BOTTOM_RIGHT,
        f(corner.TOP_LEFT));
    assertEquals(
        'TOP_RIGHT should be flipped to BOTTOM_LEFT.', corner.BOTTOM_LEFT,
        f(corner.TOP_RIGHT));
    assertEquals(
        'BOTTOM_LEFT should be flipped to TOP_RIGHT.', corner.TOP_RIGHT,
        f(corner.BOTTOM_LEFT));
    assertEquals(
        'BOTTOM_RIGHT should be flipped to TOP_LEFT.', corner.TOP_LEFT,
        f(corner.BOTTOM_RIGHT));

    assertEquals(
        'TOP_START should be flipped to BOTTOM_END.', corner.BOTTOM_END,
        f(corner.TOP_START));
    assertEquals(
        'TOP_END should be flipped to BOTTOM_START.', corner.BOTTOM_START,
        f(corner.TOP_END));
    assertEquals(
        'BOTTOM_START should be flipped to TOP_END.', corner.TOP_END,
        f(corner.BOTTOM_START));
    assertEquals(
        'BOTTOM_END should be flipped to TOP_START.', corner.TOP_START,
        f(corner.BOTTOM_END));
  },

  testPositionAtAnchorFrameViewportStandard() {
    const iframe = document.getElementById('iframe-standard');
    const iframeDoc = dom.getFrameContentDocument(iframe);
    assertTrue(new DomHelper(iframeDoc).isCss1CompatMode());

    new DomHelper(iframeDoc).getDocumentScrollElement().scrollTop = 100;
    const anchor = iframeDoc.getElementById('anchor1');
    const popup = document.getElementById('popup6');

    const status = positioning.positionAtAnchor(
        anchor, corner.TOP_RIGHT, popup, corner.BOTTOM_RIGHT);
    const iframeRect = style.getBounds(iframe);
    const popupRect = style.getBounds(popup);
    assertEquals(
        'Status should not have any ADJUSTED and FAILED.', OverflowStatus.NONE,
        status);
    assertRoundedEquals(
        'Popup should be positioned just above the iframe, ' +
            'not above the anchor element inside the iframe',
        iframeRect.top, popupRect.top + popupRect.height);
  },

  testPositionAtAnchorFrameViewportQuirk() {
    const iframe = document.getElementById('iframe-quirk');
    const iframeDoc = dom.getFrameContentDocument(iframe);
    assertFalse(new DomHelper(iframeDoc).isCss1CompatMode());

    window.scrollTo(0, 100);
    new DomHelper(iframeDoc).getDocumentScrollElement().scrollTop = 100;
    const anchor = iframeDoc.getElementById('anchor1');
    const popup = document.getElementById('popup6');

    const status = positioning.positionAtAnchor(
        anchor, corner.TOP_RIGHT, popup, corner.BOTTOM_RIGHT);
    const iframeRect = style.getBounds(iframe);
    const popupRect = style.getBounds(popup);
    assertEquals(
        'Status should not have any ADJUSTED and FAILED.', OverflowStatus.NONE,
        status);
    assertRoundedEquals(
        'Popup should be positioned just above the iframe, ' +
            'not above the anchor element inside the iframe',
        iframeRect.top, popupRect.top + popupRect.height);
  },

  testPositionAtAnchorFrameViewportWithPopupInScroller() {
    const iframe = document.getElementById('iframe-standard');
    const iframeDoc = dom.getFrameContentDocument(iframe);

    new DomHelper(iframeDoc).getDocumentScrollElement().scrollTop = 100;
    const anchor = iframeDoc.getElementById('anchor1');
    const popup = document.getElementById('popup7');
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    popup.offsetParent.scrollTop = 50;

    const status = positioning.positionAtAnchor(
        anchor, corner.TOP_RIGHT, popup, corner.BOTTOM_RIGHT);
    const iframeRect = style.getBounds(iframe);
    const popupRect = style.getBounds(popup);
    assertEquals(
        'Status should not have any ADJUSTED and FAILED.', OverflowStatus.NONE,
        status);
    assertRoughlyEquals(
        'Popup should be positioned just above the iframe, ' +
            'not above the anchor element inside the iframe',
        iframeRect.top, popupRect.top + popupRect.height, ALLOWED_OFFSET);
  },

  testPositionAtAnchorNestedFrames() {
    const outerIframe = document.getElementById('nested-outer');
    const outerDoc = dom.getFrameContentDocument(outerIframe);
    const popup = outerDoc.getElementById('popup1');
    const innerIframe = outerDoc.getElementById('inner-frame');
    const innerDoc = dom.getFrameContentDocument(innerIframe);
    const anchor = innerDoc.getElementById('anchor1');

    let status = positioning.positionAtAnchor(
        anchor, corner.TOP_LEFT, popup, corner.BOTTOM_LEFT);
    assertEquals(
        'Status should not have any ADJUSTED and FAILED.', OverflowStatus.NONE,
        status);
    let innerIframeRect = style.getBounds(innerIframe);
    let popupRect = style.getBounds(popup);
    assertRoundedEquals(
        'Top of frame should align with bottom of the popup',
        innerIframeRect.top, popupRect.top + popupRect.height);

    // The anchor is scrolled up by 10px.
    // Popup position should be the same as above.
    dom.getWindow(innerDoc).scrollTo(0, 10);
    status = positioning.positionAtAnchor(
        anchor, corner.TOP_LEFT, popup, corner.BOTTOM_LEFT);
    assertEquals(
        'Status should not have any ADJUSTED and FAILED.', OverflowStatus.NONE,
        status);
    innerIframeRect = style.getBounds(innerIframe);
    popupRect = style.getBounds(popup);
    assertRoundedEquals(
        'Top of frame should align with bottom of the popup',
        innerIframeRect.top, popupRect.top + popupRect.height);
  },

  testPositionAtAnchorOffscreen() {
    const offset = 0;
    const anchor = dom.getElement('offscreen-anchor');
    const popup = dom.getElement('popup3');

    positioning.positionAtAnchor(
        anchor, corner.TOP_LEFT, popup, corner.TOP_LEFT, null, null,
        overflow.ADJUST_X | overflow.ADJUST_Y);
    assertObjectEquals(newCoord(offset, offset), style.getPageOffset(popup));

    positioning.positionAtAnchor(
        anchor, corner.TOP_LEFT, popup, corner.TOP_LEFT, null, null,
        overflow.ADJUST_X_EXCEPT_OFFSCREEN | overflow.ADJUST_Y);
    assertObjectEquals(newCoord(-1000, offset), style.getPageOffset(popup));

    positioning.positionAtAnchor(
        anchor, corner.TOP_LEFT, popup, corner.TOP_LEFT, null, null,
        overflow.ADJUST_X | overflow.ADJUST_Y_EXCEPT_OFFSCREEN);
    assertObjectEquals(newCoord(offset, -1000), style.getPageOffset(popup));

    positioning.positionAtAnchor(
        anchor, corner.TOP_LEFT, popup, corner.TOP_LEFT, null, null,
        overflow.ADJUST_X_EXCEPT_OFFSCREEN |
            overflow.ADJUST_Y_EXCEPT_OFFSCREEN);
    assertObjectEquals(newCoord(-1000, -1000), style.getPageOffset(popup));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testPositionAtAnchorWithOverflowScrollOffsetParent() {
    const testAreaOffset = style.getPageOffset(testArea);
    const scrollbarWidth = style.getScrollbarWidth();
    window.scrollTo(testAreaOffset.x, testAreaOffset.y);

    const overflowDiv = dom.createElement(TagName.DIV);
    overflowDiv.style.overflow = 'scroll';
    overflowDiv.style.position = 'relative';
    style.setSize(overflowDiv, 200 /* width */, 100 /* height */);

    const anchor = dom.createElement(TagName.DIV);
    anchor.style.position = 'absolute';
    style.setSize(anchor, 50 /* width */, 50 /* height */);
    style.setPosition(anchor, 300 /* left */, 300 /* top */);

    const popup = createPopupDiv(75 /* width */, 50 /* height */);

    dom.append(testArea, overflowDiv, anchor);
    dom.append(overflowDiv, popup);

    // Popup should always be positioned within the overflowDiv
    style.setPosition(overflowDiv, 0 /* left */, 0 /* top */);
    positioning.positionAtAnchor(
        anchor, corner.TOP_LEFT, popup, corner.TOP_RIGHT, null, null,
        overflow.ADJUST_X | overflow.ADJUST_Y);
    assertObjectRoughlyEquals(
        new Coordinate(
            testAreaOffset.x + 200 - 75 - scrollbarWidth,
            testAreaOffset.y + 100 - 50 - scrollbarWidth),
        style.getPageOffset(popup), 1);

    style.setPosition(overflowDiv, 400 /* left */, 0 /* top */);
    positioning.positionAtAnchor(
        anchor, corner.TOP_RIGHT, popup, corner.TOP_LEFT, null, null,
        overflow.ADJUST_X | overflow.ADJUST_Y);
    assertObjectRoughlyEquals(
        new Coordinate(
            testAreaOffset.x + 400,
            testAreaOffset.y + 100 - 50 - scrollbarWidth),
        style.getPageOffset(popup), 1);

    style.setPosition(overflowDiv, 0 /* left */, 400 /* top */);
    positioning.positionAtAnchor(
        anchor, corner.BOTTOM_LEFT, popup, corner.BOTTOM_RIGHT, null, null,
        overflow.ADJUST_X | overflow.ADJUST_Y);
    assertObjectRoughlyEquals(
        new Coordinate(
            testAreaOffset.x + 200 - 75 - scrollbarWidth,
            testAreaOffset.y + 400),
        style.getPageOffset(popup), 1);

    style.setPosition(overflowDiv, 400 /* left */, 400 /* top */);
    positioning.positionAtAnchor(
        anchor, corner.BOTTOM_RIGHT, popup, corner.BOTTOM_LEFT, null, null,
        overflow.ADJUST_X | overflow.ADJUST_Y);
    assertObjectRoughlyEquals(
        new Coordinate(testAreaOffset.x + 400, testAreaOffset.y + 400),
        style.getPageOffset(popup), 1);

    // No overflow.
    style.setPosition(overflowDiv, 300 - 50 /* left */, 300 /* top */);
    positioning.positionAtAnchor(
        anchor, corner.TOP_LEFT, popup, corner.TOP_RIGHT, null, null,
        overflow.ADJUST_X | overflow.ADJUST_Y);
    assertObjectRoughlyEquals(
        new Coordinate(testAreaOffset.x + 300 - 50, testAreaOffset.y + 300),
        style.getPageOffset(popup), 1);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testPositionAtAnchorWithOverflowHiddenParent() {
    const testAreaOffset = style.getPageOffset(testArea);
    window.scrollTo(testAreaOffset.x, testAreaOffset.y);

    const overflowDiv = dom.createElement(TagName.DIV);
    overflowDiv.style.overflow = 'hidden';
    overflowDiv.style.position = 'relative';
    style.setSize(overflowDiv, 200 /* width */, 100 /* height */);

    const anchor = dom.createElement(TagName.DIV);
    anchor.style.position = 'absolute';
    style.setSize(anchor, 50 /* width */, 50 /* height */);
    style.setPosition(anchor, 300 /* left */, 300 /* top */);

    const popup = createPopupDiv(75 /* width */, 50 /* height */);

    dom.append(testArea, overflowDiv, anchor);
    dom.append(overflowDiv, popup);

    // Popup should always be positioned within the overflowDiv
    style.setPosition(overflowDiv, 0 /* left */, 0 /* top */);
    positioning.positionAtAnchor(
        anchor, corner.TOP_LEFT, popup, corner.TOP_RIGHT, null, null,
        overflow.ADJUST_X | overflow.ADJUST_Y);
    assertObjectRoughlyEquals(
        new Coordinate(
            testAreaOffset.x + 200 - 75, testAreaOffset.y + 100 - 50),
        style.getPageOffset(popup), 1);

    style.setPosition(overflowDiv, 400 /* left */, 0 /* top */);
    positioning.positionAtAnchor(
        anchor, corner.TOP_RIGHT, popup, corner.TOP_LEFT, null, null,
        overflow.ADJUST_X | overflow.ADJUST_Y);
    assertObjectRoughlyEquals(
        new Coordinate(testAreaOffset.x + 400, testAreaOffset.y + 100 - 50),
        style.getPageOffset(popup), 1);

    style.setPosition(overflowDiv, 0 /* left */, 400 /* top */);
    positioning.positionAtAnchor(
        anchor, corner.BOTTOM_LEFT, popup, corner.BOTTOM_RIGHT, null, null,
        overflow.ADJUST_X | overflow.ADJUST_Y);
    assertObjectRoughlyEquals(
        new Coordinate(testAreaOffset.x + 200 - 75, testAreaOffset.y + 400),
        style.getPageOffset(popup), 1);

    style.setPosition(overflowDiv, 400 /* left */, 400 /* top */);
    positioning.positionAtAnchor(
        anchor, corner.BOTTOM_RIGHT, popup, corner.BOTTOM_LEFT, null, null,
        overflow.ADJUST_X | overflow.ADJUST_Y);
    assertObjectRoughlyEquals(
        new Coordinate(testAreaOffset.x + 400, testAreaOffset.y + 400),
        style.getPageOffset(popup), 1);

    // No overflow.
    style.setPosition(overflowDiv, 300 - 50 /* left */, 300 /* top */);
    positioning.positionAtAnchor(
        anchor, corner.TOP_LEFT, popup, corner.TOP_RIGHT, null, null,
        overflow.ADJUST_X | overflow.ADJUST_Y);
    assertObjectRoughlyEquals(
        new Coordinate(testAreaOffset.x + 300 - 50, testAreaOffset.y + 300),
        style.getPageOffset(popup), 1);
  },
});
