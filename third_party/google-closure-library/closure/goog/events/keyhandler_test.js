/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.events.KeyEventTest');
goog.setTestOnly();

const BrowserEvent = goog.require('goog.events.BrowserEvent');
const EventType = goog.require('goog.events.EventType');
const KeyCodes = goog.require('goog.events.KeyCodes');
const KeyHandler = goog.require('goog.events.KeyHandler');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');
const userAgent = goog.require('goog.userAgent');

function assertIe8StyleKeyHandling() {
  let keyEvent;
  const keyHandler = new KeyHandler();

  events.listen(keyHandler, KeyHandler.EventType.KEY, (e) => {
    keyEvent = e;
  });

  fireKeyDown(keyHandler, KeyCodes.ENTER);
  fireKeyPress(keyHandler, KeyCodes.ENTER);
  assertEquals(
      'Enter should fire a key event with the keycode 13', KeyCodes.ENTER,
      keyEvent.keyCode);
  assertEquals(
      'Enter should fire a key event with the charcode 0', 0,
      keyEvent.charCode);

  fireKeyDown(keyHandler, KeyCodes.ESC);
  fireKeyPress(keyHandler, KeyCodes.ESC);
  assertEquals(
      'Esc should fire a key event with the keycode 27', KeyCodes.ESC,
      keyEvent.keyCode);
  assertEquals(
      'Esc should fire a key event with the charcode 0', 0, keyEvent.charCode);

  fireKeyDown(keyHandler, KeyCodes.UP);
  assertEquals(
      'Up should fire a key event with the keycode 38', KeyCodes.UP,
      keyEvent.keyCode);
  assertEquals(
      'Up should fire a key event with the charcode 0', 0, keyEvent.charCode);

  fireKeyDown(
      keyHandler, KeyCodes.SEVEN, undefined, undefined, undefined, undefined,
      true);
  fireKeyPress(
      keyHandler, 38, undefined, undefined, undefined, undefined, true);
  assertEquals(
      'Shift+7 should fire a key event with the keycode 55', KeyCodes.SEVEN,
      keyEvent.keyCode);
  assertEquals(
      'Shift+7 should fire a key event with the charcode 38', 38,
      keyEvent.charCode);

  fireKeyDown(keyHandler, KeyCodes.A);
  fireKeyPress(keyHandler, 97);
  assertEquals(
      'Lower case a should fire a key event with the keycode 65', KeyCodes.A,
      keyEvent.keyCode);
  assertEquals(
      'Lower case a should fire a key event with the charcode 97', 97,
      keyEvent.charCode);

  fireKeyDown(keyHandler, KeyCodes.A);
  fireKeyPress(keyHandler, 65);
  assertEquals(
      'Upper case A should fire a key event with the keycode 65', KeyCodes.A,
      keyEvent.keyCode);
  assertEquals(
      'Upper case A should fire a key event with the charcode 65', 65,
      keyEvent.charCode);

  fireKeyDown(keyHandler, KeyCodes.DELETE);
  assertEquals(
      'Delete should fire a key event with the keycode 46', KeyCodes.DELETE,
      keyEvent.keyCode);
  assertEquals(
      'Delete should fire a key event with the charcode 0', 0,
      keyEvent.charCode);

  fireKeyDown(keyHandler, KeyCodes.PERIOD);
  fireKeyPress(keyHandler, 46);
  assertEquals(
      'Period should fire a key event with the keycode 190', KeyCodes.PERIOD,
      keyEvent.keyCode);
  assertEquals(
      'Period should fire a key event with the charcode 46', 46,
      keyEvent.charCode);

  fireKeyDown(keyHandler, KeyCodes.CTRL);
  fireKeyDown(keyHandler, KeyCodes.A);
  assertEquals(
      'A with control down should fire a key event', KeyCodes.A,
      keyEvent.keyCode);

  // On IE, when Ctrl+<key> is held down, there is a KEYDOWN, a KEYPRESS, and
  // then a series of KEYDOWN events for each repeat.
  fireKeyDown(keyHandler, KeyCodes.B, undefined, undefined, true);
  fireKeyPress(keyHandler, KeyCodes.B, undefined, undefined, true);
  assertEquals(
      'B with control down should fire a key event', KeyCodes.B,
      keyEvent.keyCode);
  assertTrue('Ctrl should be down.', keyEvent.ctrlKey);
  assertFalse(
      'Should not have repeat=true on the first key press.', keyEvent.repeat);
  // Fire one repeated keydown event.
  fireKeyDown(keyHandler, KeyCodes.B, undefined, undefined, true);
  assertEquals(
      'A with control down should fire a key event', KeyCodes.B,
      keyEvent.keyCode);
  assertTrue('Should have repeat=true on key repeat.', keyEvent.repeat);
  assertTrue('Ctrl should be down.', keyEvent.ctrlKey);
}

