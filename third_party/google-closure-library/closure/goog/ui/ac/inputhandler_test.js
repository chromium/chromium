/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview
 * @suppress {const}
 */

goog.module('goog.ui.ac.InputHandlerTest');
goog.setTestOnly();

const BrowserEvent = goog.require('goog.events.BrowserEvent');
const GoogEvent = goog.require('goog.events.Event');
const GoogEventTarget = goog.require('goog.events.EventTarget');
const InputHandler = goog.require('goog.ui.ac.InputHandler');
const KeyCodes = goog.require('goog.events.KeyCodes');
const KeyHandler = goog.require('goog.events.KeyHandler');
const MockClock = goog.require('goog.testing.MockClock');
const Role = goog.require('goog.a11y.aria.Role');
const State = goog.require('goog.a11y.aria.State');
const TagName = goog.require('goog.dom.TagName');
const aria = goog.require('goog.a11y.aria');
const dom = goog.require('goog.dom');
const functions = goog.require('goog.functions');
const googObject = goog.require('goog.object');
const selection = goog.require('goog.dom.selection');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

/** Mock out the input element. */
class MockElement extends GoogEventTarget {
  constructor() {
    super();
    this.setAttributeNS = () => {};
    this.setAttribute = function(key, value) {
      /** @suppress {checkTypes} suppression added to enable type checking */
      this[key] = value;
    };
    this.focus = () => {};
    this.blur = () => {};
    this.ownerDocument = document;
    this.selectionStart = 0;
  }
}

class MockAutoCompleter {
  constructor() {
    this.setToken = null;
    this.setTokenWasCalled = false;
    this.selectHilitedWasCalled = false;
    this.dismissWasCalled = false;
    this.getTarget = () => mockElement;
    this.setTarget = () => {};
    this.setToken = function(token) {
      this.setTokenWasCalled = true;
      this.setToken = token;
    };
    this.selectHilited = function() {
      this.selectHilitedWasCalled = true;
      return true;  // Success.
    };
    this.cancelDelayedDismiss = () => {};
    this.dismissOnDelay = () => {};
    this.dismiss = function() {
      this.dismissWasCalled = true;
    };
    this.isOpen = functions.TRUE;
  }
}

/**
 * MockInputHandler simulates key events for testing the IME behavior of
 * InputHandler.
 */
class MockInputHandler extends InputHandler {
  constructor() {
    super();

    /** @suppress {checkTypes} suppression added to enable type checking */
    this.ac_ = new MockAutoCompleter();
    this.cursorPosition_ = 0;

    this.attachInput(mockElement);
  }

  /**
   * Checks for updates to the text area, should not happen during IME.
   * @suppress {checkTypes,missingProperties} suppression added to enable type
   * checking
   */
  update() {
    this.updates++;
  }

  /** Simulates key events. */
  fireKeyEvents(keyCode, down, press, up, properties = undefined) {
    if (down) this.fireEvent('keydown', keyCode, properties);
    if (press) this.fireEvent('keypress', keyCode, properties);
    if (up) this.fireEvent('keyup', keyCode, properties);
  }

  /** Simulates an event. */
  fireEvent(type, keyCode, properties = undefined) {
    let e = {};
    e.type = type;
    e.keyCode = keyCode;
    e.preventDefault = () => {};
    if (!userAgent.IE) {
      e.which = type == 'keydown' ? keyCode : 0;
    }
    if (properties) {
      googObject.extend(e, properties);
    }
    /** @suppress {checkTypes} suppression added to enable type checking */
    e = new BrowserEvent(e);
    mockElement.dispatchEvent(e);
  }

  setCursorPosition(cursorPosition) {
    this.cursorPosition_ = cursorPosition;
  }

  getCursorPosition() {
    return this.cursorPosition_;
  }
}

// Variables used by all test
let mh = null;
let oldMac;
let oldWin;
let oldLinux;
let oldIe;
let oldFf;
let oldWebkit;
let oldVersion;
let mockElement;
let mockClock;


/** Used to simulate behavior of Windows/Firefox */
function simulateWinFirefox() {
  userAgent.MAC = false;
  userAgent.WINDOWS = true;
  userAgent.LINUX = false;
  userAgent.IE = false;
  userAgent.EDGE = false;
  userAgent.EDGE_OR_IE = false;
  userAgent.GECKO = true;
  userAgent.WEBKIT = false;
}

