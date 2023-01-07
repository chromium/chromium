/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.editor.plugins.SpacesTabHandlerTest');
goog.setTestOnly();

const BrowserEvent = goog.require('goog.events.BrowserEvent');
const FieldMock = goog.require('goog.testing.editor.FieldMock');
const KeyCodes = goog.require('goog.events.KeyCodes');
const Range = goog.require('goog.dom.Range');
const SpacesTabHandler = goog.require('goog.editor.plugins.SpacesTabHandler');
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
  /** @suppress {checkTypes} suppression added to enable type checking */
  setUp() {
    field = dom.getElement('field');
    editableField = new FieldMock();
    // Modal mode behavior tested in AbstractTabHandler.
    editableField.inModalMode = functions.FALSE;
    testHelper = new TestHelper(field);
    testHelper.setUpEditableElement();

    tabHandler = new SpacesTabHandler();
    tabHandler.registerFieldObject(editableField);
  },

  tearDown() {
    editableField = null;
    testHelper.tearDownEditableElement();
    tabHandler.dispose();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testSelectedTextIndent() {
    dom.setTextContent(field, 'Test');

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
    event.shiftKey = false;

    editableField.stopChangeEvents(true, true);
    editableField.dispatchChange();
    editableField.dispatchSelectionChangeEvent();
    event.preventDefault();

    editableField.$replay();
    event.$replay();

    assertTrue(
        'Event marked as handled',
        tabHandler.handleKeyboardShortcut(event, '', false));
    const contents = field.textContent || field.innerText;
    // Chrome doesn't treat \u00a0 as a space.
    assertTrue(
        `Text should be replaced with 4 spaces but was: "${contents}"`,
        /^(\s|\u00a0){4}$/.test(contents));

    editableField.$verify();
    event.$verify();
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testCursorIndent() {
    dom.setTextContent(field, 'Test');

    const testText = field.firstChild;
    testHelper.select(testText, 2, testText, 2);

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

    editableField.stopChangeEvents(true, true);
    editableField.dispatchChange();
    editableField.dispatchSelectionChangeEvent();
    event.preventDefault();

    editableField.$replay();
    event.$replay();

    assertTrue(
        'Event marked as handled',
        tabHandler.handleKeyboardShortcut(event, '', false));
    const contents = field.textContent || field.innerText;
    assertTrue(
        `Expected contents "Te    st" but was: "${contents}"`,
        /Te[\s|\u00a0]{4}st/.test(contents));

    editableField.$verify();
    event.$verify();
  },

  /**
     @suppress {checkTypes,missingProperties} suppression added to enable type
     checking
   */
  testShiftTabNoOp() {
    dom.setTextContent(field, 'Test');

    let range = Range.createFromNodeContents(field);
    range.collapse();
    range.select();

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

    event.preventDefault();
    editableField.$replay();
    event.$replay();

    assertTrue(
        'Event marked as handled',
        tabHandler.handleKeyboardShortcut(event, '', false));
    const contents = field.textContent || field.innerText;
    assertEquals('Shift+tab should not change contents', 'Test', contents);

    editableField.$verify();
    event.$verify();
  },

  testInListNoOp() {
    field.innerHTML = '<ul><li>Test</li></ul>';

    const testText = field.firstChild.firstChild.firstChild;  // div ul li Test
    testHelper.select(testText, 2, testText, 2);

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

    editableField.$replay();
    event.$replay();

    assertFalse(
        'Event must not be handled when selection inside list.',
        tabHandler.handleKeyboardShortcut(event, '', false));
    testHelper.assertHtmlMatches('<ul><li>Test</li></ul>');

    editableField.$verify();
    event.$verify();
  },

  testContainsListNoOp() {
    field.innerHTML = '<ul><li>Test</li></ul>';

    const testText = field.firstChild.firstChild.firstChild;  // div ul li Test
    testHelper.select(field.firstChild, 0, testText, 2);

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

    editableField.$replay();
    event.$replay();

    assertFalse(
        'Event must not be handled when selection inside list.',
        tabHandler.handleKeyboardShortcut(event, '', false));
    testHelper.assertHtmlMatches('<ul><li>Test</li></ul>');

    editableField.$verify();
    event.$verify();
  },
});
