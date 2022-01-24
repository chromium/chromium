/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.editor.plugins.UndoRedoStateTest');
goog.setTestOnly();

const UndoRedoState = goog.require('goog.editor.plugins.UndoRedoState');
const testSuite = goog.require('goog.testing.testSuite');

let asyncState;
let syncState;

testSuite({
  setUp() {
    asyncState = new UndoRedoState(true);
    syncState = new UndoRedoState(false);
  },

  testIsAsynchronous() {
    assertTrue(
        'Must return true for asynchronous state', asyncState.isAsynchronous());
    assertFalse(
        'Must return false for synchronous state', syncState.isAsynchronous());
  },
});
