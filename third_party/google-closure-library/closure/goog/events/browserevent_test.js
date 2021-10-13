/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.events.BrowserEventTest');
goog.setTestOnly();

const BrowserEvent = goog.require('goog.events.BrowserEvent');
const BrowserFeature = goog.require('goog.events.BrowserFeature');
const Coordinate = goog.require('goog.math.Coordinate');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

const stubs = new PropertyReplacer();
const Button = BrowserEvent.MouseButton;
const PointerType = BrowserEvent.PointerType;

/**
 * @param {string} type
 * @param {number} button
 * @param {boolean=} opt_ctrlKey
 * @return {!BrowserEvent}
 * @suppress {checkTypes} suppression added to enable type checking
 */
function createMouseEvent(type, button, opt_ctrlKey) {
  return new BrowserEvent({type: type, button: button, ctrlKey: !!opt_ctrlKey});
}

/**
 * @param {string} type
 * @param {!Element} target
 * @param {!Coordinate} clientCoords
 * @param {!Coordinate} screenCoords
 * @return {!BrowserEvent}
 * @suppress {checkTypes} suppression added to enable type checking
 */
function createTouchEvent(type, target, clientCoords, screenCoords) {
  return new BrowserEvent({
    type: type,
    target: target,
    changedTouches: [{
      clientX: clientCoords.x,
      clientY: clientCoords.y,
      screenX: screenCoords.x,
      screenY: screenCoords.y,
    }],
  });
}

/**
 * @param {string} type
 * @param {number} pointerId
 * @param {string} pointerType
 * @return {!BrowserEvent}
 * @suppress {checkTypes} suppression added to enable type checking
 */
function createPointerEvent(type, pointerId, pointerType) {
  return new BrowserEvent(
      {type: type, pointerId: pointerId, pointerType: pointerType});
}

/**
 * @param {!BrowserEvent} event
 * @param {BrowserEvent.MouseButton} button
 * @param {boolean} isActionButton
 * @return {!BrowserEvent}
 * @suppress {missingReturn} suppression added to enable type checking
 */
