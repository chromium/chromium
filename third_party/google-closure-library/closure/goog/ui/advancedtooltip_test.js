/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.AdvancedTooltipTest');
goog.setTestOnly();

const AdvancedTooltip = goog.require('goog.ui.AdvancedTooltip');
const Box = goog.require('goog.math.Box');
const Coordinate = goog.require('goog.math.Coordinate');
const EventType = goog.require('goog.events.EventType');
const GoogEvent = goog.require('goog.events.Event');
const MockClock = goog.require('goog.testing.MockClock');
const TagName = goog.require('goog.dom.TagName');
const Tooltip = goog.require('goog.ui.Tooltip');
const dom = goog.require('goog.dom');
const events = goog.require('goog.testing.events');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

let att;
let clock;
let anchor;
let elsewhere;
let popup;

const SHOWDELAY = 50;
const HIDEDELAY = 250;
const TRACKINGDELAY = 100;

function isWindowTooSmall() {
  // Firefox 3 fails if the window is too small.
  return userAgent.GECKO &&
      (window.innerWidth < 350 || window.innerHeight < 100);
}

function assertVisible(msg, element) {
  if (element) {
    assertEquals(msg, 'visible', element.style.visibility);
  } else {
    assertEquals('visible', msg.style.visibility);
  }
}

function assertHidden(msg, element) {
  if (element) {
    assertEquals(msg, 'hidden', element.style.visibility);
  } else {
    assertEquals('hidden', msg.style.visibility);
  }
}

/**
 * Helper function to fire events related to moving a mouse from one element
 * to another. Fires mouseout, mouseover, and mousemove event.
 * @param {Element} from Element the mouse is moving from.
 * @param {Element} to Element the mouse is moving to.
 */
function fireMouseEvents(from, to) {
  events.fireMouseOutEvent(from, to);
  events.fireMouseOverEvent(to, from);
  const bounds = style.getBounds(to);
  events.fireMouseMoveEvent(
      document, new Coordinate(bounds.left + 1, bounds.top + 1));
}

