/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.editor.plugins.AbstractTabHandlerTest');
goog.setTestOnly();

const AbstractTabHandler = goog.require('goog.editor.plugins.AbstractTabHandler');
const BrowserEvent = goog.require('goog.events.BrowserEvent');
const Field = goog.require('goog.editor.Field');
const FieldMock = goog.require('goog.testing.editor.FieldMock');
const KeyCodes = goog.require('goog.events.KeyCodes');
const StrictMock = goog.require('goog.testing.StrictMock');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

let tabHandler;
let editableField;
let handleTabKeyCalled = false;

testSuite({
  /** @suppress {checkTypes} suppression added to enable type checking */
  setUp() {
    editableField = new FieldMock();
    /** @suppress {checkTypes} suppression added to enable type checking */
    editableField.inModalMode = Field.prototype.inModalMode;
    /** @suppress {checkTypes} suppression added to enable type checking */
    editableField.setModalMode = Field.prototype.setModalMode;

    tabHandler = new AbstractTabHandler();
    tabHandler.registerFieldObject(editableField);
    /** @suppress {visibility} suppression added to enable type checking */
    tabHandler.handleTabKey = (e) => {
      handleTabKeyCalled = true;
      return true;
    };
  },

  tearDown() {
    tabHandler.dispose();
  },

  testHandleKey() {
    const event = new StrictMock(BrowserEvent);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    event.keyCode = KeyCodes.TAB;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    event.ctrlKey = false;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    event.metaKey = false;

    assertTrue(
        'Event must be handled when no modifier keys are pressed.',
        tabHandler.handleKeyboardShortcut(event, '', false));
    assertTrue(handleTabKeyCalled);
    handleTabKeyCalled = false;

    editableField.setModalMode(true);
    if (userAgent.GECKO) {
      assertFalse(
          'Event must not be handled when in modal mode',
          tabHandler.handleKeyboardShortcut(event, '', false));
      assertFalse(handleTabKeyCalled);
    } else {
      assertTrue(
          'Event must be handled when in modal mode',
          tabHandler.handleKeyboardShortcut(event, '', false));
      assertTrue(handleTabKeyCalled);
      handleTabKeyCalled = false;
    }

    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    event.ctrlKey = true;
    assertFalse(
        'Plugin must never handle tab key press when ctrlKey is pressed.',
        tabHandler.handleKeyboardShortcut(event, '', false));
    assertFalse(handleTabKeyCalled);

    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    event.ctrlKey = false;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    event.metaKey = true;
    assertFalse(
        'Plugin must never handle tab key press when metaKey is pressed.',
        tabHandler.handleKeyboardShortcut(event, '', false));
    assertFalse(handleTabKeyCalled);
  },
});