function fireKeyDown(
    keyHandler, keyCode, charCode = undefined, keyIdentifier = undefined,
    ctrlKey = undefined, altKey = undefined, shiftKey = undefined) {
  const fakeEvent = createFakeKeyEvent(
      EventType.KEYDOWN, keyCode, charCode, keyIdentifier, ctrlKey, altKey,
      shiftKey);
  keyHandler.handleKeyDown_(fakeEvent);
  return fakeEvent.returnValue_;
}

function fireKeyPress(
    keyHandler, keyCode, charCode = undefined, keyIdentifier = undefined,
    ctrlKey = undefined, altKey = undefined, shiftKey = undefined) {
  const fakeEvent = createFakeKeyEvent(
      EventType.KEYPRESS, keyCode, charCode, keyIdentifier, ctrlKey, altKey,
      shiftKey);
  keyHandler.handleEvent(fakeEvent);
  return fakeEvent.returnValue_;
}

function fireKeyUp(
    keyHandler, keyCode, charCode = undefined, keyIdentifier = undefined,
    ctrlKey = undefined, altKey = undefined, shiftKey = undefined) {
  const fakeEvent = createFakeKeyEvent(
      EventType.KEYUP, keyCode, charCode, keyIdentifier, ctrlKey, altKey,
      shiftKey);
  keyHandler.handleKeyup_(fakeEvent);
  return fakeEvent.returnValue_;
}

