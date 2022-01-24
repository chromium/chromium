/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.editor.plugins.UndoRedoTest');
goog.setTestOnly();

const Field = goog.require('goog.editor.Field');
const LoremIpsum = goog.require('goog.editor.plugins.LoremIpsum');
const MockClock = goog.require('goog.testing.MockClock');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const SafeHtml = goog.require('goog.html.SafeHtml');
const StrictMock = goog.require('goog.testing.StrictMock');
const UndoRedo = goog.require('goog.editor.plugins.UndoRedo');
const browserrange = goog.require('goog.dom.browserrange');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const functions = goog.require('goog.functions');
const googArray = goog.require('goog.array');
const testSuite = goog.require('goog.testing.testSuite');

let mockEditableField;
let editableField;
let fieldHashCode;
let undoPlugin;
let state;
let mockState;
let commands;
let clock;
const stubs = new PropertyReplacer();

// undo-redo plugin tests

/**
 * Returns the CursorPosition for the selection currently in the Field.
 * @return {UndoRedo.CursorPosition_}
 * @suppress {visibility} suppression added to enable type checking
 */
function getCurrentCursorPosition() {
  return undoPlugin.getCursorPosition_(editableField);
}

/**
 * Compares two cursor positions and returns whether they are equal.
 * @param {UndoRedo.CursorPosition_} a A cursor position.
 * @param {UndoRedo.CursorPosition_} b A cursor position.
 * @return {boolean} Whether the positions are equal.
 */
function cursorPositionsEqual(a, b) {
  if (!a && !b) {
    return true;
  } else if (a && b) {
    return a.toString() == b.toString();
  }
  // Only one cursor position is an object, can't be equal.
  return false;
}
// Undo state tests

