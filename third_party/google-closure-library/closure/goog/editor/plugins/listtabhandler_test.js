/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.editor.plugins.ListTabHandlerTest');
goog.setTestOnly();

const BrowserEvent = goog.require('goog.events.BrowserEvent');
const Command = goog.require('goog.editor.Command');
const FieldMock = goog.require('goog.testing.editor.FieldMock');
const KeyCodes = goog.require('goog.events.KeyCodes');
const ListTabHandler = goog.require('goog.editor.plugins.ListTabHandler');
const StrictMock = goog.require('goog.testing.StrictMock');
const TestHelper = goog.require('goog.testing.editor.TestHelper');
const dom = goog.require('goog.dom');
const functions = goog.require('goog.functions');
const testSuite = goog.require('goog.testing.testSuite');

let field;
let editableField;
let tabHandler;
let testHelper;

testSuite({
  setUpPage() {
    field = dom.getElement('field');
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  setUp() {
    editableField = new FieldMock();
    // Modal mode behavior tested as part of AbstractTabHandler tests.
    editableField.inModalMode = functions.FALSE;

    tabHandler = new ListTabHandler();
    tabHandler.registerFieldObject(editableField);

    testHelper = new TestHelper(field);
    testHelper.setUpEditableElement();
  },

  tearDown() {
    editableField = null;
    testHelper.tearDownEditableElement();
    tabHandler.dispose();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testListIndentInLi() {
    field.innerHTML = '<ul><li>Text</li></ul>';

    const testText = field.firstChild.firstChild.firstChild;  // div ul li Test
    testHelper.select(testText, 0, testText, 4);

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
    event.shiftKey = false;

    editableField.execCommand(Command.INDENT);
    event.preventDefault();

    editableField.$replay();
    event.$replay();

    assertTrue(
        'Event must be handled',
        tabHandler.handleKeyboardShortcut(event, '', false));

    editableField.$verify();
    event.$verify();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testListIndentContainLi() {
    field.innerHTML = '<ul><li>Text</li></ul>';

    const testText = field.firstChild.firstChild.firstChild;  // div ul li Test
    testHelper.select(field.firstChild, 0, testText, 4);

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
    event.shiftKey = false;

    editableField.execCommand(Command.INDENT);
    event.preventDefault();

    editableField.$replay();
    event.$replay();

    assertTrue(
        'Event must be handled',
        tabHandler.handleKeyboardShortcut(event, '', false));

    editableField.$verify();
    event.$verify();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testListOutdentInLi() {
    field.innerHTML = '<ul><li>Text</li></ul>';

    const testText = field.firstChild.firstChild.firstChild;  // div ul li Test
    testHelper.select(testText, 0, testText, 4);

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
    event.shiftKey = true;

    editableField.execCommand(Command.OUTDENT);
    event.preventDefault();

    editableField.$replay();
    event.$replay();

    assertTrue(
        'Event must be handled',
        tabHandler.handleKeyboardShortcut(event, '', false));

    editableField.$verify();
    event.$verify();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testListOutdentContainLi() {
    field.innerHTML = '<ul><li>Text</li></ul>';

    const testText = field.firstChild.firstChild.firstChild;  // div ul li Test
    testHelper.select(field.firstChild, 0, testText, 4);

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
    event.shiftKey = true;

    editableField.execCommand(Command.OUTDENT);
    event.preventDefault();

    editableField.$replay();
    event.$replay();

    assertTrue(
        'Event must be handled',
        tabHandler.handleKeyboardShortcut(event, '', false));

    editableField.$verify();
    event.$verify();
  },

  testNoOp() {
    dom.setTextContent(field, 'Text');

    const testText = field.firstChild;
    testHelper.select(testText, 0, testText, 4);

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
    event.shiftKey = true;

    editableField.$replay();
    event.$replay();

    assertFalse(
        'Event must not be handled',
        tabHandler.handleKeyboardShortcut(event, '', false));

    editableField.$verify();
    event.$verify();
  },
});
