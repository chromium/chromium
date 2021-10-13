/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.events.PasteHandlerTest');
goog.setTestOnly();

const BrowserEvent = goog.require('goog.events.BrowserEvent');
const EventType = goog.require('goog.events.EventType');
const GoogEventTarget = goog.require('goog.events.EventTarget');
const KeyCodes = goog.require('goog.events.KeyCodes');
const MockClock = goog.require('goog.testing.MockClock');
const PasteHandler = goog.require('goog.events.PasteHandler');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const testSuite = goog.require('goog.testing.testSuite');

/** @suppress {checkTypes} suppression added to enable type checking */
function newBrowserEvent(type) {
  if (typeof type === 'string') {
    return new BrowserEvent({type: type});
  } else {
    return new BrowserEvent(type);
  }
}

let textarea;
let clock;
let handler;
let pasted;

testSuite({
  setUp() {
    textarea = new GoogEventTarget();
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    textarea.value = '';
    clock = new MockClock(true);
    /** @suppress {checkTypes} suppression added to enable type checking */
    handler = new PasteHandler(textarea);
    pasted = false;
    events.listen(handler, PasteHandler.EventType.PASTE, () => {
      pasted = true;
    });
  },

  tearDown() {
    textarea.dispose();
    handler.dispose();
    clock.dispose();
  },

  testDispatchingPasteEventSupportedByAFewBrowsersWork() {
    if (!PasteHandler.SUPPORTS_NATIVE_PASTE_EVENT) {
      return;
    }
    /** @suppress {checkTypes} suppression added to enable type checking */
    const handlerThatSupportsPasteEvents = new PasteHandler(textarea);
    // user clicks on the textarea and give it focus
    events.listen(
        handlerThatSupportsPasteEvents, PasteHandler.EventType.PASTE, () => {
          pasted = true;
        });
    textarea.dispatchEvent(newBrowserEvent('paste'));
    assertTrue(pasted);
  },

  testJustTypingDoesntFirePasteEvent() {
    if (PasteHandler.SUPPORTS_NATIVE_PASTE_EVENT) {
      return;
    }
    // user clicks on the textarea and give it focus
    textarea.dispatchEvent(newBrowserEvent(EventType.FOCUS));
    assertFalse(pasted);
    // user starts typing
    textarea.dispatchEvent(newBrowserEvent({
      type: EventType.KEYDOWN,
      keyCode: KeyCodes.A,
    }));
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    textarea.value = 'a';
    assertFalse(pasted);

    // still typing
    textarea.dispatchEvent({type: EventType.KEYDOWN, keyCode: KeyCodes.B});
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    textarea.value = 'ab';
    assertFalse(pasted);

    // ends typing
    textarea.dispatchEvent({type: EventType.KEYDOWN, keyCode: KeyCodes.C});
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    textarea.value = 'abc';
    assertFalse(pasted);
  },

  testStartsOnInitialState() {
    assertTrue(handler.getState() == PasteHandler.State.INIT);
    assertFalse(pasted);
  },

  testBlurOnInit() {
    if (PasteHandler.SUPPORTS_NATIVE_PASTE_EVENT) {
      return;
    }
    textarea.dispatchEvent(EventType.BLUR);
    assertTrue(handler.getState() == PasteHandler.State.INIT);
    assertFalse(pasted);
  },

  testFocusOnInit() {
    if (PasteHandler.SUPPORTS_NATIVE_PASTE_EVENT) {
      return;
    }
    textarea.dispatchEvent(EventType.FOCUS);
    assertTrue(handler.getState() == PasteHandler.State.FOCUSED);
    assertFalse(pasted);
  },

  testInputOnFocus() {
    if (PasteHandler.SUPPORTS_NATIVE_PASTE_EVENT) {
      return;
    }
    // user clicks on the textarea
    textarea.dispatchEvent(newBrowserEvent(EventType.FOCUS));
    clock.tick(PasteHandler.MANDATORY_MS_BETWEEN_INPUT_EVENTS_TIE_BREAKER + 1);
    // and right click -> paste a text!
    textarea.dispatchEvent(newBrowserEvent('input'));
    assertTrue(handler.getState() == PasteHandler.State.FOCUSED);
    // make sure we detected it
    assertTrue(pasted);
  },

  testKeyPressOnFocus() {
    if (PasteHandler.SUPPORTS_NATIVE_PASTE_EVENT) {
      return;
    }
    // user clicks on the textarea
    textarea.dispatchEvent(newBrowserEvent(EventType.FOCUS));

    // starts typing something
    textarea.dispatchEvent(newBrowserEvent({
      type: EventType.KEYDOWN,
      keyCode: KeyCodes.A,
    }));
    assertTrue(handler.getState() == PasteHandler.State.TYPING);
    assertFalse(pasted);

    // and then presses ctrl+v
    textarea.dispatchEvent(newBrowserEvent({
      type: EventType.KEYDOWN,
      keyCode: KeyCodes.V,
      ctrlKey: true,
    }));
    assertTrue(handler.getState() == PasteHandler.State.TYPING);

    // makes sure we detected it
    assertTrue(pasted);
  },

  testMouseOverOnInit() {
    if (PasteHandler.SUPPORTS_NATIVE_PASTE_EVENT) {
      return;
    }
    // user has something on the events
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    textarea.value = 'pasted string';
    // and right click -> paste it on the textarea, WITHOUT giving focus
    textarea.dispatchEvent(newBrowserEvent(EventType.MOUSEOVER));
    assertTrue(handler.getState() == PasteHandler.State.INIT);
    // makes sure we detect it
    assertTrue(pasted);

    pasted = false;

    // user normaly mouseovers the textarea, with no text change
    textarea.dispatchEvent(EventType.MOUSEOVER);
    assertTrue(handler.getState() == PasteHandler.State.INIT);
    // text area value doesn't change
    assertFalse(pasted);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testMouseOverAfterTyping() {
    if (PasteHandler.SUPPORTS_NATIVE_PASTE_EVENT) {
      return;
    }
    textarea.dispatchEvent(EventType.FOCUS);
    assertFalse(pasted);
    textarea.dispatchEvent({type: EventType.KEYDOWN, keyCode: KeyCodes.A});
    assertFalse(pasted);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    textarea.value = 'a';
    textarea.dispatchEvent('input');
    assertFalse(pasted);
    assertEquals('a', handler.oldValue_);
    textarea.dispatchEvent(EventType.MOUSEOVER);
    assertFalse(pasted);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testTypingAndThenRightClickPaste() {
    if (PasteHandler.SUPPORTS_NATIVE_PASTE_EVENT) {
      return;
    }
    textarea.dispatchEvent(EventType.FOCUS);

    textarea.dispatchEvent({type: EventType.KEYDOWN, keyCode: KeyCodes.A});
    assertFalse(pasted);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    textarea.value = 'a';
    clock.tick(PasteHandler.MANDATORY_MS_BETWEEN_INPUT_EVENTS_TIE_BREAKER + 1);
    textarea.dispatchEvent('input');
    assertFalse(pasted);

    assertEquals('a', handler.oldValue_);

    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    textarea.value = 'ab';
    clock.tick(PasteHandler.MANDATORY_MS_BETWEEN_INPUT_EVENTS_TIE_BREAKER + 1);
    textarea.dispatchEvent(newBrowserEvent('input'));
    assertTrue(pasted);
  },

  testTypingReallyFastDispatchesTwoInputEventsBeforeTheKeyDownEvent() {
    if (PasteHandler.SUPPORTS_NATIVE_PASTE_EVENT) {
      return;
    }
    textarea.dispatchEvent(EventType.FOCUS);

    // keydown and input events seems to be fired indepently: even though input
    // should happen after the key event, it doesn't if the user types fast
    // enough. FF2 + linux doesn't fire keydown events for every key pressed
    // when you type fast enough. if one of the keydown events gets swallowed,
    // two input events are fired consecutively. notice that there is a similar
    // scenario, that actually does produce a valid paste action.
    // {@see testRightClickRightClickAlsoDispatchesTwoConsecutiveInputEvents}

    textarea.dispatchEvent({type: EventType.KEYDOWN, keyCode: KeyCodes.A});
    assertFalse(pasted);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    textarea.value = 'a';
    clock.tick(PasteHandler.MANDATORY_MS_BETWEEN_INPUT_EVENTS_TIE_BREAKER - 1);
    textarea.dispatchEvent('input');
    assertFalse(pasted);

    // second key down events gets fired on a different order
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    textarea.value = 'ab';
    clock.tick(PasteHandler.MANDATORY_MS_BETWEEN_INPUT_EVENTS_TIE_BREAKER - 1);
    textarea.dispatchEvent('input');
    assertFalse(pasted);
  },

  testRightClickRightClickAlsoDispatchesTwoConsecutiveInputEvents() {
    if (PasteHandler.SUPPORTS_NATIVE_PASTE_EVENT) {
      return;
    }
    textarea.dispatchEvent(EventType.FOCUS);

    // there is also another case that two consecutive INPUT events are fired,
    // but in a valid paste action: if the user edit -> paste -> edit -> paste,
    // it is a valid paste action.

    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    textarea.value = 'a';
    clock.tick(PasteHandler.MANDATORY_MS_BETWEEN_INPUT_EVENTS_TIE_BREAKER + 1);
    textarea.dispatchEvent(newBrowserEvent('input'));
    assertTrue(pasted);

    // second key down events gets fired on a different order
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    textarea.value = 'ab';
    clock.tick(PasteHandler.MANDATORY_MS_BETWEEN_INPUT_EVENTS_TIE_BREAKER + 1);
    textarea.dispatchEvent(newBrowserEvent('input'));
    assertTrue(pasted);
  },

  testMiddleClickWithoutFocusTriggersPasteEvent() {
    if (PasteHandler.SUPPORTS_NATIVE_PASTE_EVENT) {
      return;
    }
    // if the textarea is NOT selected, and then we use the middle button,
    // FF2+linux pastes what was last highlighted, causing a paste action.
    textarea.dispatchEvent(EventType.FOCUS);
    textarea.dispatchEvent(newBrowserEvent('input'));
    assertTrue(pasted);
  },

  testScriptingDoesntTriggerPasteEvents() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const handlerUsedToListenForScriptingChanges = new PasteHandler(textarea);
    pasted = false;
    // user clicks on the textarea and give it focus
    events.listen(
        handlerUsedToListenForScriptingChanges, PasteHandler.EventType.PASTE,
        () => {
          pasted = true;
        });
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    dom.getElement('foo').value = 'dear paste handler,';
    assertFalse(pasted);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    dom.getElement('foo').value = 'please dont misunderstand script changes';
    assertFalse(pasted);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    dom.getElement('foo').value = 'with user generated paste events';
    assertFalse(pasted);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    dom.getElement('foo').value = 'thanks!';
    assertFalse(pasted);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testAfterPaste() {
    if (!PasteHandler.SUPPORTS_NATIVE_PASTE_EVENT) {
      return;
    }
    /** @suppress {checkTypes} suppression added to enable type checking */
    const handlerThatSupportsPasteEvents = new PasteHandler(textarea);
    pasted = false;
    events.listen(
        handlerThatSupportsPasteEvents, PasteHandler.EventType.PASTE, () => {
          pasted = true;
        });
    let afterPasteFired = false;
    events.listen(
        handlerThatSupportsPasteEvents, PasteHandler.EventType.AFTER_PASTE,
        () => {
          afterPasteFired = true;
        });

    // Initial paste event comes before AFTER_PASTE has fired.
    textarea.dispatchEvent(newBrowserEvent('paste'));
    assertTrue(pasted);
    assertFalse(afterPasteFired);

    // Once text is pasted, it takes a bit to detect it, at which point
    // AFTER_PASTE is fired.
    clock.tick(PasteHandler.PASTE_POLLING_PERIOD_MS_);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    textarea.value = 'text';
    clock.tick(PasteHandler.PASTE_POLLING_PERIOD_MS_);
    assertTrue(afterPasteFired);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testAfterPasteNotFiredIfDelayTooLong() {
    if (!PasteHandler.SUPPORTS_NATIVE_PASTE_EVENT) {
      return;
    }
    /** @suppress {checkTypes} suppression added to enable type checking */
    const handlerThatSupportsPasteEvents = new PasteHandler(textarea);
    pasted = false;
    events.listen(
        handlerThatSupportsPasteEvents, PasteHandler.EventType.PASTE, () => {
          pasted = true;
        });
    let afterPasteFired = false;
    events.listen(
        handlerThatSupportsPasteEvents, PasteHandler.EventType.AFTER_PASTE,
        () => {
          afterPasteFired = true;
        });

    // Initial paste event comes before AFTER_PASTE has fired.
    textarea.dispatchEvent(newBrowserEvent('paste'));
    assertTrue(pasted);
    assertFalse(afterPasteFired);

    // If the new text doesn't show up in time, we never fire AFTER_PASTE.
    clock.tick(PasteHandler.PASTE_POLLING_TIMEOUT_MS_);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    textarea.value = 'text';
    clock.tick(PasteHandler.PASTE_POLLING_PERIOD_MS_);
    assertFalse(afterPasteFired);
  },
});
