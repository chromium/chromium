/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.HoverCardTest');
goog.setTestOnly();

const Coordinate = goog.require('goog.math.Coordinate');
const GoogTestingEvent = goog.require('goog.testing.events.Event');
const HoverCard = goog.require('goog.ui.HoverCard');
const MockClock = goog.require('goog.testing.MockClock');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');

const timer = new MockClock();
let card;

// Variables for mocks
let triggeredElement;
let cancelledElement;
let showDelay;
let shownCard;
let hideDelay;

// spans
let john;
let jane;
let james;
let bill;
let child;

// Inactive
let elsewhere;
let offAnchor;

function initCard(
    opt_isAnchor, checkChildren = undefined, maxSearchSteps = undefined) {
  const isAnchor = opt_isAnchor || {SPAN: 'email'};
  card = new HoverCard(isAnchor, checkChildren);
  card.setText('Test hovercard');

  if (maxSearchSteps != null) {
    card.setMaxSearchSteps(maxSearchSteps);
  }

  events.listen(card, HoverCard.EventType.TRIGGER, onTrigger);
  events.listen(card, HoverCard.EventType.CANCEL_TRIGGER, onCancel);
  events.listen(card, HoverCard.EventType.BEFORE_SHOW, onBeforeShow);

  // This gets around the problem where AdvancedToolTip thinks it's
  // receiving a ghost event because cursor position hasn't moved off of
  // (0, 0).
  /** @suppress {visibility} suppression added to enable type checking */
  card.cursorPosition = new Coordinate(1, 1);
}

// Event handlers
function onTrigger(event) {
  triggeredElement = event.anchor;
  if (showDelay) {
    card.setShowDelayMs(showDelay);
  }
  return true;
}

function onCancel(event) {
  cancelledElement = event.anchor;
}

function onBeforeShow() {
  shownCard = card.getAnchorElement();
  if (hideDelay) {
    card.setHideDelayMs(hideDelay);
  }
  return true;
}