/** @suppress {checkTypes} suppression added to enable type checking */
function checkNestedTooltips(useAdvancedTooltip) {
  popup.appendChild(
      dom.createDom(TagName.SPAN, {id: 'nestedAnchor'}, 'Nested Anchor'));
  const nestedAnchor = dom.getElement('nestedAnchor');
  let nestedTooltip;
  if (useAdvancedTooltip) {
    nestedTooltip = new AdvancedTooltip(nestedAnchor, 'popup');
  } else {
    nestedTooltip = new Tooltip(nestedAnchor, 'popup');
  }
  const nestedPopup = nestedTooltip.getElement();
  nestedTooltip.setShowDelayMs(SHOWDELAY);
  nestedTooltip.setHideDelayMs(HIDEDELAY);

  fireMouseEvents(elsewhere, anchor);
  clock.tick(SHOWDELAY);
  fireMouseEvents(anchor, popup);
  fireMouseEvents(popup, nestedAnchor);
  clock.tick(SHOWDELAY + HIDEDELAY);
  assertVisible('Mouse into nested anchor should show popup', nestedPopup);
  assertVisible('Mouse into nested anchor should not hide parent', popup);
  fireMouseEvents(nestedAnchor, elsewhere);
  clock.tick(HIDEDELAY);
  assertHidden('Mouse out of nested popup should hide it', nestedPopup);
  clock.tick(HIDEDELAY);
  assertHidden(
      'Mouse out of nested popup should eventually hide parent', popup);

  events.fireBrowserEvent(new GoogEvent(EventType.FOCUS, anchor));
  clock.tick(SHOWDELAY);
  events.fireBrowserEvent(new GoogEvent(EventType.BLUR, anchor));
  events.fireBrowserEvent(new GoogEvent(EventType.FOCUS, nestedAnchor));
  clock.tick(SHOWDELAY + HIDEDELAY);
  assertVisible("Moving focus to child anchor doesn't hide parent", popup);
  assertVisible('Set focus shows nested popup', nestedPopup);

  events.fireBrowserEvent(new GoogEvent(EventType.BLUR, nestedAnchor));
  events.fireBrowserEvent(new GoogEvent(EventType.FOCUS, anchor));
  clock.tick(HIDEDELAY + HIDEDELAY);
  assertHidden('Lose focus hides nested popup', nestedPopup);
  assertVisible(
      "Moving focus from nested anchor to parent doesn't hide parent", popup);

  events.fireBrowserEvent(new GoogEvent(EventType.BLUR, anchor));
  events.fireBrowserEvent(new GoogEvent(EventType.FOCUS, nestedAnchor));
  clock.tick(SHOWDELAY);
  events.fireBrowserEvent(new GoogEvent(EventType.BLUR, nestedAnchor));
  clock.tick(HIDEDELAY);
  assertHidden('Lose focus hides nested popup', nestedPopup);
  clock.tick(HIDEDELAY);
  assertHidden('Nested anchor losing focus hides parent', popup);

  events.fireBrowserEvent(new GoogEvent(EventType.FOCUS, anchor));
  clock.tick(SHOWDELAY);
  events.fireBrowserEvent(new GoogEvent(EventType.BLUR, anchor));
  events.fireBrowserEvent(new GoogEvent(EventType.FOCUS, nestedAnchor));
  clock.tick(SHOWDELAY);
  const coordElsewhere = new Coordinate(1, 1);
  events.fireMouseMoveEvent(document, coordElsewhere);
  clock.tick(HIDEDELAY);
  assertHidden('Mouse move should hide parent with active child', popup);
  assertHidden('Mouse move should hide nested popup', nestedPopup);
}
testSuite({
  setUp() {
    popup = dom.createDom(
        TagName.SPAN,
        {id: 'popup', style: 'position:absolute;top:300;left:300'}, 'Hello');
    att = new AdvancedTooltip('hovertarget');
    att.setElement(popup);
    att.setCursorTracking(true);
    att.setHotSpotPadding(new Box(10, 10, 10, 10));
    att.setShowDelayMs(SHOWDELAY);
    att.setHideDelayMs(HIDEDELAY);
    att.setCursorTrackingHideDelayMs(TRACKINGDELAY);
    att.setMargin(new Box(300, 0, 0, 300));

    clock = new MockClock(true);

    anchor = dom.getElement('hovertarget');
    elsewhere = dom.getElement('notpopup');
  },

  /** @suppress {visibility} suppression added to enable type checking */
  tearDown() {
    // tooltip needs to be hidden as well as disposed of so that it doesn't
    // leave global state hanging around to trip up other tests.
    if (att.isVisible()) {
      att.onHide();
    }
    att.dispose();
    clock.uninstall();
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testCursorTracking() {
    if (isWindowTooSmall()) {
      return;
    }

    let oneThirdOfTheWay;
    let twoThirdsOfTheWay;

    oneThirdOfTheWay = new Coordinate(100, 100);
    twoThirdsOfTheWay = new Coordinate(200, 200);

    events.fireMouseOverEvent(anchor, elsewhere);
    clock.tick(SHOWDELAY);
    assertVisible('Mouse over anchor should show popup', popup);

    events.fireMouseOutEvent(anchor, elsewhere);
    events.fireMouseMoveEvent(document, oneThirdOfTheWay);
    clock.tick(HIDEDELAY);
    assertVisible('Moving mouse towards popup shouldn\'t hide it', popup);

    events.fireMouseMoveEvent(document, twoThirdsOfTheWay);
    events.fireMouseMoveEvent(document, oneThirdOfTheWay);
    clock.tick(TRACKINGDELAY);
    assertHidden('Moving mouse away from popup should hide it', popup);

    events.fireMouseMoveEvent(document, twoThirdsOfTheWay);
    events.fireBrowserEvent(new GoogEvent(EventType.FOCUS, anchor));
    clock.tick(SHOWDELAY);
    assertVisible('Set focus shows popup', popup);
    events.fireMouseMoveEvent(document, oneThirdOfTheWay);
    clock.tick(TRACKINGDELAY);
    assertHidden('Mouse move after focus should hide popup', popup);
  },

  testPadding() {
    if (isWindowTooSmall()) {
      return;
    }

    events.fireMouseOverEvent(anchor, elsewhere);
    clock.tick(SHOWDELAY);

    const attBounds = style.getBounds(popup);
    const inPadding = new Coordinate(attBounds.left - 5, attBounds.top - 5);
    const outOfPadding =
        new Coordinate(attBounds.left - 15, attBounds.top - 15);

    fireMouseEvents(anchor, popup);
    events.fireMouseOutEvent(popup, elsewhere);
    events.fireMouseMoveEvent(document, inPadding);
    clock.tick(HIDEDELAY);
    assertVisible(
        'Mouse out of popup but within padding shouldn\'t hide it', popup);

    events.fireMouseMoveEvent(document, outOfPadding);
    clock.tick(HIDEDELAY);
    assertHidden('Mouse move beyond popup padding should hide it', popup);
  },

  testAnchorWithChild() {
    const child = dom.getElement('childtarget');

    fireMouseEvents(elsewhere, anchor);
    fireMouseEvents(anchor, child);
    clock.tick(SHOWDELAY);
    assertVisible('Mouse into child of anchor should still show popup', popup);

    fireMouseEvents(child, anchor);
    clock.tick(HIDEDELAY);
    assertVisible('Mouse from child to anchor should still show popup', popup);
  },

  testNestedTooltip() {
    if (!isWindowTooSmall()) {
      checkNestedTooltips(false);
    }
  },

  testNestedAdvancedTooltip() {
    if (!isWindowTooSmall()) {
      checkNestedTooltips(true);
    }
  },

  testResizingTooltipWhileShown() {
    fireMouseEvents(elsewhere, anchor);
    clock.tick(SHOWDELAY);
    popup.style.height = '100px';
    const attBounds = style.getBounds(popup);
    const inPadding = new Coordinate(
        attBounds.left + 5, attBounds.top + attBounds.height + 5);
    const outOfPadding = new Coordinate(
        attBounds.left + 5, attBounds.top + attBounds.height + 15);

    fireMouseEvents(anchor, popup);
    events.fireMouseOutEvent(popup, elsewhere);
    events.fireMouseMoveEvent(document, inPadding);
    clock.tick(HIDEDELAY);
    assertVisible(
        'Mouse out of popup but within padding shouldn\'t hide it', popup);

    events.fireMouseMoveEvent(document, outOfPadding);
    clock.tick(HIDEDELAY);
    assertHidden('Mouse move beyond popup padding should hide it', popup);
  },
});
