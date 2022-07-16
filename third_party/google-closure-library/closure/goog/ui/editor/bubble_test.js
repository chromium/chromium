/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.editor.BubbleTest');
goog.setTestOnly();

const Bubble = goog.require('goog.ui.editor.Bubble');
const Component = goog.require('goog.ui.Component');
const Corner = goog.require('goog.positioning.Corner');
const EventType = goog.require('goog.events.EventType');
const OverflowStatus = goog.require('goog.positioning.OverflowStatus');
const TagName = goog.require('goog.dom.TagName');
const TestHelper = goog.require('goog.testing.editor.TestHelper');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const googString = goog.require('goog.string');
const product = goog.require('goog.userAgent.product');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');

let testHelper;
let fieldDiv;
let bubble;
let link;
let link2;
let panelId;

/**
 * This is a helper function for setting up the target element with a
 * given direction.
 * @param {string} dir The direction of the target element, 'ltr' or 'rtl'.
 * @param {boolean=} preferTopPosition Whether to prefer placing the bubble
 *     above the element instead of below it. Defaults to preferring below.
 */
function prepareTargetWithGivenDirection(dir, preferTopPosition = undefined) {
  style.setStyle(document.body, 'direction', dir);

  fieldDiv.style.direction = dir;
  fieldDiv.innerHTML = '<a href="http://www.google.com">Google</a>';
  link = fieldDiv.firstChild;

  panelId = bubble.addPanel('A', 'Link', link, (el) => {
    el.innerHTML = '<div style="border:1px solid blue;">B</div>';
  }, preferTopPosition);
}

/**
 * This is a helper function for getting the expected position of the bubble.
 * (align to the right or the left of the target element).  Align left by
 * default and align right if alignRight is true. The expected Y is
 * unaffected by alignment.
 * @param {boolean=} alignRight Sets the expected alignment to be right.
 */
function getExpectedBubblePositionWithGivenAlignment(alignRight = undefined) {
  const targetPosition = style.getFramedPageOffset(link, window);
  const targetWidth = link.offsetWidth;
  /** @suppress {visibility} suppression added to enable type checking */
  const bubbleSize = style.getSize(bubble.bubbleContainer_);
  const expectedBubbleX = alignRight ?
      targetPosition.x + targetWidth - bubbleSize.width :
      targetPosition.x;
  /** @suppress {visibility} suppression added to enable type checking */
  const expectedBubbleY =
      link.offsetHeight + targetPosition.y + Bubble.VERTICAL_CLEARANCE_;

  return {x: expectedBubbleX, y: expectedBubbleY};
}