function assertIsButton(event, button, isActionButton) {
  for (let key in Button) {
    assertEquals(
        'Testing isButton(' + key + ') against ' + button + ' and type ' +
            event.type,
        Button[key] == button, event.isButton(Button[key]));
  }

  assertEquals(isActionButton, event.isMouseActionButton());
}
testSuite({
  setUp() {
    stubs.reset();
  },

  /**
   * @see https://bugzilla.mozilla.org/show_bug.cgi?id=497780
   * @suppress {visibility} suppression added to enable type checking
   */
  testInvalidNodeBug() {
    if (!userAgent.GECKO) return;

    const event = {};
    event.relatedTarget = {};
    event.relatedTarget.__defineGetter__('nodeName', () => {
      throw new Error('https://bugzilla.mozilla.org/show_bug.cgi?id=497780');
    });
    assertThrows(/**
                    @suppress {missingProperties} suppression added to enable
                    type checking
                  */
                 () => event.relatedTarget.nodeName);

    /** @suppress {checkTypes} suppression added to enable type checking */
    const bEvent = new BrowserEvent(event);
    assertEquals(event, bEvent.event_);
    assertNull(bEvent.relatedTarget);
  },

  testPreventDefault() {
    const event = {};
    event.defaultPrevented = false;
    /** @suppress {checkTypes} suppression added to enable type checking */
    const bEvent = new BrowserEvent(event);
    assertFalse(bEvent.defaultPrevented);
    bEvent.preventDefault();
    assertTrue(bEvent.defaultPrevented);
  },

  testDefaultPrevented() {
    const event = {};
    event.defaultPrevented = true;
    event.preventDefault = recordFunction();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const bEvent = new BrowserEvent(event);
    assertTrue(bEvent.defaultPrevented);
    assertEquals(0, event.preventDefault.getCallCount());
  },

  testIsButtonWebkitMac() {
    stubs.set(BrowserFeature, 'HAS_W3C_BUTTON', true);
    stubs.set(userAgent, 'WEBKIT', true);
    stubs.set(userAgent, 'MAC', true);
    assertIsButton(createMouseEvent('mousedown', 0), Button.LEFT, true);
    assertIsButton(createMouseEvent('mousedown', 0, true), Button.LEFT, false);
    assertIsButton(createMouseEvent('mousedown', 2), Button.RIGHT, false);
    assertIsButton(createMouseEvent('mousedown', 2, true), Button.RIGHT, false);
    assertIsButton(createMouseEvent('mousedown', 1), Button.MIDDLE, false);
    assertIsButton(
        createMouseEvent('mousedown', 1, true), Button.MIDDLE, false);
  },

  testIsButtonGecko() {
    stubs.set(BrowserFeature, 'HAS_W3C_BUTTON', true);
    stubs.set(userAgent, 'GECKO', true);
    stubs.set(userAgent, 'MAC', true);
    assertIsButton(createMouseEvent('mousedown', 0), Button.LEFT, true);
    assertIsButton(createMouseEvent('mousedown', 2, true), Button.RIGHT, false);
  },

  testTouchEventHandling() {
    const clientCoords = new Coordinate(5, 5);
    const screenCoords = new Coordinate(10, 10);
    const target = document.body;
    const touchStart =
        createTouchEvent('touchstart', target, clientCoords, screenCoords);
    const touchMove =
        createTouchEvent('touchmove', target, clientCoords, screenCoords);
    const touchEnd =
        createTouchEvent('touchend', target, clientCoords, screenCoords);
    const touchCancel =
        createTouchEvent('touchcancel', target, clientCoords, screenCoords);

    assertEquals(clientCoords.x, touchStart.clientX);
    assertEquals(clientCoords.y, touchStart.clientY);
    assertEquals(target, touchStart.target);

    assertEquals(screenCoords.x, touchMove.screenX);
    assertEquals(screenCoords.y, touchMove.screenY);

    assertEquals(clientCoords.x, touchEnd.clientX);
    assertEquals(clientCoords.y, touchEnd.clientY);

    assertEquals(screenCoords.x, touchCancel.screenX);
    assertEquals(screenCoords.y, touchCancel.screenY);
    assertEquals(target, touchCancel.target);
  },

  testGuardAgainstUndefinedTouchCoordinates() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const noChangedTouches = new BrowserEvent({
      type: 'touchstart',
      target: document.body,
      changedTouches: [],
    });

    /** @suppress {checkTypes} suppression added to enable type checking */
    const emptyTouchObject = new BrowserEvent({
      type: 'touchstart',
      target: document.body,
      changedTouches: [{}],
    });

    /** @suppress {checkTypes} suppression added to enable type checking */
    const onlyPageCoords = new BrowserEvent({
      type: 'touchstart',
      target: document.body,
      changedTouches: [{pageX: 6, pageY: 7}],
    });

    assertEquals(undefined, noChangedTouches.clientX);
    assertEquals(undefined, noChangedTouches.clientY);
    assertEquals(0, noChangedTouches.screenX);
    assertEquals(0, noChangedTouches.screenY);

    assertEquals(undefined, emptyTouchObject.clientX);
    assertEquals(undefined, emptyTouchObject.clientY);
    assertEquals(0, emptyTouchObject.screenX);
    assertEquals(0, emptyTouchObject.screenY);

    assertEquals(6, onlyPageCoords.clientX);
    assertEquals(7, onlyPageCoords.clientY);
    assertEquals(0, onlyPageCoords.screenX);
    assertEquals(0, onlyPageCoords.screenY);
  },

  testPointerEvent() {
    const event = createPointerEvent('pointerdown', 123, PointerType.MOUSE);
    assertEquals(123, event.pointerId);
    assertEquals(PointerType.MOUSE, event.pointerType);
  },

  testMSPointerEvent() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const event = createPointerEvent('MSPointerDown', 123, 4 /* mouse */);
    assertEquals(123, event.pointerId);
    assertEquals(PointerType.MOUSE, event.pointerType);
  },

  testUnsupportedPointerEvent() {
    const event = createMouseEvent('mousedown', 1);
    assertEquals(0, event.pointerId);
    assertEquals('', event.pointerType);
  },
});
