/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.KeyboardShortcutHandlerTest');
goog.setTestOnly();

const BrowserEvent = goog.require('goog.events.BrowserEvent');
const KeyCodes = goog.require('goog.events.KeyCodes');
const KeyboardShortcutHandler = goog.require('goog.ui.KeyboardShortcutHandler');
const MockClock = goog.require('goog.testing.MockClock');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const StrictMock = goog.require('goog.testing.StrictMock');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');
const userAgent = goog.require('goog.userAgent');

const Modifiers = KeyboardShortcutHandler.Modifiers;

let handler;
let targetDiv;
let listener;
let mockClock;
const stubs = new PropertyReplacer();

/**
 * Fires a keypress on the target div.
 * @return {boolean} The returnValue of the sequence: false if preventDefault()
 *     was called on any of the events, true otherwise.
 */
function fire(keycode, extraProperties = undefined, element = undefined) {
  return testingEvents.fireKeySequence(
      element || targetDiv, keycode, extraProperties);
}

/**
 * Simulates a complete keystroke (keydown, keypress, and keyup) when typing
 * a non-ASCII character.
 * @param {number} keycode The keycode of the keydown and keyup events.
 * @param {number} keyPressKeyCode The keycode of the keypress event.
 * @param {Object=} extraProperties Event properties to be mixed into the
 *     BrowserEvent.
 * @param {EventTarget=} element Optional target for the event.
 * @return {boolean} The returnValue of the sequence: false if preventDefault()
 *     was called on any of the events, true otherwise.
 */
function fireAltGraphKey(
    keycode, keyPressKeyCode, extraProperties = undefined,
    element = undefined) {
  return testingEvents.fireNonAsciiKeySequence(
      element || targetDiv, keycode, keyPressKeyCode, extraProperties);
}

/**
 * Registers a slew of keyboard shortcuts to test each primary category
 * of shortcuts.
 */
function registerEnterSpaceXF1AltY() {
  // Enter and space are specially handled keys.
  handler.registerShortcut('enter', KeyCodes.ENTER);
  handler.registerShortcut('space', KeyCodes.SPACE);
  // 'x' should be treated as text in many contexts
  handler.registerShortcut('x', 'x');
  // F1 is a global shortcut.
  handler.registerShortcut('global', KeyCodes.F1);
  // Alt-Y has modifiers, which pass through most form elements.
  handler.registerShortcut('withAlt', 'alt+y');
}

/**
 * Fires enter, space, X, F1, and Alt-Y keys on a widget.
 * @param {Element} target The element on which to fire the events.
 * @param {Object?=} extraProperties Event properties to be mixed into the
 *     BrowserEvent.
 */
function fireEnterSpaceXF1AltY(target, extraProperties) {
  if (!extraProperties) {
    extraProperties = {};
  }

  fire(KeyCodes.ENTER, extraProperties, target);
  fire(KeyCodes.SPACE, extraProperties, target);
  fire(KeyCodes.X, extraProperties, target);
  fire(KeyCodes.F1, extraProperties, target);
  fire(KeyCodes.Y, Object.assign({}, {altKey: true}, extraProperties), target);
}

/**
 * Checks that the shortcuts are fired on each target.
 * @param {Array<string>} shortcuts A list of shortcut identifiers.
 * @param {Array<string>} targets A list of element IDs.
 * @param {function(Element)} fireEvents Function that fires events.
 * @suppress {missingProperties} suppression added to enable type checking
 */
function expectShortcutsOnTargets(shortcuts, targets, fireEvents) {
  for (let i = 0, ii = targets.length; i < ii; i++) {
    for (let j = 0, jj = shortcuts.length; j < jj; j++) {
      listener.shortcutFired(shortcuts[j]);
    }
    listener.$replay();
    fireEvents(dom.getElement(targets[i]));
    listener.$verify();
    listener.$reset();
  }
}

// Regression test for failure to reset keyCode between strokes.

