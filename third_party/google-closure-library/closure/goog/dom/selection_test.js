/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.selectionTest');
goog.setTestOnly();

const InputType = goog.require('goog.dom.InputType');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const selection = goog.require('goog.dom.selection');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

let input;
let hiddenInput;
let textarea;
let hiddenTextarea;

function getStartHelper(field, hiddenField) {
  assertEquals(0, selection.getStart(field));
  assertEquals(0, selection.getStart(hiddenField));

  field.focus();
  assertEquals(0, selection.getStart(field));
}

function setTextHelper(field) {
  // Test one line string only
  select(field);
  assertEquals('', selection.getText(field));

  selection.setText(field, 'Get Behind Me Satan');
  assertEquals('Get Behind Me Satan', selection.getText(field));
}

function setCursorHelper(field) {
  select(field);
  // try to set the cursor beyond the length of the content
  selection.setStart(field, 5);
  selection.setEnd(field, 15);
  assertEquals(0, selection.getStart(field));
  assertEquals(0, selection.getEnd(field));

  select(field);
  const message = 'Get Behind Me Satan';
  selection.setText(field, message);
  selection.setStart(field, 5);
  selection.setEnd(field, message.length);
  assertEquals(5, selection.getStart(field));
  assertEquals(message.length, selection.getEnd(field));

  // Set the end before the start, and see if getEnd returns the start
  // position itself.
  selection.setStart(field, 5);
  selection.setEnd(field, 3);
  assertEquals(3, selection.getEnd(field));
}

function getAndSetSelectedTextHelper(field) {
  select(field);
  selection.setText(field, 'Get Behind Me Satan');

  // select 'Behind'
  selection.setStart(field, 4);
  selection.setEnd(field, 10);
  assertEquals('Behind', selection.getText(field));

  selection.setText(field, 'In Front Of');
  selection.setStart(field, 0);
  selection.setEnd(field, 100);
  assertEquals('Get In Front Of Me Satan', selection.getText(field));
}

function setCursorOnHiddenInputHelper(hiddenField) {
  selection.setStart(hiddenField, 0);
  assertEquals(0, selection.getStart(hiddenField));
}

/** Helper function to clear the textfield contents. */
function clearField(field) {
  field.value = '';
}

/** Helper function to set the start and end and assert the getter values. */
function checkSetAndGetTextarea(start, end) {
  selection.setStart(textarea, start);
  selection.setEnd(textarea, end);
  assertEquals(start, selection.getStart(textarea));
  assertEquals(end, selection.getEnd(textarea));
}

/**
 * Helper function to focus and select a field. In IE8, selected
 * fields need focus.
 */
