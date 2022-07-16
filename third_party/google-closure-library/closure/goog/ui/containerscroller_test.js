/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.ContainerScrollerTest');
goog.setTestOnly();

const Container = goog.require('goog.ui.Container');
const ContainerScroller = goog.require('goog.ui.ContainerScroller');
const MockClock = goog.require('goog.testing.MockClock');
const dom = goog.require('goog.dom');
const events = goog.require('goog.testing.events');
const testSuite = goog.require('goog.testing.testSuite');

let sandbox;
let sandboxHtml;
let container;
let mockClock;
let scroller;

testSuite({
  setUpPage() {
    sandbox = dom.getElement('sandbox');
    sandboxHtml = sandbox.innerHTML;
  },

  setUp() {
    container = new Container();
    container.decorate(sandbox);
    /** @suppress {checkTypes} suppression added to enable type checking */
    container.getElement().scrollTop = 0;
    mockClock = new MockClock(true);
    scroller = null;
  },

  tearDown() {
    container.dispose();
    if (scroller) {
      scroller.dispose();
    }
    // Tick one second to clear all the extra registered events.
    mockClock.tick(1000);
    mockClock.uninstall();
    sandbox.innerHTML = sandboxHtml;
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testHighlightFirstStaysAtTop() {
    scroller = new ContainerScroller(container);
    container.getChildAt(0).setHighlighted(true);
    assertEquals(0, container.getElement().scrollTop);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testHighlightSecondStaysAtTop() {
    scroller = new ContainerScroller(container);
    container.getChildAt(1).setHighlighted(true);
    assertEquals(0, container.getElement().scrollTop);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testHighlightSecondLastScrollsNearTheBottom() {
    scroller = new ContainerScroller(container);
    container.getChildAt(8).setHighlighted(true);
    assertEquals(
        'Since scrolling is lazy, when highlighting the second' +
            ' last, the item should be the last visible one.',
        80, container.getElement().scrollTop);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testHighlightLastScrollsToBottom() {
    scroller = new ContainerScroller(container);
    container.getChildAt(9).setHighlighted(true);
    assertEquals(100, container.getElement().scrollTop);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testScrollRestoreIfStillVisible() {
    scroller = new ContainerScroller(container);
    container.getChildAt(9).setHighlighted(true);
    /** @suppress {checkTypes} suppression added to enable type checking */
    const scrollTop = container.getElement().scrollTop;
    container.setVisible(false);
    container.setVisible(true);
    assertEquals(
        'Scroll position should be the same after restore, if it ' +
            'still makes highlighted item visible',
        scrollTop, container.getElement().scrollTop);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testNoScrollRestoreIfNotVisible() {
    scroller = new ContainerScroller(container);
    /** @suppress {checkTypes} suppression added to enable type checking */
    container.getElement().scrollTop = 100;
    container.setVisible(false);
    container.getChildAt(0).setHighlighted(true);
    container.setVisible(true);
    assertNotEquals(
        'Scroll position should not be the same after restore, if ' +
            'the scroll position when the menu was hidden no longer ' +
            'makes the highlighted item visible when the container is ' +
            'shown again',
        100, container.getElement().scrollTop);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testCenterOnHighlightedOnFirstOpen() {
    container.setVisible(false);
    scroller = new ContainerScroller(container);
    container.getChildAt(4).setHighlighted(true);
    container.setVisible(true);
    // #2 should be at the top when 4 is centered, meaning a scroll top
    // of 40 pixels.
    assertEquals(
        'On the very first display of the scroller, the item should be ' +
            'centered, rather than just assured in view.',
        40, container.getElement().scrollTop);
  },

  testHighlightsAreIgnoredInResponseToScrolling() {
    scroller = new ContainerScroller(container);
    container.getChildAt(9).setHighlighted(true);
    events.fireMouseOverEvent(
        dom.getElement('control-5'), dom.getElement('control-9'));
    assertEquals(
        'Mouseovers due to scrolls should be ignored', 9,
        container.getHighlightedIndex());
  },

  testHighlightsAreNotIgnoredWhenNotScrolling() {
    scroller = new ContainerScroller(container);
    container.getChildAt(5).setHighlighted(true);
    mockClock.tick(1000);
    events.fireMouseOutEvent(
        dom.getElement('control-5'), dom.getElement('control-6'));
    events.fireMouseOverEvent(
        dom.getElement('control-6'), dom.getElement('control-5'));
    assertEquals(
        'Mousovers not due to scrolls should not be ignored', 6,
        container.getHighlightedIndex());
  },

  testFastSynchronousHighlightsNotIgnored() {
    scroller = new ContainerScroller(container);
    // Whereas subsequent highlights from mouseovers due to a scroll, should
    // be ignored, they should not ignored if they are made synchronusly
    // from the code and not from a mouseover.  Imagine how bad it would be
    // if you could only set the highligted index a certain number of
    // times in the same execution context.
    container.getChildAt(9).setHighlighted(true);
    container.getChildAt(1).setHighlighted(true);
    assertEquals(
        'Synchronous highlights should NOT be ignored.', 1,
        container.getHighlightedIndex());
    container.getChildAt(8).setHighlighted(true);
    assertEquals(
        'Synchronous highlights should NOT be ignored.', 8,
        container.getHighlightedIndex());
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testInitialItemIsCentered() {
    container.getChildAt(4).setHighlighted(true);
    scroller = new ContainerScroller(container);
    // #2 should be at the top when 4 is centered, meaning a scroll top
    // of 40 pixels.
    assertEquals(
        'On the very first attachment of the scroller, the item should be ' +
            'centered, rather than just assured in view.',
        40, container.getElement().scrollTop);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testInitialItemIsCenteredTopItem() {
    container.getChildAt(0).setHighlighted(true);
    scroller = new ContainerScroller(container);
    assertEquals(0, container.getElement().scrollTop);
  },

  testHidingMenuItemsDoesntAffectContainerScroller() {
    scroller = new ContainerScroller(container);
    /** @suppress {checkTypes} suppression added to enable type checking */
    container.getElement = () => {
      fail(
          'getElement() must not be called when a control in the container is ' +
          'being hidden');
    };
    container.getChildAt(0).setVisible(false);
  },
});
