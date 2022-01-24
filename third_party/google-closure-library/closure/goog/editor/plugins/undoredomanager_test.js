/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.editor.plugins.UndoRedoManagerTest');
goog.setTestOnly();

const StrictMock = goog.require('goog.testing.StrictMock');
const UndoRedoManager = goog.require('goog.editor.plugins.UndoRedoManager');
const UndoRedoState = goog.require('goog.editor.plugins.UndoRedoState');
const events = goog.require('goog.events');
const testSuite = goog.require('goog.testing.testSuite');

let mockState1;
let mockState2;
let mockState3;
let states;
let manager;
let stateChangeCount;
let beforeUndoCount;
let beforeRedoCount;
let preventDefault;

/** Adds all the mock states to the undo-redo manager. */
function addStatesToManager() {
  manager.addState(states[0]);

  for (let i = 1; i < states.length; i++) {
    const state = states[i];
    manager.addState(state);
  }

  stateChangeCount = 0;
}

/** Resets all mock states so that they are ready for testing. */
function resetStates() {
  for (let i = 0; i < states.length; i++) {
    states[i].$reset();
  }
}

testSuite({
  setUp() {
    manager = new UndoRedoManager();
    stateChangeCount = 0;
    events.listen(manager, UndoRedoManager.EventType.STATE_CHANGE, () => {
      stateChangeCount++;
    });

    beforeUndoCount = 0;
    preventDefault = false;
    events.listen(manager, UndoRedoManager.EventType.BEFORE_UNDO, (e) => {
      beforeUndoCount++;
      if (preventDefault) {
        e.preventDefault();
      }
    });

    beforeRedoCount = 0;
    events.listen(manager, UndoRedoManager.EventType.BEFORE_REDO, (e) => {
      beforeRedoCount++;
      if (preventDefault) {
        e.preventDefault();
      }
    });

    mockState1 = new StrictMock(UndoRedoState);
    mockState2 = new StrictMock(UndoRedoState);
    mockState3 = new StrictMock(UndoRedoState);
    states = [mockState1, mockState2, mockState3];

    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    mockState1.equals = mockState2.equals =
        mockState3.equals = function(state) {
          return this == state;
        };

    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    mockState1.isAsynchronous = mockState2.isAsynchronous =
        mockState3.isAsynchronous = () => false;
  },

  tearDown() {
    events.removeAll(manager);
    manager.dispose();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSetMaxUndoDepth() {
    manager.setMaxUndoDepth(2);
    addStatesToManager();
    assertArrayEquals(
        'Undo stack must contain only the two most recent states.',
        [mockState2, mockState3], manager.undoStack_);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testAddState() {
    let stateAddedCount = 0;
    events.listen(manager, UndoRedoManager.EventType.STATE_ADDED, () => {
      stateAddedCount++;
    });

    manager.addState(mockState1);
    assertArrayEquals(
        'Undo stack must contain added state.', [mockState1],
        manager.undoStack_);
    assertEquals(
        'Manager must dispatch one state change event on ' +
            'undo stack 0->1 transition.',
        1, stateChangeCount);
    assertEquals('State added must have dispatched once.', 1, stateAddedCount);
    mockState1.$reset();

    // Test adding same state twice.
    manager.addState(mockState1);
    assertArrayEquals(
        'Undo stack must not contain two equal, sequential states.',
        [mockState1], manager.undoStack_);
    assertEquals(
        'Manager must not dispatch state change event when nothing is ' +
            'added to the stack.',
        1, stateChangeCount);
    assertEquals('State added must have dispatched once.', 1, stateAddedCount);

    // Test adding a second state.
    manager.addState(mockState2);
    assertArrayEquals(
        'Undo stack must contain both states.', [mockState1, mockState2],
        manager.undoStack_);
    assertEquals(
        'Manager must not dispatch state change event when second ' +
            'state is added to the stack.',
        1, stateChangeCount);
    assertEquals('State added must have dispatched twice.', 2, stateAddedCount);

    // Test adding a state when there is state on the redo stack.
    manager.undo();
    assertEquals(
        'Manager must dispatch state change when redo stack goes to 1.', 2,
        stateChangeCount);

    manager.addState(mockState3);
    assertArrayEquals(
        'Undo stack must contain states 1 and 3.', [mockState1, mockState3],
        manager.undoStack_);
    assertEquals(
        'Manager must dispatch state change event when redo stack ' +
            'goes to zero.',
        3, stateChangeCount);
    assertEquals(
        'State added must have dispatched three times.', 3, stateAddedCount);
  },

  testHasState() {
    assertFalse('New manager must have no undo state.', manager.hasUndoState());
    assertFalse('New manager must have no redo state.', manager.hasRedoState());

    manager.addState(mockState1);
    assertTrue('Manager must have only undo state.', manager.hasUndoState());
    assertFalse('Manager must have no redo state.', manager.hasRedoState());

    manager.undo();
    assertFalse('Manager must have no undo state.', manager.hasUndoState());
    assertTrue('Manager must have only redo state.', manager.hasRedoState());
  },

  testClearHistory() {
    addStatesToManager();
    manager.undo();
    stateChangeCount = 0;

    manager.clearHistory();
    assertFalse('Undo stack must be empty.', manager.hasUndoState());
    assertFalse('Redo stack must be empty.', manager.hasRedoState());
    assertEquals(
        'State change count must be 1 after clear history.', 1,
        stateChangeCount);

    manager.clearHistory();
    assertEquals(
        'Repeated clearHistory must not change state change count.', 1,
        stateChangeCount);
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testUndo() {
    addStatesToManager();

    mockState3.undo();
    mockState3.$replay();
    manager.undo();
    assertEquals(
        'Adding first item to redo stack must dispatch state change.', 1,
        stateChangeCount);
    assertEquals(
        'Undo must cause before action to dispatch', 1, beforeUndoCount);
    mockState3.$verify();

    preventDefault = true;
    mockState2.$replay();
    manager.undo();
    assertEquals(
        'No stack transitions between 0 and 1, must not dispatch ' +
            'state change.',
        1, stateChangeCount);
    assertEquals(
        'Undo must cause before action to dispatch', 2, beforeUndoCount);
    mockState2.$verify();  // Verify that undo was prevented.

    preventDefault = false;
    mockState1.undo();
    mockState1.$replay();
    manager.undo();
    assertEquals(
        'Doing last undo operation must dispatch state change.', 2,
        stateChangeCount);
    assertEquals(
        'Undo must cause before action to dispatch', 3, beforeUndoCount);
    mockState1.$verify();
  },

  /**
     @suppress {missingProperties,checkTypes} suppression added to enable type
     checking
   */
  testUndo_Asynchronous() {
    // Using a stub instead of a mock here so that the state can behave as an
    // EventTarget and dispatch events.
    const stubState = new UndoRedoState(true);
    let undoCalled = false;
    stubState.undo = () => {
      undoCalled = true;
    };
    stubState.redo = goog.nullFunction;
    stubState.equals = () => false;

    manager.addState(mockState2);
    manager.addState(mockState1);
    manager.addState(stubState);

    manager.undo();
    assertTrue('undoCalled must be true (undo must be called).', undoCalled);
    assertEquals(
        'Undo must cause before action to dispatch', 1, beforeUndoCount);

    // Calling undo shouldn't actually undo since the first async undo hasn't
    // fired an event yet.
    mockState1.$replay();
    manager.undo();
    mockState1.$verify();
    assertEquals(
        'Before action must not dispatch for pending undo.', 1,
        beforeUndoCount);

    // Dispatching undo completed on first undo, should cause the second pending
    // undo to happen.
    mockState1.$reset();
    mockState1.undo();
    mockState1.$replay();
    mockState2.$replay();  // Nothing should happen to mockState2.
    stubState.dispatchEvent(UndoRedoState.ACTION_COMPLETED);
    mockState1.$verify();
    mockState2.$verify();
    assertEquals(
        'Second undo must cause before action to dispatch', 2, beforeUndoCount);

    // Test last undo.
    mockState2.$reset();
    mockState2.undo();
    mockState2.$replay();
    manager.undo();
    mockState2.$verify();
    assertEquals(
        'Third undo must cause before action to dispatch', 3, beforeUndoCount);
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testRedo() {
    addStatesToManager();
    manager.undo();
    manager.undo();
    manager.undo();
    resetStates();
    stateChangeCount = 0;

    mockState1.redo();
    mockState1.$replay();
    manager.redo();
    assertEquals(
        'Pushing first item onto undo stack during redo must dispatch ' +
            'state change.',
        1, stateChangeCount);
    assertEquals(
        'First redo must cause before action to dispatch', 1, beforeRedoCount);
    mockState1.$verify();

    preventDefault = true;
    mockState2.$replay();
    manager.redo();
    assertEquals(
        'No stack transitions between 0 and 1, must not dispatch ' +
            'state change.',
        1, stateChangeCount);
    assertEquals(
        'Second redo must cause before action to dispatch', 2, beforeRedoCount);
    mockState2.$verify();  // Verify that redo was prevented.

    preventDefault = false;
    mockState3.redo();
    mockState3.$replay();
    manager.redo();
    assertEquals(
        'Removing last item from redo stack must dispatch state change.', 2,
        stateChangeCount);
    assertEquals(
        'Third redo must cause before action to dispatch', 3, beforeRedoCount);
    mockState3.$verify();
    mockState3.$reset();

    mockState3.undo();
    mockState3.$replay();
    manager.undo();
    assertEquals(
        'Putting item on redo stack must dispatch state change.', 3,
        stateChangeCount);
    assertEquals(
        'Undo must cause before action to dispatch', 4, beforeUndoCount);
    mockState3.$verify();
  },

  /**
     @suppress {checkTypes,missingProperties} suppression added to enable type
     checking
   */
  testRedo_Asynchronous() {
    const stubState = new UndoRedoState(true);
    let redoCalled = false;
    stubState.redo = () => {
      redoCalled = true;
    };
    stubState.undo = goog.nullFunction;
    stubState.equals = () => false;

    manager.addState(stubState);
    manager.addState(mockState1);
    manager.addState(mockState2);

    manager.undo();
    manager.undo();
    manager.undo();
    stubState.dispatchEvent(UndoRedoState.ACTION_COMPLETED);
    resetStates();

    manager.redo();
    assertTrue('redoCalled must be true (redo must be called).', redoCalled);

    // Calling redo shouldn't actually redo since the first async redo hasn't
    // fired an event yet.
    mockState1.$replay();
    manager.redo();
    mockState1.$verify();

    // Dispatching redo completed on first redo, should cause the second pending
    // redo to happen.
    mockState1.$reset();
    mockState1.redo();
    mockState1.$replay();
    mockState2.$replay();  // Nothing should happen to mockState1.
    stubState.dispatchEvent(UndoRedoState.ACTION_COMPLETED);
    mockState1.$verify();
    mockState2.$verify();

    // Test last redo.
    mockState2.$reset();
    mockState2.redo();
    mockState2.$replay();
    manager.redo();
    mockState2.$verify();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testUndoAndRedoPeek() {
    addStatesToManager();
    manager.undo();

    assertEquals(
        'redoPeek must return the top of the redo stack.',
        manager.redoStack_[manager.redoStack_.length - 1], manager.redoPeek());
    assertEquals(
        'undoPeek must return the top of the undo stack.',
        manager.undoStack_[manager.undoStack_.length - 1], manager.undoPeek());
  },
});