/** @suppress {checkTypes} suppression added to enable type checking */
function createFakeKeyEvent(
    type, keyCode, opt_charCode, opt_keyIdentifier, opt_ctrlKey, opt_altKey,
    opt_shiftKey) {
  const event = {
    type: type,
    keyCode: keyCode,
    charCode: opt_charCode || undefined,
    keyIdentifier: opt_keyIdentifier || undefined,
    ctrlKey: opt_ctrlKey || false,
    altKey: opt_altKey || false,
    shiftKey: opt_shiftKey || false,
    timeStamp: Date.now(),
  };
  return new BrowserEvent(event);
}
testSuite({
  setUp() {
    // Have this based on a fictitious DOCUMENT_MODE constant.
    /**
     * @suppress {strictPrimitiveOperators} suppression added to enable type
     * checking
     */
    userAgent.isDocumentMode = (mode) => mode <= userAgent.DOCUMENT_MODE;
  },

  /**
   * Tests the key handler for the IE 8 and lower behavior.
   * @suppress {const}
   */
  testIe8StyleKeyHandling() {
    userAgent.IE = true;
    userAgent.GECKO = false;
    userAgent.WEBKIT = false;
    userAgent.MAC = false;
    userAgent.WINDOWS = true;
    userAgent.LINUX = false;
    /** @suppress {checkTypes} suppression added to enable type checking */
    userAgent.VERSION = 8;
    userAgent.DOCUMENT_MODE = 8;

    assertIe8StyleKeyHandling();
  },

  /** Tests the key handler for the IE 8 and lower behavior. */
  testIe8StyleKeyHandlingInIe9DocumentMode() {
    userAgent.IE = true;
    userAgent.GECKO = false;
    userAgent.WEBKIT = false;
    userAgent.MAC = false;
    userAgent.WINDOWS = true;
    userAgent.LINUX = false;
    /** @suppress {checkTypes} suppression added to enable type checking */
    userAgent.VERSION = 9;  // Try IE9 in IE8 document mode.
    /**
     * @suppress {constantProperty} suppression added to enable type checking
     */
    userAgent.DOCUMENT_MODE = 8;

    assertIe8StyleKeyHandling();
  },

  /** Tests special cases for IE9. */
  testIe9StyleKeyHandling() {
    userAgent.IE = true;
    userAgent.GECKO = false;
    userAgent.WEBKIT = false;
    userAgent.MAC = false;
    userAgent.WINDOWS = true;
    userAgent.LINUX = false;
    /** @suppress {checkTypes} suppression added to enable type checking */
    userAgent.VERSION = 9;
    /**
     * @suppress {constantProperty} suppression added to enable type checking
     */
    userAgent.DOCUMENT_MODE = 9;

    let keyEvent;
    const keyHandler = new KeyHandler();

    events.listen(keyHandler, KeyHandler.EventType.KEY, (e) => {
      keyEvent = e;
    });

    fireKeyDown(keyHandler, KeyCodes.ENTER);
    fireKeyPress(keyHandler, KeyCodes.ENTER);
    assertEquals(
        'Enter should fire a key event with the keycode 13', KeyCodes.ENTER,
        keyEvent.keyCode);
    assertEquals(
        'Enter should fire a key event with the charcode 0', 0,
        keyEvent.charCode);
  },

  /**
   * Tests the key handler for the Gecko legacy behavior. This means the
   * following:
   * - `keypress` events are dispatched on non-printable character events.
   *    See: https://github.com/google/closure-library/issues/883
   * - `keyCode` is set to 0 on keypress events for non-function keys.
   *    See: https://github.com/google/closure-library/issues/932
   */
  testGeckoStyleKeyHandling_legacyBehavior() {
    userAgent.IE = false;
    userAgent.GECKO = true;
    userAgent.WEBKIT = false;
    userAgent.MAC = false;
    userAgent.WINDOWS = true;
    userAgent.LINUX = false;

    let eventsFired = 0;
    let keyEvent;
    const keyHandler = new KeyHandler();

    events.listen(keyHandler, KeyHandler.EventType.KEY, (e) => {
      keyEvent = e;
      eventsFired++;
    });

    fireKeyDown(keyHandler, KeyCodes.ENTER);
    fireKeyPress(keyHandler, KeyCodes.ENTER);
    assertEquals(
        'Enter should fire a key event with the keycode 13', KeyCodes.ENTER,
        keyEvent.keyCode);
    assertEquals(
        'Enter should fire a key event with the charcode 0', 0,
        keyEvent.charCode);
    assertEquals('Only one key event should have been fired.', 1, eventsFired);

    eventsFired = 0;
    fireKeyDown(keyHandler, KeyCodes.ESC);
    fireKeyPress(keyHandler, KeyCodes.ESC);
    assertEquals(
        'Esc should fire a key event with the keycode 27', KeyCodes.ESC,
        keyEvent.keyCode);
    assertEquals(
        'Esc should fire a key event with the charcode 0', 0,
        keyEvent.charCode);
    assertEquals('Only one key event should have been fired.', 1, eventsFired);

    eventsFired = 0;
    fireKeyDown(keyHandler, KeyCodes.UP);
    fireKeyPress(keyHandler, KeyCodes.UP);
    assertEquals(
        'Up should fire a key event with the keycode 38', KeyCodes.UP,
        keyEvent.keyCode);
    assertEquals(
        'Up should fire a key event with the charcode 0', 0, keyEvent.charCode);
    assertEquals('Only one key event should have been fired.', 1, eventsFired);

    eventsFired = 0;
    fireKeyDown(
        keyHandler, KeyCodes.SEVEN, undefined, undefined, undefined, undefined,
        true);
    fireKeyPress(
        keyHandler, undefined, 38, undefined, undefined, undefined, true);
    assertEquals(
        'Shift+7 should fire a key event with the keycode 55', KeyCodes.SEVEN,
        keyEvent.keyCode);
    assertEquals(
        'Shift+7 should fire a key event with the charcode 38', 38,
        keyEvent.charCode);
    assertEquals('Only one key event should have been fired.', 1, eventsFired);

    eventsFired = 0;
    fireKeyDown(keyHandler, KeyCodes.A);
    fireKeyPress(keyHandler, undefined, 97);
    assertEquals(
        'Lower case a should fire a key event with the keycode 65', KeyCodes.A,
        keyEvent.keyCode);
    assertEquals(
        'Lower case a should fire a key event with the charcode 97', 97,
        keyEvent.charCode);
    assertEquals('Only one key event should have been fired.', 1, eventsFired);

    eventsFired = 0;
    fireKeyDown(keyHandler, KeyCodes.A);
    fireKeyPress(keyHandler, undefined, 65);
    assertEquals(
        'Upper case A should fire a key event with the keycode 65', KeyCodes.A,
        keyEvent.keyCode);
    assertEquals(
        'Upper case A should fire a key event with the charcode 65', 65,
        keyEvent.charCode);
    assertEquals('Only one key event should have been fired.', 1, eventsFired);

    eventsFired = 0;
    fireKeyDown(keyHandler, KeyCodes.DELETE);
    fireKeyPress(keyHandler, KeyCodes.DELETE);
    assertEquals(
        'Delete should fire a key event with the keycode 46', KeyCodes.DELETE,
        keyEvent.keyCode);
    assertEquals(
        'Delete should fire a key event with the charcode 0', 0,
        keyEvent.charCode);
    assertEquals('Only one key event should have been fired.', 1, eventsFired);

    eventsFired = 0;
    fireKeyDown(keyHandler, KeyCodes.PERIOD);
    fireKeyPress(keyHandler, undefined, 46);
    assertEquals(
        'Period should fire a key event with the keycode 190', KeyCodes.PERIOD,
        keyEvent.keyCode);
    assertEquals(
        'Period should fire a key event with the charcode 46', 46,
        keyEvent.charCode);
    assertEquals('Only one key event should have been fired.', 1, eventsFired);
  },

  /**
   * Tests the key handler for the Gecko behavior with the following experiment
   * rolled out on Gecko:
   * - `keypress` events are not dispatched on non-printable character events.
   *    See: https://github.com/google/closure-library/issues/883
   */
  testGeckoStyleKeyHandling_noKeyPressEventsOnNonPrintable() {
    userAgent.IE = false;
    userAgent.GECKO = true;
    userAgent.WEBKIT = false;
    userAgent.MAC = false;
    userAgent.WINDOWS = true;
    userAgent.LINUX = false;

    let eventsFired = 0;
    let keyEvent;
    const keyHandler = new KeyHandler();

    events.listen(keyHandler, KeyHandler.EventType.KEY, (e) => {
      keyEvent = e;
      eventsFired++;
    });

    fireKeyDown(keyHandler, KeyCodes.ENTER);
    fireKeyPress(keyHandler, KeyCodes.ENTER);
    assertEquals(
        'Enter should fire a key event with the keycode 13', KeyCodes.ENTER,
        keyEvent.keyCode);
    assertEquals(
        'Enter should fire a key event with the charcode 0', 0,
        keyEvent.charCode);
    assertEquals('Only one key event should have been fired.', 1, eventsFired);

    eventsFired = 0;
    fireKeyDown(keyHandler, KeyCodes.ESC);
    assertEquals(
        'Esc should fire a key event with the keycode 27', KeyCodes.ESC,
        keyEvent.keyCode);
    assertEquals(
        'Esc should fire a key event with the charcode 0', 0,
        keyEvent.charCode);
    assertEquals('Only one key event should have been fired.', 1, eventsFired);

    eventsFired = 0;
    fireKeyDown(keyHandler, KeyCodes.UP);
    assertEquals(
        'Up should fire a key event with the keycode 38', KeyCodes.UP,
        keyEvent.keyCode);
    assertEquals(
        'Up should fire a key event with the charcode 0', 0, keyEvent.charCode);
    assertEquals('Only one key event should have been fired.', 1, eventsFired);

    eventsFired = 0;
    fireKeyDown(
        keyHandler, KeyCodes.SEVEN, undefined, undefined, undefined, undefined,
        true);
    fireKeyPress(
        keyHandler, undefined, 38, undefined, undefined, undefined, true);
    assertEquals(
        'Shift+7 should fire a key event with the keycode 55', KeyCodes.SEVEN,
        keyEvent.keyCode);
    assertEquals(
        'Shift+7 should fire a key event with the charcode 38', 38,
        keyEvent.charCode);
    assertEquals('Only one key event should have been fired.', 1, eventsFired);

    eventsFired = 0;
    fireKeyDown(keyHandler, KeyCodes.A);
    fireKeyPress(keyHandler, undefined, 97);
    assertEquals(
        'Lower case a should fire a key event with the keycode 65', KeyCodes.A,
        keyEvent.keyCode);
    assertEquals(
        'Lower case a should fire a key event with the charcode 97', 97,
        keyEvent.charCode);
    assertEquals('Only one key event should have been fired.', 1, eventsFired);

    eventsFired = 0;
    fireKeyDown(keyHandler, KeyCodes.A);
    fireKeyPress(keyHandler, undefined, 65);
    assertEquals(
        'Upper case A should fire a key event with the keycode 65', KeyCodes.A,
        keyEvent.keyCode);
    assertEquals(
        'Upper case A should fire a key event with the charcode 65', 65,
        keyEvent.charCode);
    assertEquals('Only one key event should have been fired.', 1, eventsFired);

    eventsFired = 0;
    fireKeyDown(keyHandler, KeyCodes.DELETE);
    assertEquals(
        'Delete should fire a key event with the keycode 46', KeyCodes.DELETE,
        keyEvent.keyCode);
    assertEquals(
        'Delete should fire a key event with the charcode 0', 0,
        keyEvent.charCode);
    assertEquals('Only one key event should have been fired.', 1, eventsFired);

    eventsFired = 0;
    fireKeyDown(keyHandler, KeyCodes.PERIOD);
    fireKeyPress(keyHandler, undefined, 46);
    assertEquals(
        'Period should fire a key event with the keycode 190', KeyCodes.PERIOD,
        keyEvent.keyCode);
    assertEquals(
        'Period should fire a key event with the charcode 46', 46,
        keyEvent.charCode);
    assertEquals('Only one key event should have been fired.', 1, eventsFired);
  },

  /**
   * Tests the key handler for the Gecko behavior with the following experiments
   * rolled out on Gecko:
   * - `keypress` events are not dispatched on non-printable character events.
   *    See: https://github.com/google/closure-library/issues/883
   * - `keyCode` is set to `charCode on keypress events for non-function keys.
   *    See: https://github.com/google/closure-library/issues/932
   */
  testGeckoStyleKeyHandling_includeBothExperiments() {
    userAgent.IE = false;
    userAgent.GECKO = true;
    userAgent.WEBKIT = false;
    userAgent.MAC = false;
    userAgent.WINDOWS = true;
    userAgent.LINUX = false;

    let eventsFired = 0;
    let keyEvent;
    const keyHandler = new KeyHandler();

    events.listen(keyHandler, KeyHandler.EventType.KEY, (e) => {
      keyEvent = e;
      eventsFired++;
    });

    fireKeyDown(keyHandler, KeyCodes.ENTER);
    fireKeyPress(keyHandler, KeyCodes.ENTER, KeyCodes.ENTER);
    assertEquals(
        'Enter should fire a key event with the keycode 13', KeyCodes.ENTER,
        keyEvent.keyCode);
    assertEquals(
        'Enter should fire a key event with the charcode 0', 0,
        keyEvent.charCode);
    assertEquals('Only one key event should have been fired.', 1, eventsFired);

    eventsFired = 0;
    fireKeyDown(keyHandler, KeyCodes.ESC);
    assertEquals(
        'Esc should fire a key event with the keycode 27', KeyCodes.ESC,
        keyEvent.keyCode);
    assertEquals(
        'Esc should fire a key event with the charcode 0', 0,
        keyEvent.charCode);
    assertEquals('Only one key event should have been fired.', 1, eventsFired);

    eventsFired = 0;
    fireKeyDown(keyHandler, KeyCodes.UP);
    assertEquals(
        'Up should fire a key event with the keycode 38', KeyCodes.UP,
        keyEvent.keyCode);
    assertEquals(
        'Up should fire a key event with the charcode 0', 0, keyEvent.charCode);
    assertEquals('Only one key event should have been fired.', 1, eventsFired);

    eventsFired = 0;
    fireKeyDown(
        keyHandler, KeyCodes.SEVEN, undefined, undefined, undefined, undefined,
        true);
    fireKeyPress(keyHandler, 38, 38, undefined, undefined, undefined, true);
    assertEquals(
        'Shift+7 should fire a key event with the keycode 55', KeyCodes.SEVEN,
        keyEvent.keyCode);
    assertEquals(
        'Shift+7 should fire a key event with the charcode 38', 38,
        keyEvent.charCode);
    assertEquals('Only one key event should have been fired.', 1, eventsFired);

    eventsFired = 0;
    fireKeyDown(keyHandler, KeyCodes.A);
    fireKeyPress(keyHandler, 97, 97);
    assertEquals(
        'Lower case a should fire a key event with the keycode 65', KeyCodes.A,
        keyEvent.keyCode);
    assertEquals(
        'Lower case a should fire a key event with the charcode 97', 97,
        keyEvent.charCode);
    assertEquals('Only one key event should have been fired.', 1, eventsFired);

    eventsFired = 0;
    fireKeyDown(keyHandler, KeyCodes.A);
    fireKeyPress(keyHandler, 65, 65);
    assertEquals(
        'Upper case A should fire a key event with the keycode 65', KeyCodes.A,
        keyEvent.keyCode);
    assertEquals(
        'Upper case A should fire a key event with the charcode 65', 65,
        keyEvent.charCode);
    assertEquals('Only one key event should have been fired.', 1, eventsFired);

    eventsFired = 0;
    fireKeyDown(keyHandler, KeyCodes.DELETE);
    assertEquals(
        'Delete should fire a key event with the keycode 46', KeyCodes.DELETE,
        keyEvent.keyCode);
    assertEquals(
        'Delete should fire a key event with the charcode 0', 0,
        keyEvent.charCode);
    assertEquals('Only one key event should have been fired.', 1, eventsFired);

    eventsFired = 0;
    fireKeyDown(keyHandler, KeyCodes.PERIOD);
    fireKeyPress(keyHandler, 46, 46);
    assertEquals(
        'Period should fire a key event with the keycode 190', KeyCodes.PERIOD,
        keyEvent.keyCode);
    assertEquals(
        'Period should fire a key event with the charcode 46', 46,
        keyEvent.charCode);
    assertEquals('Only one key event should have been fired.', 1, eventsFired);
  },

  /** Tests the key handler for the Safari 3 behavior. */
  testSafari3StyleKeyHandling() {
    userAgent.IE = false;
    userAgent.GECKO = false;
    userAgent.WEBKIT = true;
    userAgent.MAC = true;
    userAgent.WINDOWS = false;
    userAgent.LINUX = false;
    /** @suppress {checkTypes} suppression added to enable type checking */
    userAgent.VERSION = 525.3;

    let keyEvent;
    const keyHandler = new KeyHandler();

    // Make sure all events are caught while testing
    events.listen(keyHandler, KeyHandler.EventType.KEY, (e) => {
      keyEvent = e;
    });

    fireKeyDown(keyHandler, KeyCodes.ENTER);
    fireKeyPress(keyHandler, KeyCodes.ENTER);
    assertEquals(
        'Enter should fire a key event with the keycode 13', KeyCodes.ENTER,
        keyEvent.keyCode);
    assertEquals(
        'Enter should fire a key event with the charcode 0', 0,
        keyEvent.charCode);
    fireKeyUp(keyHandler, KeyCodes.ENTER);

    // Add a listener to ensure that an extra ENTER event is not dispatched
    // by a subsequent keypress.
    const enterCheck =
        events.listen(keyHandler, KeyHandler.EventType.KEY, (e) => {
          assertNotEquals(
              'Unexpected ENTER keypress dispatched', e.keyCode,
              KeyCodes.ENTER);
        });

    fireKeyDown(keyHandler, KeyCodes.ESC);
    assertEquals(
        'Esc should fire a key event with the keycode 27', KeyCodes.ESC,
        keyEvent.keyCode);
    assertEquals(
        'Esc should fire a key event with the charcode 0', 0,
        keyEvent.charCode);
    fireKeyPress(keyHandler, KeyCodes.ESC);
    events.unlistenByKey(enterCheck);

    fireKeyDown(keyHandler, KeyCodes.UP);
    assertEquals(
        'Up should fire a key event with the keycode 38', KeyCodes.UP,
        keyEvent.keyCode);
    assertEquals(
        'Up should fire a key event with the charcode 0', 0, keyEvent.charCode);

    fireKeyDown(
        keyHandler, KeyCodes.SEVEN, undefined, undefined, undefined, undefined,
        true);
    fireKeyPress(keyHandler, 38, 38, undefined, undefined, undefined, true);
    assertEquals(
        'Shift+7 should fire a key event with the keycode 55', KeyCodes.SEVEN,
        keyEvent.keyCode);
    assertEquals(
        'Shift+7 should fire a key event with the charcode 38', 38,
        keyEvent.charCode);

    fireKeyDown(keyHandler, KeyCodes.A);
    fireKeyPress(keyHandler, 97, 97);
    assertEquals(
        'Lower case a should fire a key event with the keycode 65', KeyCodes.A,
        keyEvent.keyCode);
    assertEquals(
        'Lower case a should fire a key event with the charcode 97', 97,
        keyEvent.charCode);

    fireKeyDown(keyHandler, KeyCodes.A);
    fireKeyPress(keyHandler, 65, 65);
    assertEquals(
        'Upper case A should fire a key event with the keycode 65', KeyCodes.A,
        keyEvent.keyCode);
    assertEquals(
        'Upper case A should fire a key event with the charcode 65', 65,
        keyEvent.charCode);

    fireKeyDown(keyHandler, KeyCodes.CTRL);
    fireKeyDown(keyHandler, KeyCodes.A, null, null, true /*ctrl*/);
    assertEquals(
        'A with control down should fire a key event', KeyCodes.A,
        keyEvent.keyCode);

    // Test that Alt-Tab outside the window doesn't break things.
    fireKeyDown(keyHandler, KeyCodes.ALT);
    keyEvent.keyCode = -1;  // Reset the event.
    fireKeyDown(keyHandler, KeyCodes.A);
    assertEquals('Should not have dispatched an Alt-A', -1, keyEvent.keyCode);
    fireKeyPress(keyHandler, 65, 65);
    assertEquals(
        'Alt should be ignored since it isn\'t currently depressed', KeyCodes.A,
        keyEvent.keyCode);

    fireKeyDown(keyHandler, KeyCodes.DELETE);
    assertEquals(
        'Delete should fire a key event with the keycode 46', KeyCodes.DELETE,
        keyEvent.keyCode);
    assertEquals(
        'Delete should fire a key event with the charcode 0', 0,
        keyEvent.charCode);

    fireKeyDown(keyHandler, KeyCodes.PERIOD);
    fireKeyPress(keyHandler, 46, 46);
    assertEquals(
        'Period should fire a key event with the keycode 190', KeyCodes.PERIOD,
        keyEvent.keyCode);
    assertEquals(
        'Period should fire a key event with the charcode 46', 46,
        keyEvent.charCode);

    // Safari sends zero key code for non-latin characters.
    fireKeyDown(keyHandler, 0, 0);
    fireKeyPress(keyHandler, 1092, 1092);
    assertEquals(
        'Cyrillic small letter "Ef" should fire a key event with ' +
            'the keycode 0',
        0, keyEvent.keyCode);
    assertEquals(
        'Cyrillic small letter "Ef" should fire a key event with ' +
            'the charcode 1092',
        1092, keyEvent.charCode);
  },

  testGeckoOnMacAltHandling() {
    userAgent.IE = false;
    userAgent.GECKO = true;
    userAgent.WEBKIT = false;
    userAgent.MAC = true;
    userAgent.WINDOWS = false;
    userAgent.LINUX = false;
    userAgent.EDGE = false;
    /** @suppress {visibility} suppression added to enable type checking */
    KeyHandler.SAVE_ALT_FOR_KEYPRESS_ = true;

    let keyEvent;
    const keyHandler = new KeyHandler();

    events.listen(keyHandler, KeyHandler.EventType.KEY, (e) => {
      keyEvent = e;
    });

    fireKeyDown(keyHandler, KeyCodes.COMMA, 0, null, false, true, false);
    fireKeyPress(keyHandler, 0, 8804, null, false, false, false);
    assertEquals(
        'should fire a key event with COMMA', KeyCodes.COMMA, keyEvent.keyCode);
    assertEquals(
        'should fire a key event with alt key set', true, keyEvent.altKey);

    // Scenario: alt down, a down, a press, a up (should say alt is true),
    // alt up.
    keyEvent = undefined;
    fireKeyDown(keyHandler, 18, 0, null, false, true, false);
    fireKeyDown(keyHandler, KeyCodes.A, 0, null, false, true, false);
    fireKeyPress(keyHandler, 0, 229, null, false, false, false);
    assertEquals(
        'should fire a key event with alt key set', true, keyEvent.altKey);
    fireKeyUp(keyHandler, 0, 229, null, false, true, false);
    assertEquals('alt key should still be set', true, keyEvent.altKey);
    fireKeyUp(keyHandler, 18, 0, null, false, false, false);
  },

  testGeckoEqualSign() {
    userAgent.IE = false;
    userAgent.GECKO = true;
    userAgent.WEBKIT = false;
    userAgent.MAC = false;
    userAgent.WINDOWS = true;
    userAgent.LINUX = false;

    let keyEvent;
    const keyHandler = new KeyHandler();

    events.listen(keyHandler, KeyHandler.EventType.KEY, (e) => {
      keyEvent = e;
    });

    fireKeyDown(keyHandler, 61, 0);
    fireKeyPress(keyHandler, 0, 61);
    assertEquals(
        '= should fire should fire a key event with the keyCode 187',
        KeyCodes.EQUALS, keyEvent.keyCode);
    assertEquals(
        '= should fire a key event with the charCode 61', KeyCodes.FF_EQUALS,
        keyEvent.charCode);
  },

  testGeckoDash() {
    userAgent.IE = false;
    userAgent.GECKO = true;
    userAgent.WEBKIT = false;
    userAgent.MAC = false;
    userAgent.WINDOWS = true;
    userAgent.LINUX = false;

    const keyEvents = [];
    const keyHandler = new KeyHandler();
    events.listen(keyHandler, KeyHandler.EventType.KEY, (e) => {
      keyEvents.push(e);
    });

    fireKeyDown(keyHandler, KeyCodes.FF_DASH, 0);
    fireKeyPress(keyHandler, 0, KeyCodes.FF_DASH);

    assertEquals('expected one key event to be fired', 1, keyEvents.length);
    assertEquals(
        '= should fire a key event with the keyCode 189', KeyCodes.DASH,
        keyEvents[0].keyCode);
    assertEquals(
        '= should fire a key event with the charCode 173', KeyCodes.FF_DASH,
        keyEvents[0].charCode);
  },

  testMacGeckoSlash() {
    userAgent.IE = false;
    userAgent.GECKO = true;
    userAgent.WEBKIT = false;
    userAgent.MAC = true;
    userAgent.WINDOWS = false;
    userAgent.LINUX = false;

    let keyEvent;
    const keyHandler = new KeyHandler();

    events.listen(keyHandler, KeyHandler.EventType.KEY, (e) => {
      keyEvent = e;
    });

    // On OS X Gecko, the following events are fired when pressing Shift+/
    // 1. keydown with keyCode=191 (/), charCode=0, shiftKey
    // 2. keypress with keyCode=0, charCode=63 (?), shiftKey
    fireKeyDown(keyHandler, 191, 0, null, false, false, true);
    fireKeyPress(keyHandler, 0, 63, null, false, false, true);
    assertEquals(
        '/ should fire a key event with the keyCode 191', KeyCodes.SLASH,
        keyEvent.keyCode);
    assertEquals(
        '? should fire a key event with the charCode 63',
        KeyCodes.QUESTION_MARK, keyEvent.charCode);
  },

  testGetElement() {
    const target = dom.createDom(TagName.DIV);
    const target2 = dom.createDom(TagName.DIV);
    let keyHandler = new KeyHandler();
    assertNull(keyHandler.getElement());

    keyHandler.attach(target);
    assertEquals(target, keyHandler.getElement());

    keyHandler.attach(target2);
    assertNotEquals(target, keyHandler.getElement());
    assertEquals(target2, keyHandler.getElement());

    const doc = dom.getDocument();
    keyHandler.attach(doc);
    assertEquals(doc, keyHandler.getElement());

    keyHandler = new KeyHandler(doc);
    assertEquals(doc, keyHandler.getElement());

    keyHandler = new KeyHandler(target);
    assertEquals(target, keyHandler.getElement());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testDetach() {
    const target = dom.createDom(TagName.DIV);
    const keyHandler = new KeyHandler(target);
    assertEquals(target, keyHandler.getElement());

    fireKeyDown(keyHandler, 0, 63, null, false, false, true);
    fireKeyPress(keyHandler, 0, 63, null, false, false, true);
    keyHandler.detach();

    assertNull(keyHandler.getElement());
    // All listeners should be cleared.
    assertNull(keyHandler.keyDownKey_);
    assertNull(keyHandler.keyPressKey_);
    assertNull(keyHandler.keyUpKey_);
    // All key related state should be cleared.
    assertEquals('Last key should be -1', -1, keyHandler.lastKey_);
    assertEquals('keycode should be -1', -1, keyHandler.keyCode_);
  },

  testCapturePhase() {
    let gotInCapturePhase;
    let gotInBubblePhase;

    const target = dom.createDom(TagName.DIV);
    events.listen(
        new KeyHandler(target, false /* bubble */), KeyHandler.EventType.KEY,
        () => {
          gotInBubblePhase = true;
          assertTrue(gotInCapturePhase);
        });
    events.listen(
        new KeyHandler(target, true /* capture */), KeyHandler.EventType.KEY,
        () => {
          gotInCapturePhase = true;
        });

    testingEvents.fireKeySequence(target, KeyCodes.ESC);
    assertTrue(gotInBubblePhase);
  },
});
