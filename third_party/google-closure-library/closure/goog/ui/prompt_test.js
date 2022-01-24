/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.PromptTest');
goog.setTestOnly();

const BidiInput = goog.require('goog.ui.BidiInput');
const Dialog = goog.require('goog.ui.Dialog');
const InputHandler = goog.require('goog.events.InputHandler');
const KeyCodes = goog.require('goog.events.KeyCodes');
const Prompt = goog.require('goog.ui.Prompt');
const dispose = goog.require('goog.dispose');
const events = goog.require('goog.testing.events');
const functions = goog.require('goog.functions');
const googString = goog.require('goog.string');
const product = goog.require('goog.userAgent.product');
const selection = goog.require('goog.dom.selection');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

let prompt;

// An interactive test so we can manually see what it looks like.
function newPrompt() {
  prompt = new Prompt('title', 'Prompt:', (result) => {
    alert(`Result: ${result}`);
    dispose(prompt);
  }, 'defaultValue');
  prompt.setVisible(true);
}
testSuite({
  setUp() {
    document.body.focus();
  },

  tearDown() {
    dispose(prompt);
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testFocusOnInputElement() {
    // FF does not perform focus if the window is not active in the first place.
    if (userAgent.GECKO && document.hasFocus && !document.hasFocus()) {
      return;
    }

    let promptResult = undefined;
    prompt = new Prompt('title', 'Prompt:', (result) => {
      promptResult = result;
    }, 'defaultValue');
    prompt.setVisible(true);

    if (product.CHROME) {
      // For some reason, this test fails non-deterministically on Chrome,
      // but only on the test farm.
      return;
    }
    assertEquals('defaultValue', selection.getText(prompt.userInputEl_));
  },

  /**
     @suppress {strictMissingProperties,visibility} suppression added to enable
     type checking
   */
  testValidationFunction() {
    let promptResult = undefined;
    prompt = new Prompt('title', 'Prompt:', (result) => {
      promptResult = result;
    }, '');
    prompt.setValidationFunction(functions.not(googString.isEmptyOrWhitespace));
    prompt.setVisible(true);

    const buttonSet = prompt.getButtonSet();
    const okButton = buttonSet.getButton(Dialog.DefaultButtonKeys.OK);
    assertTrue(okButton.disabled);

    /** @suppress {visibility} suppression added to enable type checking */
    prompt.userInputEl_.value = '';
    events.fireKeySequence(prompt.userInputEl_, KeyCodes.SPACE);
    assertTrue(okButton.disabled);
    /** @suppress {visibility} suppression added to enable type checking */
    prompt.userInputEl_.value = 'foo';
    events.fireKeySequence(prompt.userInputEl_, KeyCodes.X);
    assertFalse(okButton.disabled);
  },

  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testBidiInput() {
    const shalomInHebrew = '\u05e9\u05dc\u05d5\u05dd';
    const promptResult = undefined;
    prompt = new Prompt('title', 'Prompt:', functions.NULL, '');
    const bidiInput = new BidiInput();
    prompt.setInputDecoratorFn(goog.bind(bidiInput.decorate, bidiInput));
    prompt.setVisible(true);

    /** @suppress {visibility} suppression added to enable type checking */
    prompt.userInputEl_.value = shalomInHebrew;
    events.fireKeySequence(prompt.userInputEl_, KeyCodes.SPACE);
    events.fireBrowserEvent({'target': prompt.userInputEl_, 'type': 'input'});
    bidiInput.inputHandler_.dispatchEvent(InputHandler.EventType.INPUT);
    assertEquals('rtl', prompt.userInputEl_.dir);

    /** @suppress {visibility} suppression added to enable type checking */
    prompt.userInputEl_.value = 'shalomInEnglish';
    events.fireKeySequence(prompt.userInputEl_, KeyCodes.SPACE);
    events.fireBrowserEvent({'target': prompt.userInputEl_, 'type': 'input'});
    bidiInput.inputHandler_.dispatchEvent(InputHandler.EventType.INPUT);
    assertEquals('ltr', prompt.userInputEl_.dir);
    dispose(bidiInput);
  },

  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testBidiInput_off() {
    const shalomInHebrew = '\u05e9\u05dc\u05d5\u05dd';
    const promptResult = undefined;
    prompt = new Prompt('title', 'Prompt:', functions.NULL, '');
    prompt.setVisible(true);

    /** @suppress {visibility} suppression added to enable type checking */
    prompt.userInputEl_.value = shalomInHebrew;
    events.fireKeySequence(prompt.userInputEl_, KeyCodes.SPACE);
    events.fireBrowserEvent({'target': prompt.userInputEl_, 'type': 'input'});
    assertEquals('', prompt.userInputEl_.dir);

    /** @suppress {visibility} suppression added to enable type checking */
    prompt.userInputEl_.value = 'shalomInEnglish';
    events.fireKeySequence(prompt.userInputEl_, KeyCodes.SPACE);
    assertEquals('', prompt.userInputEl_.dir);
  },
});