function select(field) {
  field.focus();
  field.select();
}
testSuite({
  setUp() {
    input = dom.createDom(TagName.INPUT, {type: InputType.TEXT});
    textarea = dom.createDom(TagName.TEXTAREA);
    hiddenInput = dom.createDom(
        TagName.INPUT, {type: InputType.TEXT, style: 'display: none'});
    hiddenTextarea = dom.createDom(TagName.TEXTAREA, {style: 'display: none'});

    document.body.appendChild(input);
    document.body.appendChild(textarea);
    document.body.appendChild(hiddenInput);
    document.body.appendChild(hiddenTextarea);
  },

  tearDown() {
    dom.removeChildren(document.body);
  },

  /** Tests getStart routine in both input and textarea. */
  testGetStartInput() {
    getStartHelper(input, hiddenInput);
  },

  testGetStartTextarea() {
    getStartHelper(textarea, hiddenTextarea);
  },

  /**
   * Tests that getStart routine does not error for elements that are neither
   * text inputs nor text areas.
   */
  testGetStartOther() {
    const button = dom.createDom(TagName.BUTTON);
    const hiddenButton =
        dom.createDom(TagName.BUTTON, {style: 'display: none'});
    document.body.appendChild(button);
    document.body.appendChild(hiddenButton);
    getStartHelper(button, hiddenButton);
  },

  /**
   * Tests the setText routine for both input and textarea
   * with a single line of text.
   */
  testSetTextInput() {
    setTextHelper(input);
  },

  testSetTextTextarea() {
    setTextHelper(textarea);
  },

  /** Tests the setText routine for textarea with multiple lines of text. */
  testSetTextMultipleLines() {
    select(textarea);
    assertEquals('', selection.getText(textarea));
    const isLegacyIE = userAgent.IE && !userAgent.isVersionOrHigher('9');
    const message =
        isLegacyIE ? 'Get Behind Me\r\nSatan' : 'Get Behind Me\nSatan';
    selection.setText(textarea, message);
    assertEquals(message, selection.getText(textarea));

    // Select the text up to the point just after the \r\n combination
    // or \n in GECKO.
    const endOfNewline = isLegacyIE ? 15 : 14;
    let selectedMessage = message.substring(0, endOfNewline);
    selection.setStart(textarea, 0);
    selection.setEnd(textarea, endOfNewline);
    assertEquals(selectedMessage, selection.getText(textarea));

    selectedMessage = isLegacyIE ? '\r\n' : '\n';
    selection.setStart(textarea, 13);
    selection.setEnd(textarea, endOfNewline);
    assertEquals(selectedMessage, selection.getText(textarea));
  },

  /** Tests the setCursor routine for both input and textarea. */
  testSetCursorInput() {
    setCursorHelper(input);
  },

  testSetCursorTextarea() {
    setCursorHelper(textarea);
  },

  /**
   * Tests the getText and setText routines acting on selected text in
   * both input and textarea.
   */
  testGetAndSetSelectedTextInput() {
    getAndSetSelectedTextHelper(input);
  },

  testGetAndSetSelectedTextTextarea() {
    getAndSetSelectedTextHelper(textarea);
  },

  /** Test setStart on hidden input and hidden textarea. */
  testSetCursorOnHiddenInput() {
    setCursorOnHiddenInputHelper(hiddenInput);
  },

  testSetCursorOnHiddenTextarea() {
    setCursorOnHiddenInputHelper(hiddenTextarea);
  },

  /**
   * Test setStart, setEnd, getStart and getEnd in textarea with text
   * containing line breaks.
   */
  testSetAndGetCursorWithLineBreaks() {
    select(textarea);
    const isLegacyIE = userAgent.IE && !userAgent.isVersionOrHigher('9');
    const newline = isLegacyIE ? '\r\n' : '\n';
    let message = `Hello${newline}World`;
    selection.setText(textarea, message);

    // Test setEnd and getEnd, by setting the cursor somewhere after the
    // \r\n combination.
    selection.setEnd(textarea, 9);
    assertEquals(9, selection.getEnd(textarea));

    // Test basic setStart and getStart
    selection.setStart(textarea, 10);
    assertEquals(10, selection.getStart(textarea));

    // Test setEnd and getEnd, by setting the cursor exactly after the
    // \r\n combination in IE or after \n in GECKO.
    let endOfNewline = isLegacyIE ? 7 : 6;
    checkSetAndGetTextarea(endOfNewline, endOfNewline);

    // Select a \r\n combination in IE or \n in GECKO and see if
    // getStart and getEnd work correctly.
    clearField(textarea);
    message = `Hello${newline}${newline}World`;
    selection.setText(textarea, message);
    const startOfNewline = isLegacyIE ? 7 : 6;
    endOfNewline = isLegacyIE ? 9 : 7;
    checkSetAndGetTextarea(startOfNewline, endOfNewline);

    // Select 2 \r\n combinations in IE or 2 \ns in GECKO and see if getStart
    // and getEnd work correctly.
    checkSetAndGetTextarea(5, endOfNewline);

    // Position cursor b/w 2 \r\n combinations in IE or 2 \ns in GECKO and see
    // if getStart and getEnd work correctly.
    clearField(textarea);
    message = `Hello${newline}${newline}${newline}${newline}World`;
    selection.setText(textarea, message);
    const middleOfNewlines = isLegacyIE ? 9 : 7;
    checkSetAndGetTextarea(middleOfNewlines, middleOfNewlines);

    // Position cursor at end of a textarea which ends with \r\n in IE or \n in
    // GECKO.
    if (!userAgent.IE || !userAgent.isVersionOrHigher('11')) {
      // TODO(johnlenz): investigate why this fails in IE 11.
      clearField(textarea);
      message = `Hello${newline}${newline}`;
      selection.setText(textarea, message);
      const endOfTextarea = message.length;
      checkSetAndGetTextarea(endOfTextarea, endOfTextarea);
    }

    // Position cursor at the end of the 2 starting \r\ns in IE or \ns in GECKO
    // within a textarea.
    clearField(textarea);
    message = `${newline}${newline}World`;
    selection.setText(textarea, message);
    const endOfTwoNewlines = isLegacyIE ? 4 : 2;
    checkSetAndGetTextarea(endOfTwoNewlines, endOfTwoNewlines);

    // Position cursor at the end of the first \r\n in IE or \n in
    // GECKO within a textarea.
    let endOfOneNewline = isLegacyIE ? 2 : 1;
    checkSetAndGetTextarea(endOfOneNewline, endOfOneNewline);
  },

  /**
   * Test to make sure there's no error when getting the range of an unselected
   * textarea. See bug 1274027.
   */
  testGetStartOnUnfocusedTextarea() {
    input.value = 'White Blood Cells';
    input.focus();
    selection.setCursorPosition(input, 5);

    assertEquals(
        'getStart on input should return where we put the cursor', 5,
        selection.getStart(input));

    assertEquals(
        'getStart on unfocused textarea should succeed without error', 0,
        selection.getStart(textarea));
  },

  /**
   * Test to make sure there's no error setting cursor position within a
   * textarea after a newline. This is problematic on IE because of the
   * '\r\n' vs '\n' issue.
   */
  testSetCursorPositionTextareaWithNewlines() {
    textarea.value = 'Hello\nWorld';
    textarea.focus();

    // Set the selection point between 'W' and 'o'. Position is computed this
    // way instead of being hard-coded because it's different in IE due to \r\n
    // vs \n.
    selection.setCursorPosition(textarea, textarea.value.length - 4);

    const isLegacyIE = userAgent.IE && !userAgent.isVersionOrHigher('9');
    const linebreak = isLegacyIE ? '\r\n' : '\n';
    const expectedLeftString = `Hello${linebreak}W`;

    assertEquals(
        'getStart on input should return after the newline',
        expectedLeftString.length, selection.getStart(textarea));
    assertEquals(
        'getEnd on input should return after the newline',
        expectedLeftString.length, selection.getEnd(textarea));

    selection.setEnd(textarea, textarea.value.length);
    assertEquals('orld', selection.getText(textarea));
  },
});