testSuite({
  setUpPage() {
    fieldDiv = dom.getElement('field');
    const viewportSize = dom.getViewportSize();
    // Some tests depends on enough size of viewport.
    if (viewportSize.width < 600 || viewportSize.height < 440) {
      window.moveTo(0, 0);
      window.resizeTo(640, 480);
    }
  },

  setUp() {
    testHelper = new TestHelper(fieldDiv);
    testHelper.setUpEditableElement();

    bubble = new Bubble(document.body, 999);

    fieldDiv.innerHTML = '<a href="http://www.google.com">Google</a>' +
        '<a href="http://www.google.com">Google2</a>';
    link = fieldDiv.firstChild;
    link2 = fieldDiv.lastChild;

    window.scrollTo(0, 0);
    style.setStyle(document.body, 'direction', 'ltr');
    style.setStyle(document.getElementById('field'), 'position', 'static');
  },

  tearDown() {
    if (panelId) {
      bubble.removePanel(panelId);
      panelId = null;
    }
    testHelper.tearDownEditableElement();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testCreateBubbleWithLinkPanel() {
    const id = googString.createUniqueString();
    panelId = bubble.addPanel('A', 'Link', link, (container) => {
      container.innerHTML = `<span id="${id}">Test</span>`;
    });
    assertNotNull('Bubble should be created', bubble.bubbleContents_);
    assertNotNull('Added element should be present', dom.getElement(id));
    assertTrue('Bubble should be visible', bubble.isVisible());
  },

  testCloseBubble() {
    this.testCreateBubbleWithLinkPanel();

    let count = 0;
    events.listen(bubble, Component.EventType.HIDE, () => {
      count++;
    });

    bubble.removePanel(panelId);
    panelId = null;

    assertFalse('Bubble should not be visible', bubble.isVisible());
    assertEquals('Hide event should be dispatched', 1, count);
  },

  testCloseBox() {
    this.testCreateBubbleWithLinkPanel();

    let count = 0;
    events.listen(bubble, Component.EventType.HIDE, () => {
      count++;
    });

    /** @suppress {visibility} suppression added to enable type checking */
    const closeBox = dom.getElementsByTagNameAndClass(
        TagName.DIV, 'tr_bubble_closebox', bubble.bubbleContainer_)[0];
    testingEvents.fireClickSequence(closeBox);
    panelId = null;

    assertFalse('Bubble should not be visible', bubble.isVisible());
    assertEquals('Hide event should be dispatched', 1, count);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testViewPortSizeMonitorEvent() {
    this.testCreateBubbleWithLinkPanel();

    let numCalled = 0;
    bubble.reposition = () => {
      numCalled++;
    };

    assertNotUndefined(
        'viewPortSizeMonitor_ should not be undefined',
        bubble.viewPortSizeMonitor_);
    bubble.viewPortSizeMonitor_.dispatchEvent(EventType.RESIZE);

    assertEquals('reposition not called', 1, numCalled);
  },

  testBubblePositionPreferTop() {
    let called = false;
    /** @suppress {visibility} suppression added to enable type checking */
    bubble.positionAtAnchor_ = (targetCorner, bubbleCorner, overflow) => {
      called = true;

      // Assert that the bubble is positioned below the target.
      assertEquals(Corner.TOP_START, targetCorner);
      assertEquals(Corner.BOTTOM_START, bubbleCorner);

      return OverflowStatus.NONE;
    };
    prepareTargetWithGivenDirection('ltr', true);
    assertTrue(called);
  },

  testBubblePosition() {
    panelId = bubble.addPanel('A', 'Link', link, goog.nullFunction);
    /** @suppress {visibility} suppression added to enable type checking */
    const CLEARANCE = Bubble.VERTICAL_CLEARANCE_;
    /** @suppress {visibility} suppression added to enable type checking */
    const bubbleContainer = bubble.bubbleContainer_;

    // The field is at a normal place, alomost the top of the viewport, and
    // there is enough space at the bottom of the field.
    const targetPos = style.getFramedPageOffset(link, window);
    const targetSize = style.getSize(link);
    /** @suppress {checkTypes} suppression added to enable type checking */
    let pos = style.getFramedPageOffset(bubbleContainer);
    assertEquals(targetPos.y + targetSize.height + CLEARANCE, pos.y);
    assertEquals(targetPos.x, pos.x);

    // Move the target to the bottom of the viewport.
    const field = document.getElementById('field');
    const fieldPos = style.getFramedPageOffset(field, window);
    /** @suppress {visibility} suppression added to enable type checking */
    fieldPos.y += bubble.dom_.getViewportSize().height -
        (targetPos.y + targetSize.height);
    style.setStyle(field, 'position', 'absolute');
    style.setPosition(field, fieldPos);
    bubble.reposition();
    const bubbleSize = style.getSize(bubbleContainer);
    const targetPosition = style.getFramedPageOffset(link, window);
    /** @suppress {checkTypes} suppression added to enable type checking */
    pos = style.getFramedPageOffset(bubbleContainer);
    assertEquals(targetPosition.y - CLEARANCE - bubbleSize.height, pos.y);
  },

  testBubblePositionRightAligned() {
    if (product.SAFARI) {
      // TODO(user): Disabled so we can get the rest of the Closure test
      // suite running in a continuous build. Will investigate later.
      return;
    }

    prepareTargetWithGivenDirection('rtl');

    const expectedPos = getExpectedBubblePositionWithGivenAlignment(true);
    /**
     * @suppress {checkTypes,visibility} suppression added to enable type
     * checking
     */
    const pos = style.getFramedPageOffset(bubble.bubbleContainer_);
    assertRoughlyEquals(expectedPos.x, pos.x, 0.1);
    assertRoughlyEquals(expectedPos.y, pos.y, 0.1);
  },

  /**
   * Test for bug 1955511, the bubble should align to the right side
   * of the target element when the bubble is RTL, regardless of the
   * target element's directionality.
   * @suppress {visibility} suppression added to enable type checking
   */
  testBubblePositionLeftToRight() {
    if (product.SAFARI) {
      // TODO(user): Disabled so we can get the rest of the Closure test
      // suite running in a continuous build. Will investigate later.
      return;
    }

    style.setStyle(bubble.bubbleContainer_, 'direction', 'ltr');
    prepareTargetWithGivenDirection('rtl');

    const expectedPos = getExpectedBubblePositionWithGivenAlignment();
    /**
     * @suppress {checkTypes,visibility} suppression added to enable type
     * checking
     */
    const pos = style.getFramedPageOffset(bubble.bubbleContainer_);
    assertRoughlyEquals(expectedPos.x, pos.x, 0.1);
    assertRoughlyEquals(expectedPos.y, pos.y, 0.1);
  },

  /**
   * Test for bug 1955511, the bubble should align to the left side
   * of the target element when the bubble is LTR, regardless of the
   * target element's directionality.
   * @suppress {visibility} suppression added to enable type checking
   */
  testBubblePositionRightToLeft() {
    style.setStyle(bubble.bubbleContainer_, 'direction', 'rtl');
    prepareTargetWithGivenDirection('ltr');

    const expectedPos = getExpectedBubblePositionWithGivenAlignment(true);
    /**
     * @suppress {checkTypes,visibility} suppression added to enable type
     * checking
     */
    const pos = style.getFramedPageOffset(bubble.bubbleContainer_);
    assertEquals(expectedPos.x, pos.x);
    assertEquals(expectedPos.y, pos.y);
  },
});