testSuite({
  setUp() {
    targetDiv = dom.getElement('targetDiv');
    handler = new KeyboardShortcutHandler(dom.getElement('rootDiv'));

    // Create a mock event listener in order to set expectations on what
    // events are fired.  We create a fake class whose only method is
    // shortcutFired(shortcut identifier).
    listener = new StrictMock({shortcutFired: goog.nullFunction});
    events.listen(
        handler, KeyboardShortcutHandler.EventType.SHORTCUT_TRIGGERED,
        /**
           @suppress {missingProperties} suppression added to enable type
           checking
         */
        (event) => {
          listener.shortcutFired(event.identifier);
        });

    // Set up a fake clock, because keyboard shortcuts *are* time
    // sensitive.
    mockClock = new MockClock(true);
  },

  tearDown() {
    mockClock.uninstall();
    handler.dispose();
    stubs.reset();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testAllowsSingleLetterKeyBindingsSpecifiedAsString() {
    listener.shortcutFired('lettergee');
    listener.$replay();

    handler.registerShortcut('lettergee', 'g');
    fire(KeyCodes.G);

    listener.$verify();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testAllowsSingleLetterKeyBindingsSpecifiedAsStringKeyValue() {
    listener.shortcutFired('lettergee');
    listener.$replay();

    handler.registerShortcut('lettergee', 'g');
    fire('g');

    listener.$verify();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testAllowsSingleLetterKeyBindingsSpecifiedAsKeyCode() {
    listener.shortcutFired('lettergee');
    listener.$replay();

    handler.registerShortcut('lettergee', KeyCodes.G);
    fire(KeyCodes.G);

    listener.$verify();
  },

  testDoesntFireWhenWrongKeyIsPressed() {
    listener.$replay();  // no events expected

    handler.registerShortcut('letterjay', 'j');
    fire(KeyCodes.G);
    fire('g');

    listener.$verify();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testAllowsControlAndLetterSpecifiedAsAString() {
    listener.shortcutFired('lettergee');
    listener.$replay();

    handler.registerShortcut('lettergee', 'ctrl+g');
    fire(KeyCodes.G, {ctrlKey: true});

    listener.$verify();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testAllowsControlAndLetterSpecifiedAsAStringKeyValue() {
    listener.shortcutFired('lettergee');
    listener.$replay();

    handler.registerShortcut('lettergee', 'ctrl+g');
    fire('g', {ctrlKey: true});

    listener.$verify();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testAllowsControlAndLetterSpecifiedAsArgSequence() {
    listener.shortcutFired('lettergeectrl');
    listener.$replay();

    handler.registerShortcut('lettergeectrl', KeyCodes.G, Modifiers.CTRL);
    fire(KeyCodes.G, {ctrlKey: true});

    listener.$verify();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testAllowsControlAndLetterSpecifiedAsArray() {
    listener.shortcutFired('lettergeectrl');
    listener.$replay();

    handler.registerShortcut('lettergeectrl', [KeyCodes.G, Modifiers.CTRL]);
    fire(KeyCodes.G, {ctrlKey: true});

    listener.$verify();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testAllowsShift() {
    listener.shortcutFired('lettergeeshift');
    listener.$replay();

    handler.registerShortcut('lettergeeshift', [KeyCodes.G, Modifiers.SHIFT]);
    fire(KeyCodes.G, {shiftKey: true});

    listener.$verify();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testAllowsAlt() {
    listener.shortcutFired('lettergeealt');
    listener.$replay();

    handler.registerShortcut('lettergeealt', [KeyCodes.G, Modifiers.ALT]);
    fire(KeyCodes.G, {altKey: true});

    listener.$verify();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testAllowsMeta() {
    listener.shortcutFired('lettergeemeta');
    listener.$replay();

    handler.registerShortcut('lettergeemeta', [KeyCodes.G, Modifiers.META]);
    fire(KeyCodes.G, {metaKey: true});

    listener.$verify();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testAllowsMultipleModifiers() {
    listener.shortcutFired('lettergeectrlaltshift');
    listener.$replay();

    handler.registerShortcut(
        'lettergeectrlaltshift', KeyCodes.G,
        Modifiers.CTRL | Modifiers.ALT | Modifiers.SHIFT);
    fireAltGraphKey(
        KeyCodes.G, 0, {ctrlKey: true, altKey: true, shiftKey: true});

    listener.$verify();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testAllowsMultipleModifiersSpecifiedAsString() {
    listener.shortcutFired('lettergeectrlaltshiftmeta');
    listener.$replay();

    handler.registerShortcut(
        'lettergeectrlaltshiftmeta', 'ctrl+shift+alt+meta+g');
    fireAltGraphKey(
        KeyCodes.G, 0,
        {ctrlKey: true, altKey: true, shiftKey: true, metaKey: true});

    listener.$verify();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testPreventsDefaultOnReturnFalse() {
    listener.shortcutFired('x');
    listener.$replay();

    handler.registerShortcut('x', 'x');
    const key = events.listen(
        handler, KeyboardShortcutHandler.EventType.SHORTCUT_TRIGGERED,
        (event) => false);

    assertFalse(
        'return false in listener must prevent default', fire(KeyCodes.X));

    listener.$verify();

    events.unlistenByKey(key);
  },

  testPreventsDefaultWhenExceptionThrown() {
    handler.registerShortcut('x', 'x');
    handler.setAlwaysPreventDefault(true);
    events.listenOnce(
        handler, KeyboardShortcutHandler.EventType.SHORTCUT_TRIGGERED,
        (event) => {
          throw new Error('x');
        });

    // We can't use the standard infrastructure to detect that
    // the event was preventDefaulted, because of the exception.
    let callCount = 0;
    stubs.set(BrowserEvent.prototype, 'preventDefault', () => {
      callCount++;
    });

    const e = assertThrows(goog.partial(fire, KeyCodes.X));
    assertEquals('x', e.message);

    assertEquals(1, callCount);
  },

  testDoesntFireWhenUserForgetsRequiredModifier() {
    listener.$replay();  // no events expected

    handler.registerShortcut('lettergeectrl', KeyCodes.G, Modifiers.CTRL);
    fire(KeyCodes.G);

    listener.$verify();
  },

  testDoesntFireIfTooManyModifiersPressed() {
    listener.$replay();  // no events expected

    handler.registerShortcut('lettergeectrl', KeyCodes.G, Modifiers.CTRL);
    fire(KeyCodes.G, {ctrlKey: true, metaKey: true});

    listener.$verify();
  },

  testDoesntFireIfAnyRequiredModifierForgotten() {
    listener.$replay();  // no events expected

    handler.registerShortcut(
        'lettergeectrlaltshift', KeyCodes.G,
        Modifiers.CTRL | Modifiers.ALT | Modifiers.SHIFT);
    fire(KeyCodes.G, {altKey: true, shiftKey: true});

    listener.$verify();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testAllowsMultiKeySequenceSpecifiedAsArray() {
    listener.shortcutFired('quitemacs');
    listener.$replay();

    handler.registerShortcut(
        'quitemacs', [KeyCodes.X, Modifiers.CTRL, KeyCodes.C]);
    assertFalse(fire(KeyCodes.X, {ctrlKey: true}));
    fire(KeyCodes.C);

    listener.$verify();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testAllowsMultiKeySequenceSpecifiedAsArguments() {
    listener.shortcutFired('quitvi');
    listener.$replay();

    handler.registerShortcut(
        'quitvi', KeyCodes.SEMICOLON, Modifiers.SHIFT, KeyCodes.Q,
        Modifiers.NONE, KeyCodes.NUM_ONE, Modifiers.SHIFT);
    const shiftProperties = {shiftKey: true};
    assertFalse(fire(KeyCodes.SEMICOLON, shiftProperties));
    assertFalse(fire(KeyCodes.Q));
    fire(KeyCodes.NUM_ONE, shiftProperties);

    listener.$verify();
  },

  testMultiKeyEventIsNotFiredIfUserIsTooSlow() {
    listener.$replay();  // no events expected

    handler.registerShortcut(
        'quitemacs', [KeyCodes.X, Modifiers.CTRL, KeyCodes.C]);

    fire(KeyCodes.X, {ctrlKey: true});

    // Wait 3 seconds before hitting C.  Although the actual limit is 1500
    // at time of writing, it's best not to over-specify functionality.
    mockClock.tick(3000);

    fire(KeyCodes.C);

    listener.$verify();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testAllowsMultipleAHandlers() {
    listener.shortcutFired('quitvi');
    listener.shortcutFired('letterex');
    listener.shortcutFired('quitemacs');
    listener.$replay();

    // register 3 handlers in 3 diferent ways
    handler.registerShortcut(
        'quitvi', KeyCodes.SEMICOLON, Modifiers.SHIFT, KeyCodes.Q,
        Modifiers.NONE, KeyCodes.NUM_ONE, Modifiers.SHIFT);
    handler.registerShortcut(
        'quitemacs', [KeyCodes.X, Modifiers.CTRL, KeyCodes.C]);
    handler.registerShortcut('letterex', 'x');

    // quit vi
    const shiftProperties = {shiftKey: true};
    fire(KeyCodes.SEMICOLON, shiftProperties);
    fire(KeyCodes.Q);
    fire(KeyCodes.NUM_ONE, shiftProperties);

    // then press the letter x
    fire(KeyCodes.X);

    // then quit emacs
    fire(KeyCodes.X, {ctrlKey: true});
    fire(KeyCodes.C);

    listener.$verify();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testCanRemoveOneHandler() {
    listener.shortcutFired('letterex');
    listener.$replay();

    // register 2 handlers, then remove quitvi
    handler.registerShortcut(
        'quitvi', KeyCodes.SEMICOLON, Modifiers.SHIFT, KeyCodes.Q,
        Modifiers.NONE, KeyCodes.ONE, Modifiers.SHIFT);
    handler.registerShortcut('letterex', 'x');
    handler.unregisterShortcut(
        KeyCodes.SEMICOLON, Modifiers.SHIFT, KeyCodes.Q, Modifiers.NONE,
        KeyCodes.ONE, Modifiers.SHIFT);

    // call the "quit VI" keycodes, even though it is removed
    fire(KeyCodes.SEMICOLON, Modifiers.SHIFT);
    fire(KeyCodes.Q);
    fire(KeyCodes.ONE, Modifiers.SHIFT);

    // press the letter x
    fire(KeyCodes.X);

    listener.$verify();
  },

  testCanRemoveTwoHandlers() {
    listener.$replay();  // no events expected

    handler.registerShortcut(
        'quitemacs', [KeyCodes.X, Modifiers.CTRL, KeyCodes.C]);
    handler.registerShortcut('letterex', 'x');
    handler.unregisterShortcut([KeyCodes.X, Modifiers.CTRL, KeyCodes.C]);
    handler.unregisterShortcut('x');

    fire(KeyCodes.X, {ctrlKey: true});
    fire(KeyCodes.C);
    fire(KeyCodes.X);

    listener.$verify();
  },

  testIsShortcutRegistered_single() {
    assertFalse(handler.isShortcutRegistered('x'));
    handler.registerShortcut('letterex', 'x');
    assertTrue(handler.isShortcutRegistered('x'));
    handler.unregisterShortcut('x');
    assertFalse(handler.isShortcutRegistered('x'));
  },

  testIsShortcutRegistered_multi() {
    assertFalse(handler.isShortcutRegistered('a'));
    assertFalse(handler.isShortcutRegistered('a b'));
    assertFalse(handler.isShortcutRegistered('a b c'));

    handler.registerShortcut('ab', 'a b');

    assertFalse(handler.isShortcutRegistered('a'));
    assertTrue(handler.isShortcutRegistered('a b'));
    assertFalse(handler.isShortcutRegistered('a b c'));

    handler.unregisterShortcut('a b');

    assertFalse(handler.isShortcutRegistered('a'));
    assertFalse(handler.isShortcutRegistered('a b'));
    assertFalse(handler.isShortcutRegistered('a b c'));
  },

  testRegisterShortcutThrowsIfShortcutsConflict() {
    handler.registerShortcut('ab', 'a b');
    assertThrows(
        'Registering a shortcut that triggers a pre-existing shortcut when' +
            'its sequence is typed out should throw',
        () => handler.registerShortcut('abc', 'a b c'));
    assertTrue(handler.isShortcutRegistered('a b'));
    assertFalse(handler.isShortcutRegistered('a b c'));
    // Check that the error message displays the name of the existing shortcut.
    try {
      handler.registerShortcut('abc', 'a b c');
    } catch (e) {
      assertEquals(
          'Keyboard shortcut conflicts with existing shortcut: ab', e.message);
    }
  },

  testUnregister_subsequence() {
    // Unregistering a partial sequence should not orphan shortcuts further in
    // the sequence.
    handler.registerShortcut('abc', 'a b c');
    handler.unregisterShortcut('a b');
    assertTrue(handler.isShortcutRegistered('a b c'));
  },

  testUnregister_supersequence() {
    // Unregistering a sequence that extends beyond a registered sequence should
    // do nothing.
    handler.registerShortcut('ab', 'a b');
    handler.unregisterShortcut('a b c');
    assertTrue(handler.isShortcutRegistered('a b'));
  },

  testUnregister_partialMatchSequence() {
    // Unregistering a sequence that partially matches a registered sequence
    // should do nothing.
    handler.registerShortcut('abc', 'a b c');
    handler.unregisterShortcut('a b x');
    assertTrue(handler.isShortcutRegistered('a b c'));
  },

  testUnregister_deadBranch() {
    // Unregistering a sequence should prune any dead branches in the tree.
    handler.registerShortcut('abc', 'a b c');
    handler.unregisterShortcut('a b c');
    // Default is not should not be prevented in the A key stroke because the A
    // branch has been removed from the tree.
    assertTrue(fire(KeyCodes.A));
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testIgnoreNonGlobalShortcutsInSelect() {
    const targetSelect = dom.getElement('targetSelect');

    listener.shortcutFired('global');
    listener.shortcutFired('withAlt');
    listener.$replay();

    registerEnterSpaceXF1AltY();
    fireEnterSpaceXF1AltY(dom.getElement('targetSelect'));

    listener.$verify();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testIgnoreNonGlobalShortcutsInTextArea() {
    listener.shortcutFired('global');
    listener.shortcutFired('withAlt');
    listener.$replay();

    registerEnterSpaceXF1AltY();
    fireEnterSpaceXF1AltY(dom.getElement('targetTextArea'));

    listener.$verify();
  },

  testIgnoreShortcutsExceptEnterInTextInputFields() {
    const targets = [
      'targetColor',
      'targetDate',
      'targetDateTime',
      'targetDateTimeLocal',
      'targetEmail',
      'targetMonth',
      'targetNumber',
      'targetPassword',
      'targetSearch',
      'targetTel',
      'targetText',
      'targetTime',
      'targetUrl',
      'targetWeek',
    ];
    registerEnterSpaceXF1AltY();
    expectShortcutsOnTargets(
        ['enter', 'global', 'withAlt'], targets, fireEnterSpaceXF1AltY);
  },

  /**
     @suppress {strictMissingProperties,missingProperties} suppression added to
     enable type checking
   */
  testIgnoreShortcutsInShadowTextInputFields() {
    const shadowDiv = dom.getElement('targetShadow');
    // skip if shadow dom is not supported
    if (!shadowDiv.attachShadow) {
      return;
    }
    const shadowInput = dom.createElement('input');
    shadowDiv.attachShadow({mode: 'open'});
    shadowDiv.shadowRoot.appendChild(shadowInput);

    registerEnterSpaceXF1AltY();
    listener.shortcutFired('enter');
    listener.shortcutFired('global');
    listener.shortcutFired('withAlt');
    listener.$replay();

    const shadowProps = {
      composed: true,
      composedPath: function() {
        return [shadowInput];
      },
    };

    fireEnterSpaceXF1AltY(shadowDiv, shadowProps);

    listener.$verify();
  },

  testIgnoreSpaceInCheckBoxAndButton() {
    registerEnterSpaceXF1AltY();
    expectShortcutsOnTargets(
        ['enter', 'x', 'global', 'withAlt'], ['targetCheckBox', 'targetButton'],
        fireEnterSpaceXF1AltY);
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testIgnoreNonGlobalShortcutsInContentEditable() {
    // Don't set design mode in later IE as javascripts don't run when in
    // that mode.
    const setDesignMode = !userAgent.IE;
    try {
      if (setDesignMode) {
        document.designMode = 'on';
      }
      targetDiv.contentEditable = 'true';

      // Expect only global shortcuts.
      listener.shortcutFired('global');
      listener.$replay();

      registerEnterSpaceXF1AltY();
      fireEnterSpaceXF1AltY(targetDiv);

      listener.$verify();
    } finally {
      if (setDesignMode) {
        document.designMode = 'off';
      }
      targetDiv.contentEditable = 'false';
    }
  },

  testSetAllShortcutsAreGlobal() {
    handler.setAllShortcutsAreGlobal(true);
    registerEnterSpaceXF1AltY();

    expectShortcutsOnTargets(
        ['enter', 'space', 'x', 'global', 'withAlt'], ['targetTextArea'],
        fireEnterSpaceXF1AltY);
  },

  testSetModifierShortcutsAreGlobalFalse() {
    handler.setModifierShortcutsAreGlobal(false);
    registerEnterSpaceXF1AltY();

    expectShortcutsOnTargets(
        ['global'], ['targetTextArea'], fireEnterSpaceXF1AltY);
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testAltGraphKeyOnUSLayout() {
    // Windows does not assign printable characters to any ctrl+alt keys of
    // the US layout. This test verifies we fire shortcut events when typing
    // ctrl+alt keys on the US layout.
    listener.shortcutFired('letterOne');
    listener.shortcutFired('letterTwo');
    listener.shortcutFired('letterThree');
    listener.shortcutFired('letterFour');
    listener.shortcutFired('letterFive');
    if (userAgent.WINDOWS) {
      listener.$replay();

      handler.registerShortcut('letterOne', 'ctrl+alt+1');
      handler.registerShortcut('letterTwo', 'ctrl+alt+2');
      handler.registerShortcut('letterThree', 'ctrl+alt+3');
      handler.registerShortcut('letterFour', 'ctrl+alt+4');
      handler.registerShortcut('letterFive', 'ctrl+alt+5');

      // Send key events on the English (United States) layout.
      fireAltGraphKey(KeyCodes.ONE, 0, {ctrlKey: true, altKey: true});
      fireAltGraphKey(KeyCodes.TWO, 0, {ctrlKey: true, altKey: true});
      fireAltGraphKey(KeyCodes.THREE, 0, {ctrlKey: true, altKey: true});
      fireAltGraphKey(KeyCodes.FOUR, 0, {ctrlKey: true, altKey: true});
      fireAltGraphKey(KeyCodes.FIVE, 0, {ctrlKey: true, altKey: true});

      listener.$verify();
    }
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testAltGraphKeyOnFrenchLayout() {
    // Windows assigns printable characters to ctrl+alt+[2-5] keys of the
    // French layout. This test verifies we fire shortcut events only when
    // we type ctrl+alt+1 keys on the French layout.
    listener.shortcutFired('letterOne');
    if (userAgent.WINDOWS) {
      listener.$replay();

      handler.registerShortcut('letterOne', 'ctrl+alt+1');
      handler.registerShortcut('letterTwo', 'ctrl+alt+2');
      handler.registerShortcut('letterThree', 'ctrl+alt+3');
      handler.registerShortcut('letterFour', 'ctrl+alt+4');
      handler.registerShortcut('letterFive', 'ctrl+alt+5');

      // Send key events on the French (France) layout.
      fireAltGraphKey(KeyCodes.ONE, 0, {ctrlKey: true, altKey: true});
      fireAltGraphKey(KeyCodes.TWO, 0x0303, {ctrlKey: true, altKey: true});
      fireAltGraphKey(KeyCodes.THREE, 0x0023, {ctrlKey: true, altKey: true});
      fireAltGraphKey(KeyCodes.FOUR, 0x007b, {ctrlKey: true, altKey: true});
      fireAltGraphKey(KeyCodes.FIVE, 0x205b, {ctrlKey: true, altKey: true});

      listener.$verify();
    }
  },

  testAltGraphKeyOnSpanishLayout() {
    // Windows assigns printable characters to ctrl+alt+[1-5] keys of the
    // Spanish layout. This test verifies we do not fire shortcut events at
    // all when typing ctrl+alt+[1-5] keys on the Spanish layout.
    if (userAgent.WINDOWS) {
      listener.$replay();

      handler.registerShortcut('letterOne', 'ctrl+alt+1');
      handler.registerShortcut('letterTwo', 'ctrl+alt+2');
      handler.registerShortcut('letterThree', 'ctrl+alt+3');
      handler.registerShortcut('letterFour', 'ctrl+alt+4');
      handler.registerShortcut('letterFive', 'ctrl+alt+5');

      // Send key events on the Spanish (Spain) layout.
      fireAltGraphKey(KeyCodes.ONE, 0x007c, {ctrlKey: true, altKey: true});
      fireAltGraphKey(KeyCodes.TWO, 0x0040, {ctrlKey: true, altKey: true});
      fireAltGraphKey(KeyCodes.THREE, 0x0023, {ctrlKey: true, altKey: true});
      fireAltGraphKey(KeyCodes.FOUR, 0x0303, {ctrlKey: true, altKey: true});
      fireAltGraphKey(KeyCodes.FIVE, 0x20ac, {ctrlKey: true, altKey: true});

      listener.$verify();
    }
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testAltGraphKeyOnPolishLayout_withShift() {
    // Windows assigns printable characters to ctrl+alt+shift+A key in polish
    // layout. This test verifies that we do not fire shortcut events for A, but
    // does fire for Q which does not have a printable character.
    if (userAgent.WINDOWS) {
      listener.shortcutFired('letterQ');
      listener.$replay();

      handler.registerShortcut('letterA', 'ctrl+alt+shift+A');
      handler.registerShortcut('letterQ', 'ctrl+alt+shift+Q');

      // Send key events on the Polish (Programmer) layout.
      assertTrue(fireAltGraphKey(
          KeyCodes.A, 0x0104, {ctrlKey: true, altKey: true, shiftKey: true}));
      assertFalse(fireAltGraphKey(
          KeyCodes.Q, 0, {ctrlKey: true, altKey: true, shiftKey: true}));

      listener.$verify();
    }
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testNumpadKeyShortcuts() {
    const testCases = [
      ['letterNumpad0', 'num-0', KeyCodes.NUM_ZERO],
      ['letterNumpad1', 'num-1', KeyCodes.NUM_ONE],
      ['letterNumpad2', 'num-2', KeyCodes.NUM_TWO],
      ['letterNumpad3', 'num-3', KeyCodes.NUM_THREE],
      ['letterNumpad4', 'num-4', KeyCodes.NUM_FOUR],
      ['letterNumpad5', 'num-5', KeyCodes.NUM_FIVE],
      ['letterNumpad6', 'num-6', KeyCodes.NUM_SIX],
      ['letterNumpad7', 'num-7', KeyCodes.NUM_SEVEN],
      ['letterNumpad8', 'num-8', KeyCodes.NUM_EIGHT],
      ['letterNumpad9', 'num-9', KeyCodes.NUM_NINE],
      ['letterNumpadMultiply', 'num-multiply', KeyCodes.NUM_MULTIPLY],
      ['letterNumpadPlus', 'num-plus', KeyCodes.NUM_PLUS],
      ['letterNumpadMinus', 'num-minus', KeyCodes.NUM_MINUS],
      ['letterNumpadPERIOD', 'num-period', KeyCodes.NUM_PERIOD],
      ['letterNumpadDIVISION', 'num-division', KeyCodes.NUM_DIVISION],
    ];
    for (let i = 0; i < testCases.length; ++i) {
      listener.shortcutFired(testCases[i][0]);
    }
    listener.$replay();

    // Register shortcuts for numpad keys and send numpad-key events.
    for (let i = 0; i < testCases.length; ++i) {
      handler.registerShortcut(testCases[i][0], testCases[i][1]);
      fire(testCases[i][2]);
    }
    listener.$verify();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testGeckoShortcuts() {
    listener.shortcutFired('1');
    listener.$replay();

    handler.registerShortcut('1', 'semicolon');

    if (userAgent.GECKO) {
      fire(KeyCodes.FF_SEMICOLON);
    } else {
      fire(KeyCodes.SEMICOLON);
    }

    listener.$verify();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testWindows_multiKeyShortcuts() {
    if (userAgent.WINDOWS) {
      listener.shortcutFired('nextComment');
      listener.$replay();

      handler.registerShortcut('nextComment', 'ctrl+alt+n ctrl+alt+c');
      // We need to specify a keyPressKeyCode of 0 here because on Windows,
      // keystrokes that don't produce printable characters don't cause a
      // keyPress event to fire.
      assertFalse(
          fireAltGraphKey(KeyCodes.N, 0, {ctrlKey: true, altKey: true}));
      assertFalse(
          fireAltGraphKey(KeyCodes.C, 0, {ctrlKey: true, altKey: true}));
      listener.$verify();
    }
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testWindows_multikeyShortcuts_repeatedKeyDoesntInterfere() {
    if (userAgent.WINDOWS) {
      listener.shortcutFired('announceCursorLocation');
      listener.$replay();

      handler.registerShortcut('announceAnchorText', 'ctrl+alt+a ctrl+alt+a');
      handler.registerShortcut(
          'announceCursorLocation', 'ctrl+alt+a ctrl+alt+l');

      // We need to specify a keyPressKeyCode of 0 here because on Windows,
      // keystrokes that don't produce printable characters don't cause a
      // keyPress event to fire.
      assertFalse(
          fireAltGraphKey(KeyCodes.A, 0, {ctrlKey: true, altKey: true}));
      assertFalse(
          fireAltGraphKey(KeyCodes.L, 0, {ctrlKey: true, altKey: true}));
      listener.$verify();
    }
  },

  testWindows_multikeyShortcuts_polishKey() {
    if (userAgent.WINDOWS) {
      listener.$replay();

      handler.registerShortcut(
          'announceCursorLocation', 'ctrl+alt+a ctrl+alt+l');

      // If a Polish key is a subsection of a keyboard shortcut, then
      // the key should still be written.
      assertTrue(
          fireAltGraphKey(KeyCodes.A, 0x0105, {ctrlKey: true, altKey: true}));
      listener.$verify();
    }
  },

  testRegisterShortcut_modifierOnly() {
    assertThrows(
        'Registering a shortcut with just modifiers should fail.',
        goog.bind(handler.registerShortcut, handler, 'name', 'Shift'));
  },

  testParseStringShortcut_unknownKey() {
    assertThrows(
        'Unknown keys should fail.',
        goog.bind(
            KeyboardShortcutHandler.parseStringShortcut, null, 'NotAKey'));
  },

  testParseStringShortcut_resetKeyCode() {
    const strokes = KeyboardShortcutHandler.parseStringShortcut('A Shift');
    assertNull(
        'The second stroke only has a modifier key.', strokes[1].keyCode);
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testOsxGeckoCopyShortcuts() {
    // Ensures that Meta+C still fires a shortcut. In legacy versions of
    // Closure, we had to listen for Meta+C/X/V on keyup instead of keydown due
    // to a bug in Gecko 1.8 on OS X. This is a sanity check to ensure that
    // behavior has not regressed.
    listener.shortcutFired('copy');
    listener.$replay();

    handler.registerShortcut('copy', [KeyCodes.C, Modifiers.META]);
    fire(KeyCodes.C, {metaKey: true});

    listener.$verify();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testHandleEmptyBrowserEvent() {
    const rootDiv = dom.getElement('rootDiv');
    const emptyEvent = new BrowserEvent();
    emptyEvent.type = events.EventType.KEYDOWN;
    emptyEvent.target = rootDiv;
    emptyEvent.key = 'g';
    emptyEvent.keyCode = KeyCodes.G;
    emptyEvent.preventDefault = goog.nullFunction;
    emptyEvent.stopPropagation = goog.nullFunction;

    handler.registerShortcut('lettergee', 'g');

    listener.shortcutFired('lettergee');
    listener.$replay();

    events.fireListeners(rootDiv, emptyEvent.type, false, emptyEvent);

    listener.$verify();
  },
});
