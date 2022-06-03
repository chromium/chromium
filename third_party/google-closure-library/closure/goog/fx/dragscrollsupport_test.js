/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.fx.DragScrollSupportTest');
goog.setTestOnly();

const Coordinate = goog.require('goog.math.Coordinate');
const DragScrollSupport = goog.require('goog.fx.DragScrollSupport');
const GoogRect = goog.require('goog.math.Rect');
const MockClock = goog.require('goog.testing.MockClock');
const events = goog.require('goog.testing.events');
const testSuite = goog.require('goog.testing.testSuite');

let vContainerDiv;
let vContentDiv;
let hContainerDiv;
let hContentDiv;
let clock;

testSuite({
  setUpPage() {
    vContainerDiv = document.getElementById('vContainerDiv');
    vContentDiv = document.getElementById('vContentDiv');
    hContainerDiv = document.getElementById('hContainerDiv');
    hContentDiv = document.getElementById('hContentDiv');
  },

  setUp() {
    clock = new MockClock(true);
  },

  tearDown() {
    clock.dispose();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testDragZeroMarginDivVContainer() {
    const dsc = new DragScrollSupport(vContainerDiv);

    // Set initial scroll state.
    let scrollTop = 50;
    vContainerDiv.scrollTop = scrollTop;

    events.fireMouseMoveEvent(vContainerDiv, new Coordinate(50, 20 + 50));
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertEquals(
        'Mousing inside the vContainer should not trigger scrolling.',
        scrollTop, vContainerDiv.scrollTop);
    assertEquals(
        'Scroll timer should not tick yet', 0, clock.getTimeoutsMade());

    scrollTop = vContainerDiv.scrollTop;
    events.fireMouseMoveEvent(vContainerDiv, new Coordinate(50, 10));
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertTrue(
        'Mousing above the vContainer should trigger scrolling up.',
        scrollTop > vContainerDiv.scrollTop);
    scrollTop = vContainerDiv.scrollTop;
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertTrue(
        'Mousing above the vContainer should trigger scrolling up.',
        scrollTop > vContainerDiv.scrollTop);

    scrollTop = vContainerDiv.scrollTop;
    events.fireMouseMoveEvent(vContainerDiv, new Coordinate(50, 20 + 110));
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertTrue(
        'Mousing below the vContainer should trigger scrolling down.',
        scrollTop < vContainerDiv.scrollTop);
    scrollTop = vContainerDiv.scrollTop;
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertTrue(
        'Mousing below the vContainer should trigger scrolling down.',
        scrollTop < vContainerDiv.scrollTop);

    scrollTop = vContainerDiv.scrollTop;
    events.fireMouseMoveEvent(vContainerDiv, new Coordinate(50, 20 + 50));
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertEquals(
        'Mousing inside the vContainer should stop scrolling.', scrollTop,
        vContainerDiv.scrollTop);

    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);

    dsc.dispose();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testDragZeroMarginDivHContainer() {
    const dsc = new DragScrollSupport(hContainerDiv);

    // Set initial scroll state.
    let scrollLeft = 50;
    hContainerDiv.scrollLeft = scrollLeft;

    events.fireMouseMoveEvent(hContainerDiv, new Coordinate(200 + 50, 20 + 50));
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertEquals(
        'Mousing inside the hContainer should not trigger scrolling.',
        scrollLeft, hContainerDiv.scrollLeft);
    assertEquals(
        'Scroll timer should not tick yet', 0, clock.getTimeoutsMade());

    scrollLeft = hContainerDiv.scrollLeft;
    events.fireMouseMoveEvent(hContainerDiv, new Coordinate(200 - 10, 20 + 50));
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertTrue(
        'Mousing left of the hContainer should trigger scrolling left.',
        scrollLeft > hContainerDiv.scrollLeft);
    scrollLeft = hContainerDiv.scrollLeft;
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertTrue(
        'Mousing left of the hContainer should trigger scrolling left.',
        scrollLeft > hContainerDiv.scrollLeft);

    scrollLeft = hContainerDiv.scrollLeft;
    events.fireMouseMoveEvent(
        hContainerDiv, new Coordinate(200 + 110, 20 + 50));
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertTrue(
        'Mousing right of the hContainer should trigger scrolling right.',
        scrollLeft < hContainerDiv.scrollLeft);
    scrollLeft = hContainerDiv.scrollLeft;
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertTrue(
        'Mousing right of the hContainer should trigger scrolling right.',
        scrollLeft < hContainerDiv.scrollLeft);

    scrollLeft = hContainerDiv.scrollLeft;
    events.fireMouseMoveEvent(hContainerDiv, new Coordinate(200 + 50, 20 + 50));
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertEquals(
        'Mousing inside the hContainer should stop scrolling.', scrollLeft,
        hContainerDiv.scrollLeft);

    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);

    dsc.dispose();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testDragMarginDivVContainer() {
    const dsc = new DragScrollSupport(vContainerDiv, 20);

    // Set initial scroll state.
    let scrollTop = 50;
    vContainerDiv.scrollTop = scrollTop;

    events.fireMouseMoveEvent(vContainerDiv, new Coordinate(50, 20 + 50));
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertEquals(
        'Mousing inside the vContainer should not trigger scrolling.',
        scrollTop, vContainerDiv.scrollTop);
    assertEquals(
        'Scroll timer should not tick yet', 0, clock.getTimeoutsMade());

    scrollTop = vContainerDiv.scrollTop;
    events.fireMouseMoveEvent(vContainerDiv, new Coordinate(50, 30));
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertTrue(
        'Mousing above the margin should trigger scrolling up.',
        scrollTop > vContainerDiv.scrollTop);
    scrollTop = vContainerDiv.scrollTop;
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertTrue(
        'Mousing above the margin should trigger scrolling up.',
        scrollTop > vContainerDiv.scrollTop);

    scrollTop = vContainerDiv.scrollTop;
    events.fireMouseMoveEvent(vContainerDiv, new Coordinate(50, 20 + 90));
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertTrue(
        'Mousing below the margin should trigger scrolling down.',
        scrollTop < vContainerDiv.scrollTop);
    scrollTop = vContainerDiv.scrollTop;
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertTrue(
        'Mousing above the margin should trigger scrolling down.',
        scrollTop < vContainerDiv.scrollTop);

    scrollTop = vContainerDiv.scrollTop;
    events.fireMouseMoveEvent(vContainerDiv, new Coordinate(50, 20 + 50));
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertEquals(
        'Mousing inside the margin should stop scrolling.', scrollTop,
        vContainerDiv.scrollTop);

    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);

    // 5 timeouts are scheduled, but the last one is cancelled.
    assertEquals(
        'Scroll timer should have ticked 4 times', 4,
        clock.getCallbacksTriggered());

    dsc.dispose();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testDragMarginScrollConstrainedDivVContainer() {
    const dsc = new DragScrollSupport(vContainerDiv, 20);
    dsc.setConstrainScroll(true);

    // Set initial scroll state.
    let scrollTop = 50;
    vContainerDiv.scrollTop = scrollTop;

    events.fireMouseMoveEvent(vContainerDiv, new Coordinate(50, 20 + 50));
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertEquals(
        'Mousing inside the vContainer should not trigger scrolling.',
        scrollTop, vContainerDiv.scrollTop);
    assertEquals(
        'Scroll timer should not tick yet', 0, clock.getTimeoutsMade());

    scrollTop = vContainerDiv.scrollTop;
    events.fireMouseMoveEvent(vContainerDiv, new Coordinate(50, 30));
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertTrue(
        'Mousing above the margin should trigger scrolling up.',
        scrollTop > vContainerDiv.scrollTop);
    scrollTop = vContainerDiv.scrollTop;
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertTrue(
        'Mousing above the margin should trigger scrolling up.',
        scrollTop > vContainerDiv.scrollTop);

    scrollTop = vContainerDiv.scrollTop;
    events.fireMouseMoveEvent(vContainerDiv, new Coordinate(50, 20 + 90));
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertTrue(
        'Mousing below the margin should trigger scrolling down.',
        scrollTop < vContainerDiv.scrollTop);
    scrollTop = vContainerDiv.scrollTop;
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertTrue(
        'Mousing above the margin should trigger scrolling down.',
        scrollTop < vContainerDiv.scrollTop);

    scrollTop = vContainerDiv.scrollTop;
    events.fireMouseMoveEvent(vContainerDiv, new Coordinate(50, 20 + 50));
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertEquals(
        'Mousing inside the margin should stop scrolling.', scrollTop,
        vContainerDiv.scrollTop);

    scrollTop = vContainerDiv.scrollTop;
    events.fireMouseMoveEvent(vContainerDiv, new Coordinate(50, 10));
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertEquals(
        'Mousing above the vContainer should not trigger scrolling up.',
        scrollTop, vContainerDiv.scrollTop);
    scrollTop = vContainerDiv.scrollTop;
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertEquals(
        'Mousing above the vContainer should not trigger scrolling up.',
        scrollTop, vContainerDiv.scrollTop);

    scrollTop = vContainerDiv.scrollTop;
    events.fireMouseMoveEvent(vContainerDiv, new Coordinate(50, 20 + 110));
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertEquals(
        'Mousing below the vContainer should not trigger scrolling down.',
        scrollTop, vContainerDiv.scrollTop);
    scrollTop = vContainerDiv.scrollTop;
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertEquals(
        'Mousing below the vContainer should not trigger scrolling down.',
        scrollTop, vContainerDiv.scrollTop);

    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);

    scrollTop = vContainerDiv.scrollTop;
    events.fireMouseMoveEvent(vContainerDiv, new Coordinate(150, 20 + 90));
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertEquals(
        'Mousing to the right of the vContainer should not trigger ' +
            'scrolling up.',
        scrollTop, vContainerDiv.scrollTop);
    scrollTop = vContainerDiv.scrollTop;
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertEquals(
        'Mousing to the right of the vContainer should not trigger ' +
            'scrolling up.',
        scrollTop, vContainerDiv.scrollTop);

    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);

    // 5 timeouts are scheduled, but the last one is cancelled.
    assertEquals(
        'Scroll timer should have ticked 4 times', 4,
        clock.getCallbacksTriggered());

    dsc.dispose();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSetHorizontalScrolling() {
    const dsc = new DragScrollSupport(hContainerDiv);
    dsc.setHorizontalScrolling(false);

    // Set initial scroll state.
    let scrollLeft = 50;
    hContainerDiv.scrollLeft = scrollLeft;

    events.fireMouseMoveEvent(hContainerDiv, new Coordinate(200 - 10, 20 + 50));
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertEquals(
        'Horizontal scrolling should be turned off', 0,
        clock.getTimeoutsMade());

    events.fireMouseMoveEvent(
        hContainerDiv, new Coordinate(200 + 110, 20 + 50));
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertEquals(
        'Horizontal scrolling should be turned off', 0,
        clock.getTimeoutsMade());

    dsc.setHorizontalScrolling(true);
    scrollLeft = hContainerDiv.scrollLeft;
    events.fireMouseMoveEvent(hContainerDiv, new Coordinate(200 - 10, 20 + 50));
    clock.tick(DragScrollSupport.TIMER_STEP_ + 1);
    assertTrue(
        'Mousing left of the hContainer should trigger scrolling left.',
        scrollLeft > hContainerDiv.scrollLeft);

    dsc.dispose();
  },

  testConstrainBoundsWithMargin() {
    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    const rect = DragScrollSupport.prototype.constrainBounds_.call(
        {margin_: 25}, new GoogRect(0, 0, 100, 100));
    assertEquals(25, rect.left);
    assertEquals(25, rect.top);
    assertEquals(25, rect.left);
    assertEquals(50, rect.width);
    assertEquals(50, rect.height);
  },
});