testSuite({
  /** @suppress {checkTypes} suppression added to enable type checking */
  setUp() {
    mockEditableField = new StrictMock(Field);

    // Update the arg list verifier for dispatchCommandValueChange to
    // correctly compare arguments that are arrays (or other complex objects).
    mockEditableField.$registerArgumentListVerifier(
        'dispatchEvent',
        (expected, args) => googArray.equals(expected, args, (a, b) => {
          assertObjectEquals(a, b);
          return true;
        }));
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    mockEditableField.getHashCode = () => 'fieldId';

    undoPlugin = new UndoRedo();
    undoPlugin.registerFieldObject(mockEditableField);
    mockState = new StrictMock(UndoRedo.UndoState_);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    mockState.fieldHashCode = 'fieldId';
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    mockState.isAsynchronous = () => false;
    // Don't bother mocking the inherited event target pieces of the state.
    // If we don't do this, then mocked asynchronous undos are a lot harder and
    // that behavior is tested as part of the UndoRedoManager tests.
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    mockState.addEventListener = goog.nullFunction;

    commands = [
      UndoRedo.COMMAND.REDO,
      UndoRedo.COMMAND.UNDO,
    ];
    /** @suppress {visibility} suppression added to enable type checking */
    state = new UndoRedo.UndoState_('1', '', null, goog.nullFunction);

    clock = new MockClock(true);

    editableField = new Field('testField');
    fieldHashCode = editableField.getHashCode();
  },

  /** @suppress {uselessCode} suppression added to enable type checking */
  tearDown() {
    // Reset field so any attempted access during disposes don't cause errors.
    mockEditableField.$reset();
    clock.dispose();
    undoPlugin.dispose();

    // NOTE(nicksantos): I think IE is blowing up on this call because
    // it is lame. It manifests its lameness by throwing an exception.
    // Kudos to XT for helping me to figure this out.
    try {
    } catch (e) {
    }

    if (!editableField.isUneditable()) {
      editableField.makeUneditable();
    }
    editableField.dispose();
    dom.removeChildren(dom.getElement('testField'));
    stubs.reset();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testQueryCommandValue() {
    assertFalse(
        'Must return false for empty undo stack.',
        undoPlugin.queryCommandValue(UndoRedo.COMMAND.UNDO));

    assertFalse(
        'Must return false for empty redo stack.',
        undoPlugin.queryCommandValue(UndoRedo.COMMAND.REDO));

    undoPlugin.undoManager_.addState(mockState);

    assertTrue(
        'Must return true for a non-empty undo stack.',
        undoPlugin.queryCommandValue(UndoRedo.COMMAND.UNDO));
  },

  /**
     @suppress {visibility,missingProperties} suppression added to enable type
     checking
   */
  testExecCommand() {
    undoPlugin.undoManager_.addState(mockState);

    mockState.undo();
    mockState.$replay();

    undoPlugin.execCommand(UndoRedo.COMMAND.UNDO);
    // Second undo should do nothing since only one item on stack.
    undoPlugin.execCommand(UndoRedo.COMMAND.UNDO);
    mockState.$verify();

    mockState.$reset();
    mockState.redo();
    mockState.$replay();
    undoPlugin.execCommand(UndoRedo.COMMAND.REDO);
    // Second redo should do nothing since only one item on stack.
    undoPlugin.execCommand(UndoRedo.COMMAND.REDO);
    mockState.$verify();
  },

  /**
     @suppress {visibility,missingProperties} suppression added to enable type
     checking
   */
  testHandleKeyboardShortcut_TrogStates() {
    undoPlugin.undoManager_.addState(mockState);
    undoPlugin.undoManager_.addState(state);
    undoPlugin.undoManager_.undo();
    mockEditableField.$reset();

    const stubUndoEvent = {ctrlKey: true, altKey: false, shiftKey: false};
    const stubRedoEvent = {ctrlKey: true, altKey: false, shiftKey: true};
    const stubRedoEvent2 = {ctrlKey: true, altKey: false, shiftKey: false};
    let result;

    // Test handling Trogedit undos. Should always call EditableField's
    // execCommand. Since EditableField is mocked, this will not result in a
    // call to the mockState's undo and redo methods.
    mockEditableField.execCommand(UndoRedo.COMMAND.UNDO);
    mockEditableField.$replay();
    result = undoPlugin.handleKeyboardShortcut(stubUndoEvent, 'z', true);
    assertTrue('Plugin must return true when it handles shortcut.', result);
    mockEditableField.$verify();
    mockEditableField.$reset();

    mockEditableField.execCommand(UndoRedo.COMMAND.REDO);
    mockEditableField.$replay();
    result = undoPlugin.handleKeyboardShortcut(stubRedoEvent, 'z', true);
    assertTrue('Plugin must return true when it handles shortcut.', result);
    mockEditableField.$verify();
    mockEditableField.$reset();

    mockEditableField.execCommand(UndoRedo.COMMAND.REDO);
    mockEditableField.$replay();
    result = undoPlugin.handleKeyboardShortcut(stubRedoEvent2, 'y', true);
    assertTrue('Plugin must return true when it handles shortcut.', result);
    mockEditableField.$verify();
    mockEditableField.$reset();

    mockEditableField.$replay();
    result = undoPlugin.handleKeyboardShortcut(stubRedoEvent2, 'y', false);
    assertFalse(
        'Plugin must return false when modifier is not pressed.', result);
    mockEditableField.$verify();
    mockEditableField.$reset();

    mockEditableField.$replay();
    result = undoPlugin.handleKeyboardShortcut(stubUndoEvent, 'f', true);
    assertFalse(
        'Plugin must return false when it doesn\'t handle shortcut.', result);
    mockEditableField.$verify();
  },

  /**
     @suppress {visibility,missingProperties} suppression added to enable type
     checking
   */
  testHandleKeyboardShortcut_NotTrogStates() {
    const stubUndoEvent = {ctrlKey: true, altKey: false, shiftKey: false};

    // Trogedit undo states all have a fieldHashCode, nulling that out makes
    // this state be treated as a non-Trogedit undo-redo state.
    /** @suppress {checkTypes} suppression added to enable type checking */
    state.fieldHashCode = null;
    undoPlugin.undoManager_.addState(state);
    mockEditableField.$reset();

    // Non-trog state shouldn't go through EditableField.execCommand, however,
    // we still exect command value change dispatch since undo-redo plugin
    // redispatches those anytime manager's state changes.
    mockEditableField.dispatchEvent({
      type: Field.EventType.COMMAND_VALUE_CHANGE,
      commands: commands,
    });
    mockEditableField.$replay();
    const result = undoPlugin.handleKeyboardShortcut(stubUndoEvent, 'z', true);
    assertTrue('Plugin must return true when it handles shortcut.', result);
    mockEditableField.$verify();
  },

  testEnable() {
    assertFalse(
        'Plugin must start disabled.', undoPlugin.isEnabled(editableField));

    editableField.makeEditable();
    editableField.setSafeHtml(false, SafeHtml.create('div', {}, 'a'));
    undoPlugin.enable(editableField);

    assertTrue(undoPlugin.isEnabled(editableField));
    assertNotNull(
        'Must have an event handler for enabled field.',
        undoPlugin.eventHandlers_[fieldHashCode]);

    const currentState = undoPlugin.currentStates_[fieldHashCode];
    assertNotNull('Enabled plugin must have a current state.', currentState);
    assertEquals(
        'After enable, undo content must match the field content.',
        editableField.getElement().innerHTML, currentState.undoContent_);

    assertTrue(
        'After enable, undo cursorPosition must match the field cursor' +
            'position.',
        cursorPositionsEqual(
            getCurrentCursorPosition(), currentState.undoCursorPosition_));

    assertUndefined(
        'Current state must never have redo content.',
        currentState.redoContent_);
    assertUndefined(
        'Current state must never have redo cursor position.',
        currentState.redoCursorPosition_);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testDisable() {
    editableField.makeEditable();
    undoPlugin.enable(editableField);
    assertTrue(
        'Plugin must be enabled so we can test disabling.',
        undoPlugin.isEnabled(editableField));

    let delayedChangeFired = false;
    events.listenOnce(editableField, Field.EventType.DELAYEDCHANGE, (e) => {
      delayedChangeFired = true;
    });
    editableField.setSafeHtml(false, SafeHtml.htmlEscape('foo'));

    undoPlugin.disable(editableField);
    assertTrue(
        'disable must fire pending delayed changes.', delayedChangeFired);
    assertEquals(
        'disable must add undo state from pending change.', 1,
        undoPlugin.undoManager_.undoStack_.length);

    assertFalse(undoPlugin.isEnabled(editableField));
    assertUndefined(
        'Disabled plugin must not have current state.',
        undoPlugin.eventHandlers_[fieldHashCode]);
    assertUndefined(
        'Disabled plugin must not have event handlers.',
        undoPlugin.eventHandlers_[fieldHashCode]);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testUpdateCurrentState_() {
    editableField.registerPlugin(new LoremIpsum('LOREM'));
    editableField.makeEditable();
    editableField.getPluginByClassId('LoremIpsum').usingLorem_ = true;
    undoPlugin.updateCurrentState_(editableField);
    let currentState = undoPlugin.currentStates_[fieldHashCode];
    assertNotUndefined(
        'Must create empty states for field using lorem ipsum.',
        undoPlugin.currentStates_[fieldHashCode]);
    assertEquals('', currentState.undoContent_);
    assertNull(currentState.undoCursorPosition_);

    editableField.getPluginByClassId('LoremIpsum').usingLorem_ = false;

    // Pretend foo is the default contents to test '' == default contents
    // behavior.
    editableField.getInjectableContents = (contents, styles) =>
        contents == '' ? 'foo' : contents;
    editableField.setSafeHtml(false, SafeHtml.htmlEscape('foo'));
    undoPlugin.updateCurrentState_(editableField);
    assertEquals(currentState, undoPlugin.currentStates_[fieldHashCode]);

    // NOTE(user): Because there is already a current state, this setSafeHtml
    // will add a state to the undo stack.
    editableField.setSafeHtml(false, SafeHtml.create('div', {}, 'a'));
    // Select some text so we have a valid selection that gets saved in the
    // UndoState.
    browserrange.createRangeFromNodeContents(editableField.getElement())
        .select();

    undoPlugin.updateCurrentState_(editableField);
    currentState = undoPlugin.currentStates_[fieldHashCode];
    assertNotNull(
        'Must create state for field not using lorem ipsum', currentState);
    assertEquals(fieldHashCode, currentState.fieldHashCode);
    const content = editableField.getElement().innerHTML;
    const cursorPosition = getCurrentCursorPosition();
    assertEquals(content, currentState.undoContent_);
    assertTrue(
        cursorPositionsEqual(cursorPosition, currentState.undoCursorPosition_));
    assertUndefined(currentState.redoContent_);
    assertUndefined(currentState.redoCursorPosition_);

    undoPlugin.updateCurrentState_(editableField);
    assertEquals(
        'Updating state when state has not changed must not add undo ' +
            'state to stack.',
        1, undoPlugin.undoManager_.undoStack_.length);
    assertEquals(
        'Updating state when state has not changed must not create ' +
            'a new state.',
        currentState, undoPlugin.currentStates_[fieldHashCode]);
    assertUndefined(
        'Updating state when state has not changed must not add ' +
            'redo content.',
        currentState.redoContent_);
    assertUndefined(
        'Updating state when state has not changed must not add ' +
            'redo cursor position.',
        currentState.redoCursorPosition_);

    editableField.setSafeHtml(false, SafeHtml.create('div', {}, 'b'));
    undoPlugin.updateCurrentState_(editableField);
    currentState = undoPlugin.currentStates_[fieldHashCode];
    assertNotNull(
        'Must create state for field not using lorem ipsum', currentState);
    assertEquals(fieldHashCode, currentState.fieldHashCode);
    const newContent = editableField.getElement().innerHTML;
    const newCursorPosition = getCurrentCursorPosition();
    assertEquals(newContent, currentState.undoContent_);
    assertTrue(cursorPositionsEqual(
        newCursorPosition, currentState.undoCursorPosition_));
    assertUndefined(currentState.redoContent_);
    assertUndefined(currentState.redoCursorPosition_);

    /** @suppress {visibility} suppression added to enable type checking */
    const undoState = googArray.peek(undoPlugin.undoManager_.undoStack_);
    assertNotNull(
        'Must create state for field not using lorem ipsum', currentState);
    assertEquals(fieldHashCode, currentState.fieldHashCode);
    assertEquals(content, undoState.undoContent_);
    assertTrue(
        cursorPositionsEqual(cursorPosition, undoState.undoCursorPosition_));
    assertEquals(newContent, undoState.redoContent_);
    assertTrue(
        cursorPositionsEqual(newCursorPosition, undoState.redoCursorPosition_));
  },

  /**
   * Tests that change events get restarted properly after an undo call despite
   * an exception being thrown in the process (see bug/1991234).
   * @suppress {visibility} suppression added to enable type checking
   */
  testUndoRestartsChangeEvents() {
    undoPlugin.registerFieldObject(editableField);
    editableField.makeEditable();
    editableField.setSafeHtml(false, SafeHtml.create('div', {}, 'a'));
    clock.tick(1000);
    undoPlugin.enable(editableField);

    // Change content so we can undo it.
    editableField.setSafeHtml(false, SafeHtml.create('div', {}, 'b'));
    clock.tick(1000);

    const currentState = undoPlugin.currentStates_[fieldHashCode];
    stubs.set(
        editableField, 'setCursorPosition',
        functions.error('Faking exception during setCursorPosition()'));
    try {
      currentState.undo();
    } catch (e) {
      fail('Exception should not have been thrown during undo()');
    }
    assertEquals(
        'Change events should be on', 0,
        editableField.stoppedEvents_[Field.EventType.CHANGE]);
    assertEquals(
        'Delayed change events should be on', 0,
        editableField.stoppedEvents_[Field.EventType.DELAYEDCHANGE]);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testRefreshCurrentState() {
    editableField.makeEditable();
    editableField.setSafeHtml(false, SafeHtml.create('div', {}, 'a'));
    clock.tick(1000);
    undoPlugin.enable(editableField);

    // Create current state and verify it.
    let currentState = undoPlugin.currentStates_[fieldHashCode];
    assertEquals(fieldHashCode, currentState.fieldHashCode);
    let content = editableField.getElement().innerHTML;
    let cursorPosition = getCurrentCursorPosition();
    assertEquals(content, currentState.undoContent_);
    assertTrue(
        cursorPositionsEqual(cursorPosition, currentState.undoCursorPosition_));

    // Update the field w/o dispatching delayed change, and verify that the
    // current state hasn't changed to reflect new values.
    editableField.setSafeHtml(false, SafeHtml.create('div', {}, 'b'), true);
    clock.tick(1000);
    currentState = undoPlugin.currentStates_[fieldHashCode];
    assertEquals(
        'Content must match old state.', content, currentState.undoContent_);
    assertTrue(
        'Cursor position must match old state.',
        cursorPositionsEqual(cursorPosition, currentState.undoCursorPosition_));

    undoPlugin.refreshCurrentState(editableField);
    assertFalse(
        'Refresh must not cause states to go on the undo-redo stack.',
        undoPlugin.undoManager_.hasUndoState());
    currentState = undoPlugin.currentStates_[fieldHashCode];
    content = editableField.getElement().innerHTML;
    cursorPosition = getCurrentCursorPosition();
    assertEquals(
        'Content must match current field state.', content,
        currentState.undoContent_);
    assertTrue(
        'Cursor position must match current field state.',
        cursorPositionsEqual(cursorPosition, currentState.undoCursorPosition_));

    undoPlugin.disable(editableField);
    assertUndefined(undoPlugin.currentStates_[fieldHashCode]);
    undoPlugin.refreshCurrentState(editableField);
    assertUndefined(
        'Must not refresh current state of fields that do not have ' +
            'undo-redo enabled.',
        undoPlugin.currentStates_[fieldHashCode]);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSetUndoState() {
    state.setUndoState('content', 'position');
    assertEquals('Undo content incorrectly set', 'content', state.undoContent_);
    assertEquals(
        'Undo cursor position incorrectly set', 'position',
        state.undoCursorPosition_);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSetRedoState() {
    state.setRedoState('content', 'position');
    assertEquals('Redo content incorrectly set', 'content', state.redoContent_);
    assertEquals(
        'Redo cursor position incorrectly set', 'position',
        state.redoCursorPosition_);
  },

  testEquals() {
    assertTrue('A state must equal itself', state.equals(state));

    /**
     * @suppress {checkTypes,visibility} suppression added to enable type
     * checking
     */
    let state2 = new UndoRedo.UndoState_('1', '', null);
    assertTrue(
        'A state must equal a state with the same hash code and content.',
        state.equals(state2));

    /**
     * @suppress {checkTypes,visibility} suppression added to enable type
     * checking
     */
    state2 = new UndoRedo.UndoState_('1', '', 'foo');
    assertTrue(
        'States with different cursor positions must be equal',
        state.equals(state2));

    state2.setRedoState('bar', null);
    assertFalse(
        'States with different redo content must not be equal',
        state.equals(state2));

    /**
     * @suppress {checkTypes,visibility} suppression added to enable type
     * checking
     */
    state2 = new UndoRedo.UndoState_('3', '', null);
    assertFalse(
        'States with different field hash codes must not be equal',
        state.equals(state2));

    /**
     * @suppress {checkTypes,visibility} suppression added to enable type
     * checking
     */
    state2 = new UndoRedo.UndoState_('1', 'baz', null);
    assertFalse(
        'States with different undoContent must not be equal',
        state.equals(state2));
  },

  /**
   * @bug 1359214
   * @suppress {visibility} suppression added to enable type
   *      checking
   */
  testClearUndoHistory() {
    const undoRedoPlugin = new UndoRedo();
    editableField.registerPlugin(undoRedoPlugin);
    editableField.makeEditable();

    editableField.dispatchChange();
    clock.tick(10000);

    dom.setTextContent(editableField.getElement(), 'y');
    editableField.dispatchChange();
    assertFalse(undoRedoPlugin.undoManager_.hasUndoState());

    clock.tick(10000);
    assertTrue(undoRedoPlugin.undoManager_.hasUndoState());

    dom.setTextContent(editableField.getElement(), 'z');
    editableField.dispatchChange();

    let numCalls = 0;
    events.listen(editableField, Field.EventType.DELAYEDCHANGE, () => {
      numCalls++;
    });
    undoRedoPlugin.clearHistory();
    // 1 call from stopChangeEvents(). 0 calls from startChangeEvents().
    assertEquals(
        'clearHistory must not cause delayed change when none pending', 1,
        numCalls);

    clock.tick(10000);
    assertFalse(undoRedoPlugin.undoManager_.hasUndoState());
  },
});
