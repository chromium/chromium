/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.editor.focusTest');
goog.setTestOnly();

const BrowserFeature = goog.require('goog.editor.BrowserFeature');
const focus = goog.require('goog.editor.focus');
const selection = goog.require('goog.dom.selection');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  setUp() {
    // Make sure focus is not in the input to begin with.
    const dummy = document.getElementById('dummyLink');
    dummy.focus();
  },

  /**
   * Tests that focusInputField() puts focus in the input field and sets the
   * cursor to the end of the text cointained inside.
   * @suppress {strictMissingProperties} suppression added to enable type
   * checking
   */
  testFocusInputField() {
    const input = document.getElementById('myInput');
    assertNotEquals(
        'Input should not be focused initially', input, document.activeElement);

    focus.focusInputField(input);
    if (BrowserFeature.HAS_ACTIVE_ELEMENT) {
      assertEquals(
          'Input should be focused after call to focusInputField', input,
          document.activeElement);
    }
    assertEquals(
        'Selection should start at the end of the input text',
        input.value.length, selection.getStart(input));
    assertEquals(
        'Selection should end at the end of the input text', input.value.length,
        selection.getEnd(input));
  },
});
