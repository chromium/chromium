/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Utilties to handle focusing related to rich text editing.
 */

goog.provide('goog.editor.focus');

goog.require('goog.dom.selection');


/**
 * Change focus to the given input field and set cursor to end of current text.
 * @param {Element} inputElem Input DOM element.
 * @suppress {strictMissingProperties} Part of the go/strict_warnings_migration
 */
goog.editor.focus.focusInputField = function(inputElem) {
  'use strict';
  inputElem.focus();
  goog.dom.selection.setCursorPosition(inputElem, inputElem.value.length);
};