/** Used to simulate behavior of Windows/InternetExplorer7 */
function simulateWinIe7() {
  userAgent.MAC = false;
  userAgent.WINDOWS = true;
  userAgent.LINUX = false;
  userAgent.IE = true;
  userAgent.EDGE = false;
  userAgent.EDGE_OR_IE = true;
  userAgent.DOCUMENT_MODE = 7;
  userAgent.GECKO = false;
  userAgent.WEBKIT = false;
}

/** Used to simulate behavior of Windows/Chrome */
function simulateWinChrome() {
  userAgent.MAC = false;
  userAgent.WINDOWS = true;
  userAgent.LINUX = false;
  userAgent.IE = false;
  userAgent.EDGE = false;
  userAgent.EDGE_OR_IE = false;
  userAgent.GECKO = false;
  userAgent.WEBKIT = true;
  userAgent.VERSION = '525';
}

/** Used to simulate behavior of Mac/Firefox */
function simulateMacFirefox() {
  userAgent.MAC = true;
  userAgent.WINDOWS = false;
  userAgent.LINUX = false;
  userAgent.IE = false;
  userAgent.EDGE = false;
  userAgent.EDGE_OR_IE = false;
  userAgent.GECKO = true;
  userAgent.WEBKIT = false;
}

/** Used to simulate behavior of Mac/Safari3 */
function simulateMacSafari3() {
  userAgent.MAC = true;
  userAgent.WINDOWS = false;
  userAgent.LINUX = false;
  userAgent.IE = false;
  userAgent.EDGE = false;
  userAgent.EDGE_OR_IE = false;
  userAgent.GECKO = false;
  userAgent.WEBKIT = true;
  userAgent.VERSION = '525';
}

/** Used to simulate behavior of Linux/Firefox */
function simulateLinuxFirefox() {
  userAgent.MAC = false;
  userAgent.WINDOWS = false;
  userAgent.LINUX = true;
  userAgent.IE = false;
  userAgent.EDGE = false;
  userAgent.EDGE_OR_IE = false;
  userAgent.GECKO = true;
  userAgent.WEBKIT = false;
}

