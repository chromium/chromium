/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.events.ImeHandlerTest');
goog.setTestOnly();

const GoogTestingEvent = goog.require('goog.testing.events.Event');
const ImeHandler = goog.require('goog.events.ImeHandler');
const KeyCodes = goog.require('goog.events.KeyCodes');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const googObject = goog.require('goog.object');
const googString = goog.require('goog.string');
const googUserAgent = goog.require('goog.userAgent');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');

let sandbox;
let imeHandler;
let eventsFired;
const stubs = new PropertyReplacer();
const eventTypes = ImeHandler.EventType;

function initImeHandler() {
  ImeHandler.USES_COMPOSITION_EVENTS = googUserAgent.GECKO ||
      (googUserAgent.WEBKIT && googUserAgent.isVersionOrHigher(532));
  imeHandler = new ImeHandler(sandbox);
  eventsFired = [];
  events.listen(imeHandler, googObject.getValues(ImeHandler.EventType), (e) => {
    eventsFired.push(e.type);
  });
}

function assertEventsFired(var_args) {
  assertArrayEquals(Array.prototype.slice.call(arguments), eventsFired);
}

function fireInputEvent(type) {
  return testingEvents.fireBrowserEvent(new GoogTestingEvent(type, sandbox));
}

function fireImeKeySequence() {
  return fireKeySequence(KeyCodes.WIN_IME);
}

/**
 * @suppress {strictPrimitiveOperators} suppression added to enable type
 * checking
 */
function fireKeySequence(keyCode) {
  return (
      testingEvents.fireBrowserEvent(
          new GoogTestingEvent('textInput', sandbox)) &
      testingEvents.fireKeySequence(sandbox, keyCode));
}

function runChromeCompositionEvents(platform) {
  setUserAgent('WEBKIT');
  setVersion(532);
  stubs.set(googUserAgent, platform, true);
  initImeHandler();

  fireImeKeySequence();

  fireInputEvent('compositionstart');
  assertImeMode();

  fireInputEvent('compositionupdate');
  fireInputEvent('compositionupdate');

  fireInputEvent('compositionend');
  assertEventsFired(
      eventTypes.START, eventTypes.UPDATE, eventTypes.UPDATE, eventTypes.END);
  assertNotImeMode();
}

function assertScimInputIgnored() {
  initImeHandler();

  fireImeKeySequence();
  assertNotImeMode();

  fireInputEvent('compositionstart');
  assertImeMode();

  fireImeKeySequence();
  assertImeMode();

  fireInputEvent('compositionend');
  assertNotImeMode();
}

const userAgents = ['IE', 'GECKO', 'WEBKIT'];

function setUserAgent(userAgent) {
  for (let i = 0; i < userAgents.length; i++) {
    stubs.set(googUserAgent, userAgents[i], userAgents[i] == userAgent);
  }
}

function setVersion(version) {
  googUserAgent.VERSION = version;
  /**
   * @suppress {visibility,checkTypes,constantProperty} suppression added to
   * enable type checking
   */
  googUserAgent.isVersionOrHigherCache_ = {};
}

function assertImeMode() {
  assertTrue('Should be in IME mode.', imeHandler.isImeMode());
}

function assertNotImeMode() {
  assertFalse('Should not be in IME mode.', imeHandler.isImeMode());
}
testSuite({
  setUp() {
    sandbox = dom.getElement('sandbox');
  },

  tearDown() {
    imeHandler.dispose();
    imeHandler = null;

    stubs.reset();
  },

  tearDownPage() {
    // Set up a test bed.
    sandbox.innerHTML = '<div contentEditable="true">hello world</div>';
    initImeHandler();

    function unshiftEvent(e) {
      last10Events.unshift(
          e.type + ':' + e.keyCode + ':' +
          googString.htmlEscape(dom.getTextContent(sandbox)));
      last10Events.length = Math.min(last10Events.length, 10);
      dom.getElement('logger').innerHTML = last10Events.join('<br>');
    }

    const last10Events = [];
    events.listen(
        imeHandler, googObject.getValues(ImeHandler.EventType), unshiftEvent);
    events.listen(sandbox, ['keydown', 'textInput'], unshiftEvent);
  },

  testHandleKeyDown_GeckoCompositionEvents() {
    // This test verifies that our IME functions can dispatch IME events to
    // InputHandler in the expected order on Gecko.

    // Set the userAgent used for this test to Firefox.
    setUserAgent('GECKO');
    stubs.set(googUserAgent, 'MAC', false);
    initImeHandler();

    fireInputEvent('compositionstart');
    assertImeMode();

    fireInputEvent('compositionupdate');
    fireInputEvent('compositionupdate');

    fireInputEvent('compositionend');

    assertEventsFired(
        eventTypes.START, eventTypes.UPDATE, eventTypes.UPDATE, eventTypes.END);
    assertNotImeMode();
  },

  /**
   * Verifies that our IME functions can dispatch IME events to the input
   * handler in the expected order on Chrome. jsUnitFarm does not have Linux
   * Chrome or Mac Chrome. So, we manually change the platform and run this test
   * three times.
   */
  testChromeCompositionEventsLinux() {
    runChromeCompositionEvents('LINUX');
  },

  testChromeCompositionEventsMac() {
    runChromeCompositionEvents('MAC');
  },

  testChromeCompositionEventsWindows() {
    runChromeCompositionEvents('WINDOWS');
  },

  /** Ensures that the IME mode turn on/off correctly. */
  testHandlerKeyDownForIme_imeOnOff() {
    setUserAgent('IE');
    initImeHandler();

    // Send a WIN_IME keyDown event and see whether IME mode turns on.
    fireImeKeySequence();
    assertImeMode();

    // Send keyDown events which should not turn off IME mode and see whether
    // IME mode holds on.
    fireKeySequence(KeyCodes.SHIFT);
    assertImeMode();

    fireKeySequence(KeyCodes.CTRL);
    assertImeMode();

    // Send a keyDown event with keyCode = ENTER and see whether IME mode
    // turns off.
    fireKeySequence(KeyCodes.ENTER);
    assertNotImeMode();

    assertEventsFired(eventTypes.START, eventTypes.END);
  },

  /**
   * Ensures that IME mode turns off when keyup events which are involved
   * in committing IME text occurred in Safari.
   */
  testHandleKeyUpForSafari() {
    setUserAgent('WEBKIT');
    setVersion(531);
    initImeHandler();

    fireImeKeySequence();
    assertImeMode();

    fireKeySequence(KeyCodes.ENTER);
    assertNotImeMode();
  },

  /**
   * SCIM on Linux will fire WIN_IME keycodes for random characters.
   * Fortunately, all Linux-based browsers use composition events.
   * This test just verifies that we ignore the WIN_IME keycodes.
   */
  testScimFiresWinImeKeycodesGeckoLinux() {
    setUserAgent('GECKO');
    assertScimInputIgnored();
  },

  testScimFiresWinImeKeycodesChromeLinux() {
    setUserAgent('WEBKIT');
    setVersion(532);
    assertScimInputIgnored();
  },
});