testSuite({
  setUpPage() {
    john = dom.getElement('john');
    jane = dom.getElement('jane');
    james = dom.getElement('james');
    bill = dom.getElement('bill');
    child = dom.getElement('child');
  },

  setUp() {
    timer.install();
    triggeredElement = null;
    cancelledElement = null;
    showDelay = null;
    shownCard = null;
    hideDelay = null;
    elsewhere = dom.getElement('notpopup');
    offAnchor = new Coordinate(1, 1);
  },

  tearDown() {
    card.dispose();
    timer.uninstall();
  },

  /**
     Verify that hovercard displays and goes away under normal circumstances.
   */
  testTrigger() {
    initCard();

    // Mouse over correct element fires trigger
    showDelay = 500;
    testingEvents.fireMouseOverEvent(john, elsewhere);
    assertEquals('Hovercard should have triggered', john, triggeredElement);

    // Show card after delay
    timer.tick(showDelay - 1);
    assertNull('Card should not have shown', shownCard);
    assertFalse(card.isVisible());
    hideDelay = 5000;
    timer.tick(1);
    assertEquals('Card should have shown', john, shownCard);
    assertTrue(card.isVisible());

    // Mouse out leads to hide delay
    testingEvents.fireMouseOutEvent(john, elsewhere);
    testingEvents.fireMouseMoveEvent(document, offAnchor);
    timer.tick(hideDelay - 1);
    assertTrue('Card should still be visible', card.isVisible());
    timer.tick(10);
    assertFalse('Card should be hidden', card.isVisible());
  },

  /**
   * Verify that CANCEL_TRIGGER event occurs when mouse goes out of
   * triggering element before hovercard is shown.
   */
  testOnCancel() {
    initCard();

    showDelay = 500;
    testingEvents.fireMouseOverEvent(john, elsewhere);
    timer.tick(showDelay - 1);
    testingEvents.fireMouseOutEvent(john, elsewhere);
    testingEvents.fireMouseMoveEvent(document, offAnchor);
    timer.tick(10);
    assertFalse('Card should be hidden', card.isVisible());
    assertEquals('Should have cancelled trigger', john, cancelledElement);
  },

  /** Verify that mousing over non-triggering elements don't interfere. */
  testMouseOverNonTrigger() {
    initCard();

    // Mouse over correct element fires trigger
    showDelay = 500;
    testingEvents.fireMouseOverEvent(john, elsewhere);
    timer.tick(showDelay);

    // Mouse over and out other element does nothing
    triggeredElement = null;
    testingEvents.fireMouseOverEvent(jane, elsewhere);
    timer.tick(showDelay + 1);
    assertNull(triggeredElement);
  },

  /**
   * Verify that a mouse over event with no target will not break
   * hover card.
   * @suppress {visibility,checkTypes} suppression added to enable type checking
   */
  testMouseOverNoTarget() {
    initCard();
    card.handleTriggerMouseOver_(new GoogTestingEvent());
  },

  /**
   * Verify that mousing over a second trigger before the first one shows
   * will correctly cancel the first and show the second.
   */
  testMultipleTriggers() {
    initCard();

    // Test second trigger when first one still pending
    showDelay = 500;
    hideDelay = 1000;
    testingEvents.fireMouseOverEvent(john, elsewhere);
    timer.tick(250);
    testingEvents.fireMouseOutEvent(john, james);
    testingEvents.fireMouseOverEvent(james, john);
    // First trigger should cancel because it isn't showing yet
    assertEquals('Should cancel first trigger', john, cancelledElement);
    timer.tick(300);
    assertFalse(card.isVisible());
    timer.tick(250);
    assertEquals('Should show second card', james, shownCard);
    assertTrue(card.isVisible());

    testingEvents.fireMouseOutEvent(james, john);
    testingEvents.fireMouseOverEvent(john, james);
    assertEquals(
        'Should still show second card', james, card.getAnchorElement());
    assertTrue(card.isVisible());

    shownCard = null;
    timer.tick(501);
    assertEquals('Should show first card again', john, shownCard);
    assertTrue(card.isVisible());

    // Test that cancelling while another is showing gives correct cancel
    // information
    cancelledElement = null;
    testingEvents.fireMouseOutEvent(john, james);
    testingEvents.fireMouseOverEvent(james, john);
    testingEvents.fireMouseOutEvent(james, elsewhere);
    assertEquals('Should cancel second card', james, cancelledElement);
  },

  /** Verify manual triggering. */
  testManualTrigger() {
    initCard();

    // Doesn't normally trigger for div tag
    showDelay = 500;
    testingEvents.fireMouseOverEvent(bill, elsewhere);
    timer.tick(showDelay);
    assertFalse(card.isVisible());

    // Manually trigger element
    card.triggerForElement(bill);
    hideDelay = 600;
    timer.tick(showDelay);
    assertTrue(card.isVisible());
    testingEvents.fireMouseOutEvent(bill, elsewhere);
    testingEvents.fireMouseMoveEvent(document, offAnchor);
    timer.tick(hideDelay);
    assertFalse(card.isVisible());
  },

  /** Verify creating with isAnchor function. */
  testIsAnchor() {
    // Initialize card so only bill triggers it.
    initCard((element) => element == bill);

    showDelay = 500;
    testingEvents.fireMouseOverEvent(bill, elsewhere);
    timer.tick(showDelay);
    assertTrue('Should trigger card', card.isVisible());

    hideDelay = 300;
    testingEvents.fireMouseOutEvent(bill, elsewhere);
    testingEvents.fireMouseMoveEvent(document, offAnchor);
    timer.tick(hideDelay);
    assertFalse(card.isVisible());

    testingEvents.fireMouseOverEvent(john, elsewhere);
    timer.tick(showDelay);
    assertFalse('Should not trigger card', card.isVisible());
  },

  /** Verify mouse over child of anchor triggers hovercard. */
  testAnchorWithChildren() {
    initCard();

    showDelay = 500;
    testingEvents.fireMouseOverEvent(james, elsewhere);
    timer.tick(250);

    // Moving from an anchor to a child of that anchor shouldn't cancel
    // or retrigger.
    const childBounds = style.getBounds(child);
    const inChild = new Coordinate(childBounds.left + 1, childBounds.top + 1);
    testingEvents.fireMouseOutEvent(james, child);
    testingEvents.fireMouseMoveEvent(child, inChild);
    assertNull('Shouldn\'t cancel trigger', cancelledElement);
    triggeredElement = null;
    testingEvents.fireMouseOverEvent(child, james);
    assertNull('Shouldn\'t retrigger card', triggeredElement);
    timer.tick(250);
    assertTrue('Card should show with original delay', card.isVisible());

    hideDelay = 300;
    testingEvents.fireMouseOutEvent(child, elsewhere);
    testingEvents.fireMouseMoveEvent(child, offAnchor);
    timer.tick(hideDelay);
    assertFalse(card.isVisible());

    testingEvents.fireMouseOverEvent(child, elsewhere);
    timer.tick(showDelay);
    assertTrue('Mouse over child should trigger card', card.isVisible());
  },

  testNoTriggerWithMaxSearchSteps() {
    initCard(undefined, true, 0);

    showDelay = 500;
    testingEvents.fireMouseOverEvent(child, elsewhere);
    timer.tick(showDelay);
    assertFalse('Should not trigger card', card.isVisible());
  },

  testTriggerWithMaxSearchSteps() {
    initCard(undefined, true, 2);

    showDelay = 500;
    testingEvents.fireMouseOverEvent(child, elsewhere);
    timer.tick(showDelay);
    assertTrue('Should trigger card', card.isVisible());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testPositionAfterSecondTriggerWithMaxSearchSteps() {
    initCard(undefined, true, 2);

    showDelay = 500;
    testingEvents.fireMouseOverEvent(john, elsewhere);
    timer.tick(showDelay);
    assertTrue('Should trigger card', card.isVisible());
    assertEquals(
        'Card cursor x coordinate should be 1', card.position_.coordinate.x, 1);
    /** @suppress {visibility} suppression added to enable type checking */
    card.cursorPosition = new Coordinate(2, 2);
    testingEvents.fireMouseOverEvent(child, elsewhere);
    timer.tick(showDelay);
    assertTrue('Should trigger card', card.isVisible());
    assertEquals(
        'Card cursor x coordinate should be 2', card.position_.coordinate.x, 2);
  },
});