testSuite({
  setUp() {
    oldMac = userAgent.MAC;
    oldWin = userAgent.WINDOWS;
    oldLinux = userAgent.LINUX;
    oldIe = userAgent.IE;
    oldFf = userAgent.GECKO;
    oldWebkit = userAgent.WEBKIT;
    oldVersion = userAgent.VERSION;
    mockClock = new MockClock(true);
    mockElement = new MockElement;
    mh = new MockInputHandler;
  },

  tearDown() {
    userAgent.MAC = oldMac;
    userAgent.WINDOWS = oldWin;
    userAgent.LINUX = oldLinux;
    userAgent.IE = oldIe;
    userAgent.GECKO = oldFf;
    userAgent.WEBKIT = oldWebkit;
    userAgent.VERSION = oldVersion;
    mockClock.dispose();
    mockElement.dispose();
  },

  /**
   * Test the normal, non-IME case
   * @suppress {visibility} suppression added to
   *      enable type checking
   */
  testRegularKey() {
    // Each key fires down, press, and up in that order, and each should
    // trigger an autocomplete update
    assertFalse('IME should not be triggered', mh.waitingForIme_);

    mh.fireKeyEvents(KeyCodes.K, true, true, true);
    assertFalse('IME should not be triggered by K', mh.waitingForIme_);

    mh.fireKeyEvents(KeyCodes.A, true, true, true);
    assertFalse('IME should not be triggered by A', mh.waitingForIme_);
  },

  /**
   * This test simulates the key inputs generated by pressing
   * '<ime_on>a<enter>i<ime_off>u' using the Japanese IME
   * on Windows/Firefox.
   * @suppress {visibility} suppression added to enable type checking
   */
  testImeWinFirefox() {
    simulateWinFirefox();
    mh.fireEvent('focus', '');
    assertFalse('IME should not be triggered', mh.waitingForIme_);

    // ime_on

    // a
    mh.fireKeyEvents(KeyCodes.WIN_IME, true, true, false);
    // Event is not generated for key code a.
    assertTrue('IME should be triggered', mh.waitingForIme_);

    // enter
    mh.fireKeyEvents(KeyCodes.ENTER, false, false, true);
    assertFalse('IME should not be triggered', mh.waitingForIme_);

    // i
    mh.fireKeyEvents(KeyCodes.WIN_IME, true, true, false);
    // Event is not generated for key code i.
    assertTrue('IME should be triggered', mh.waitingForIme_);

    // ime_off

    // u
    mh.fireKeyEvents(KeyCodes.U, true, true, true);
    assertFalse('IME should not be triggered', mh.waitingForIme_);

    mh.fireEvent('blur', '');
  },

  /**
   * This test simulates the key inputs generated by pressing
   * '<ime_on>a<enter>i<ime_off>u' using the Japanese IME
   * on Windows/InternetExplorer7.
   * @suppress {visibility} suppression added to enable type checking
   */
  testImeWinIe7() {
    simulateWinIe7();
    mh.fireEvent('focus', '');
    assertFalse('IME should not be triggered', mh.waitingForIme_);

    // ime_on

    // a
    mh.fireKeyEvents(KeyCodes.WIN_IME, true, false, false);
    mh.fireKeyEvents(KeyCodes.A, false, false, true);
    assertTrue('IME should be triggered', mh.waitingForIme_);

    // enter
    mh.fireKeyEvents(KeyCodes.WIN_IME, true, false, false);
    mh.fireKeyEvents(KeyCodes.ENTER, false, false, true);
    assertFalse('IME should not be triggered', mh.waitingForIme_);

    // i
    mh.fireKeyEvents(KeyCodes.WIN_IME, true, false, false);
    mh.fireKeyEvents(KeyCodes.I, false, false, true);
    assertTrue('IME should be triggered', mh.waitingForIme_);

    // ime_off

    // u
    mh.fireKeyEvents(KeyCodes.U, true, true, true);
    assertFalse('IME should not be triggered', mh.waitingForIme_);

    mh.fireEvent('blur', '');
  },

  /**
   * This test simulates the key inputs generated by pressing
   * '<ime_on>a<enter>i<ime_off>u' using the Japanese IME
   * on Windows/Chrome.
   * @suppress {visibility} suppression added to enable type checking
   */
  testImeWinChrome() {
    simulateWinChrome();
    mh.fireEvent('focus', '');
    assertFalse('IME should not be triggered', mh.waitingForIme_);

    // ime_on

    // a
    mh.fireKeyEvents(KeyCodes.WIN_IME, true, false, false);
    mh.fireKeyEvents(KeyCodes.A, false, false, true);
    assertTrue('IME should be triggered', mh.waitingForIme_);

    // enter
    mh.fireKeyEvents(KeyCodes.WIN_IME, true, false, false);
    mh.fireKeyEvents(KeyCodes.ENTER, false, false, true);
    assertFalse('IME should not be triggered', mh.waitingForIme_);

    // i
    mh.fireKeyEvents(KeyCodes.WIN_IME, true, false, false);
    mh.fireKeyEvents(KeyCodes.I, false, false, true);
    assertTrue('IME should be triggered', mh.waitingForIme_);

    // ime_off

    // u
    mh.fireKeyEvents(KeyCodes.U, true, true, true);
    assertFalse('IME should not be triggered', mh.waitingForIme_);

    mh.fireEvent('blur', '');
  },

  /**
   * This test simulates the key inputs generated by pressing
   * '<ime_on>a<enter>i<ime_off>u' using the Japanese IME
   * on Mac/Firefox.
   * @suppress {visibility} suppression added to enable type checking
   */
  testImeMacFirefox() {
    // TODO(user): Currently our code cannot distinguish preedit characters
    // from normal ones for Mac/Firefox.
    // Enable this test after we fix it.

    simulateMacFirefox();
    mh.fireEvent('focus', '');
    assertFalse('IME should not be triggered', mh.waitingForIme_);

    // ime_on

    // a
    mh.fireKeyEvents(KeyCodes.WIN_IME, true, true, false);
    assertTrue('IME should be triggered', mh.waitingForIme_);
    mh.fireKeyEvents(KeyCodes.A, true, false, true);
    assertTrue('IME should be triggered', mh.waitingForIme_);

    // enter
    mh.fireKeyEvents(KeyCodes.ENTER, true, true, true);
    assertFalse('IME should not be triggered', mh.waitingForIme_);

    // i
    mh.fireKeyEvents(KeyCodes.WIN_IME, true, true, false);
    mh.fireKeyEvents(KeyCodes.I, true, false, true);
    assertTrue('IME should be triggered', mh.waitingForIme_);

    // ime_off

    // u
    mh.fireKeyEvents(KeyCodes.U, true, true, true);
    assertFalse('IME should not be triggered', mh.waitingForIme_);

    mh.fireEvent('blur', '');
  },

  /**
   * This test simulates the key inputs generated by pressing
   * '<ime_on>a<enter>i<ime_off>u' using the Japanese IME
   * on Mac/Safari3.
   * @suppress {visibility} suppression added to enable type checking
   */
  testImeMacSafari3() {
    simulateMacSafari3();
    mh.fireEvent('focus', '');
    assertFalse('IME should not be triggered', mh.waitingForIme_);

    // ime_on

    // a
    mh.fireKeyEvents(KeyCodes.WIN_IME, true, false, false);
    mh.fireKeyEvents(KeyCodes.A, false, false, true);
    assertTrue('IME should be triggered', mh.waitingForIme_);

    // enter
    mh.fireKeyEvents(KeyCodes.WIN_IME, true, false, false);
    mh.fireKeyEvents(KeyCodes.ENTER, false, false, true);
    assertFalse('IME should not be triggered', mh.waitingForIme_);

    // i
    mh.fireKeyEvents(KeyCodes.WIN_IME, true, false, false);
    mh.fireKeyEvents(KeyCodes.I, false, false, true);
    assertTrue('IME should be triggered', mh.waitingForIme_);

    // ime_off

    // u
    mh.fireKeyEvents(KeyCodes.U, true, true, true);
    assertFalse('IME should not be triggered', mh.waitingForIme_);

    mh.fireEvent('blur', '');
  },

  /**
   * This test simulates the key inputs generated by pressing
   * '<ime_on>a<enter>i<ime_off>u' using the Japanese IME
   * on Linux/Firefox.
   * @suppress {visibility} suppression added to enable type checking
   */
  testImeLinuxFirefox() {
    // TODO(user): Currently our code cannot distinguish preedit characters
    // from normal ones for Linux/Firefox.
    // Enable this test after we fix it.

    simulateLinuxFirefox();
    mh.fireEvent('focus', '');
    assertFalse('IME should not be triggered', mh.waitingForIme_);

    // ime_on
    mh.fireKeyEvents(KeyCodes.WIN_IME, true, true, false);

    // a
    mh.fireKeyEvents(KeyCodes.A, true, false, true);
    assertTrue('IME should be triggered', mh.waitingForIme_);

    // enter
    mh.fireKeyEvents(KeyCodes.ENTER, true, true, true);
    assertFalse('IME should not be triggered', mh.waitingForIme_);

    // i
    mh.fireKeyEvents(KeyCodes.WIN_IME, true, true, false);
    mh.fireKeyEvents(KeyCodes.I, true, false, true);
    assertTrue('IME should be triggered', mh.waitingForIme_);

    // ime_off

    // u
    mh.fireKeyEvents(KeyCodes.U, true, true, true);
    assertFalse('IME should not be triggered', mh.waitingForIme_);

    mh.fireEvent('blur', '');
  },

  /**
     Check attaching to an EventTarget instead of an element.
     @suppress {visibility} suppression added to enable type checking
   */
  testAttachEventTarget1() {
    const target = new GoogEventTarget();

    assertNull(mh.activeElement_);
    mh.attachInput(target);
    assertNull(mh.activeElement_);

    mockElement.dispatchEvent(new GoogEvent('focus', mockElement));
    assertEquals(mockElement, mh.activeElement_);

    mh.detachInput(target);
  },

  /**
   * Make sure that the active element handling works.
   * @suppress {visibility} suppression added to enable type checking
   */
  testActiveElement() {
    assertNull(mh.activeElement_);

    mockElement.dispatchEvent('keydown');
    assertEquals(mockElement, mh.activeElement_);

    mockElement.dispatchEvent('blur');
    assertNull(mh.activeElement_);

    mockElement.dispatchEvent('focus');
    assertEquals(mockElement, mh.activeElement_);

    mh.detachInput(mockElement);
    assertNull(mh.activeElement_);
  },

  /**
   * We can attach an EventTarget that isn't an element.
   * @suppress {visibility} suppression added to enable type checking
   */
  testAttachEventTarget2() {
    const target = new GoogEventTarget();

    assertNull(mh.activeElement_);
    mh.attachInput(target);
    assertNull(mh.activeElement_);

    target.dispatchEvent(new GoogEvent('focus', mockElement));
    assertEquals(mockElement, mh.activeElement_);

    mh.detachInput(target);
  },

  /**
     Make sure an already-focused element becomes active immediately.
     @suppress {visibility} suppression added to enable type checking
   */
  testActiveElementAlreadyFocused() {
    const element = document.getElementById('textInput');
    element.style.display = '';
    element.focus();

    assertNull(mh.activeElement_);

    mh.attachInput(element);
    assertEquals(element, mh.activeElement_);

    mh.detachInput(element);
    element.style.display = 'none';
  },

  testUpdateDoesNotTriggerSetTokenForSelectRow() {
    const ih = new InputHandler();

    // Set up our input handler with the necessary mocks
    const mockAutoCompleter = new MockAutoCompleter();
    /** @suppress {checkTypes} suppression added to enable type checking */
    ih.ac_ = mockAutoCompleter;
    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    ih.activeElement_ = mockElement;

    const row = {};
    ih.selectRow(row, false);

    ih.update();
    assertFalse(
        'update should not call setToken on selectRow',
        mockAutoCompleter.setTokenWasCalled);

    ih.update();
    assertFalse(
        'update should not call setToken on selectRow',
        mockAutoCompleter.setTokenWasCalled);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSetTokenText() {
    const ih = new MockInputHandler();

    // Set up our input handler with the necessary mocks
    const mockAutoCompleter = new MockAutoCompleter();
    /** @suppress {checkTypes} suppression added to enable type checking */
    ih.ac_ = mockAutoCompleter;
    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    ih.activeElement_ = mockElement;
    /** @suppress {checkTypes} suppression added to enable type checking */
    mockElement.value = 'bob, wal, joey';
    ih.setCursorPosition(8);

    ih.setTokenText('waldo', true /* multi-row */);

    assertEquals('bob, waldo, joey', mockElement.value);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSetTokenTextLeftHandSideOfToken() {
    const ih = new MockInputHandler();
    ih.setSeparators(' ');
    ih.setWhitespaceWrapEntries(false);

    // Set up our input handler with the necessary mocks
    const mockAutoCompleter = new MockAutoCompleter();
    /** @suppress {checkTypes} suppression added to enable type checking */
    ih.ac_ = mockAutoCompleter;
    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    ih.activeElement_ = mockElement;
    /** @suppress {checkTypes} suppression added to enable type checking */
    mockElement.value = 'foo bar';
    // Sets cursor position right before 'bar'
    ih.setCursorPosition(4);

    ih.setTokenText('bar', true /* multi-row */);

    assertEquals('foo bar ', mockElement.value);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSetTokenTextAppendSeparator() {
    const ih = new MockInputHandler();
    ih.setSeparators('\n');
    ih.setWhitespaceWrapEntries(false);

    // Set up our input handler with the necessary mocks
    const mockAutoCompleter = new MockAutoCompleter();
    /** @suppress {checkTypes} suppression added to enable type checking */
    ih.ac_ = mockAutoCompleter;
    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    ih.activeElement_ = mockElement;
    /** @suppress {checkTypes} suppression added to enable type checking */
    mockElement.value = 'foo bar';
    ih.setCursorPosition(0);

    // The token is 'foo bar', we replace it with 'baz'.
    ih.setTokenText('baz', true /* multi-row */);

    assertEquals('baz\n', mockElement.value);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSetTokenTextDontAppendSeparator() {
    const ih = new MockInputHandler();
    ih.setSeparators('\n');
    ih.setWhitespaceWrapEntries(false);
    ih.setEndsWithSeparatorRegExp(null);

    // Set up our input handler with the necessary mocks
    const mockAutoCompleter = new MockAutoCompleter();
    /** @suppress {checkTypes} suppression added to enable type checking */
    ih.ac_ = mockAutoCompleter;
    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    ih.activeElement_ = mockElement;
    /** @suppress {checkTypes} suppression added to enable type checking */
    mockElement.value = 'foo bar';
    ih.setCursorPosition(0);

    // The token is 'foo bar', we replace it with 'baz'.
    ih.setTokenText('baz', true /* multi-row */);

    assertEquals('baz', mockElement.value);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testEmptyTokenWithSeparator() {
    const ih = new InputHandler();
    const mockAutoCompleter = new MockAutoCompleter();
    /** @suppress {checkTypes} suppression added to enable type checking */
    ih.ac_ = mockAutoCompleter;
    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    ih.activeElement_ = mockElement;
    /** @suppress {checkTypes} suppression added to enable type checking */
    mockElement.value = ', ,';
    // Sets cursor position before the second comma
    selection.setStart(mockElement, 2);

    ih.update();
    assertTrue(
        'update should call setToken on selectRow',
        mockAutoCompleter.setTokenWasCalled);
    assertEquals(
        'update should be called with empty string', '',
        mockAutoCompleter.setToken);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testNonEmptyTokenWithSeparator() {
    const ih = new InputHandler();
    const mockAutoCompleter = new MockAutoCompleter();
    /** @suppress {checkTypes} suppression added to enable type checking */
    ih.ac_ = mockAutoCompleter;
    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    ih.activeElement_ = mockElement;
    /** @suppress {checkTypes} suppression added to enable type checking */
    mockElement.value = ', joe ,';
    // Sets cursor position before the second comma
    selection.setStart(mockElement, 5);

    ih.update();
    assertTrue(
        'update should call setToken on selectRow',
        mockAutoCompleter.setTokenWasCalled);
    assertEquals(
        'update should be called with expected string', 'joe',
        mockAutoCompleter.setToken);
  },

  testGetThrottleTime() {
    const ih = new InputHandler();
    ih.setThrottleTime(999);
    assertEquals('throttle time set+get', 999, ih.getThrottleTime());
  },

  testGetUpdateDuringTyping() {
    const ih = new InputHandler();
    ih.setUpdateDuringTyping(false);
    assertFalse('update during typing set+get', ih.getUpdateDuringTyping());
  },

  testEnterToSelect() {
    mh.fireEvent('focus', '');
    mh.fireKeyEvents(KeyCodes.ENTER, true, true, true);
    assertTrue('Should hilite', mh.ac_.selectHilitedWasCalled);
    assertFalse('Should NOT be dismissed', mh.ac_.dismissWasCalled);
  },

  testEnterDoesNotSelectWhenClosed() {
    mh.fireEvent('focus', '');
    mh.ac_.isOpen = functions.FALSE;
    mh.fireKeyEvents(KeyCodes.ENTER, true, true, true);
    assertFalse('Should NOT hilite', mh.ac_.selectHilitedWasCalled);
    assertTrue('Should be dismissed', mh.ac_.dismissWasCalled);
  },

  testTabToSelect() {
    mh.fireEvent('focus', '');
    mh.fireKeyEvents(KeyCodes.TAB, true, true, true);
    assertTrue('Should hilite', mh.ac_.selectHilitedWasCalled);
    assertFalse('Should NOT be dismissed', mh.ac_.dismissWasCalled);
  },

  testTabDoesNotSelectWhenClosed() {
    mh.fireEvent('focus', '');
    mh.ac_.isOpen = functions.FALSE;
    mh.fireKeyEvents(KeyCodes.TAB, true, true, true);
    assertFalse('Should NOT hilite', mh.ac_.selectHilitedWasCalled);
    assertTrue('Should be dismissed', mh.ac_.dismissWasCalled);
  },

  testShiftTabDoesNotSelect() {
    mh.fireEvent('focus', '');
    mh.ac_.isOpen = functions.TRUE;
    mh.fireKeyEvents(KeyCodes.TAB, true, true, true, {shiftKey: true});
    assertFalse('Should NOT hilite', mh.ac_.selectHilitedWasCalled);
    assertTrue('Should be dismissed', mh.ac_.dismissWasCalled);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testEmptySeparatorUsesDefaults() {
    const inputHandler = new InputHandler('');
    assertFalse(inputHandler.separatorCheck_.test(''));
    assertFalse(inputHandler.separatorCheck_.test('x'));
    assertTrue(inputHandler.separatorCheck_.test(','));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testMultipleSeparatorUsesEmptyDefaults() {
    const inputHandler = new InputHandler(',\n', null, true);
    inputHandler.setWhitespaceWrapEntries(false);
    inputHandler.setSeparators(',\n', '');

    // Set up our input handler with the necessary mocks
    const mockAutoCompleter = new MockAutoCompleter();
    /** @suppress {checkTypes} suppression added to enable type checking */
    inputHandler.ac_ = mockAutoCompleter;
    /**
     * @suppress {visibility,checkTypes} suppression added to enable type
     * checking
     */
    inputHandler.activeElement_ = mockElement;
    /** @suppress {checkTypes} suppression added to enable type checking */
    mockElement.value = 'bob,wal';
    inputHandler.setCursorPosition(8);

    inputHandler.setTokenText('waldo', true /* multi-row */);

    assertEquals('bob,waldo', mockElement.value);
  },

  testAriaTags() {
    const target = dom.createDom(TagName.DIV);
    mh.attachInput(target);

    assertEquals(Role.COMBOBOX, aria.getRole(target));
    assertEquals('list', aria.getState(target, State.AUTOCOMPLETE));

    mh.detachInput(target);

    assertNull(aria.getRole(target));
    assertEquals('', aria.getState(target, State.AUTOCOMPLETE));
  },
});
