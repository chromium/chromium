/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.BidiInputTest');
goog.setTestOnly();

const BidiInput = goog.require('goog.ui.BidiInput');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  setUp() {
    document.body.focus();
  },

  tearDown() {
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    document.getElementById('emptyText').value = '';
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    document.getElementById('bidiText').value = 'hello, world!';
  },

  testEmptyInput() {
    const bidiInput = new BidiInput();
    const emptyText = dom.getElement('emptyText');
    bidiInput.decorate(emptyText);
    assertEquals('', bidiInput.getValue());
    bidiInput.setValue('hello!');
    assertEquals('hello!', bidiInput.getValue());
  },

  testSetDirection() {
    const shalomInHebrew = '\u05e9\u05dc\u05d5\u05dd';
    const isAGoodLanguageInHebrew =
        '\u05d4\u05d9\u05d0 \u05e9\u05e4\u05d4 \u05d8\u05d5\u05d1\u05d4';
    const learnInHebrew = '\u05dc\u05de\u05d3';

    const bidiInput = new BidiInput();
    const bidiText = dom.getElement('bidiText');
    bidiInput.decorate(bidiText);
    assertEquals('ltr', bidiInput.getDirection());

    bidiInput.setValue(shalomInHebrew);
    assertEquals('rtl', bidiInput.getDirection());

    bidiInput.setValue('hello!');
    assertEquals('ltr', bidiInput.getDirection());

    bidiInput.setValue(`:) , ? ! ${shalomInHebrew}`);
    assertEquals('rtl', bidiInput.getDirection());

    bidiInput.setValue(':) , ? ! hello!');
    assertEquals('ltr', bidiInput.getDirection());

    bidiInput.setValue('   ;)   ');
    assertEquals(null, bidiInput.getDirection());

    bidiInput.setValue(`${shalomInHebrew}, how are you today?`);
    assertEquals('ltr', bidiInput.getDirection());

    bidiInput.setValue(`Hello is ${shalomInHebrew} in Hebrew`);
    assertEquals('ltr', bidiInput.getDirection());

    bidiInput.setValue(`JavaScript ${isAGoodLanguageInHebrew}`);
    assertEquals('rtl', bidiInput.getDirection());

    bidiInput.setValue(`${learnInHebrew} JavaScript`);
    assertEquals('rtl', bidiInput.getDirection());

    bidiInput.setValue('');
    assertEquals(null, bidiInput.getDirection());
  },

  testSetDirection_inContenteditableDiv() {
    const shalomInHebrew = '\u05e9\u05dc\u05d5\u05dd';
    const isAGoodLanguageInHebrew =
        '\u05d4\u05d9\u05d0 \u05e9\u05e4\u05d4 \u05d8\u05d5\u05d1\u05d4';
    const learnInHebrew = '\u05dc\u05de\u05d3';

    const bidiInput = new BidiInput();
    const bidiTextDiv = dom.getElement('bidiTextDiv');
    bidiInput.decorate(bidiTextDiv);
    assertEquals('ltr', bidiInput.getDirection());

    bidiInput.setValue(shalomInHebrew);
    assertEquals('rtl', bidiInput.getDirection());

    bidiInput.setValue('hello!');
    assertEquals('ltr', bidiInput.getDirection());

    bidiInput.setValue(`:) , ? ! ${shalomInHebrew}`);
    assertEquals('rtl', bidiInput.getDirection());

    bidiInput.setValue(':) , ? ! hello!');
    assertEquals('ltr', bidiInput.getDirection());

    bidiInput.setValue('   ;)   ');
    assertEquals(null, bidiInput.getDirection());

    bidiInput.setValue(`${shalomInHebrew}, how are you today?`);
    assertEquals('ltr', bidiInput.getDirection());

    bidiInput.setValue(`Hello is ${shalomInHebrew} in Hebrew`);
    assertEquals('ltr', bidiInput.getDirection());

    bidiInput.setValue(`JavaScript ${isAGoodLanguageInHebrew}`);
    assertEquals('rtl', bidiInput.getDirection());

    bidiInput.setValue(`${learnInHebrew} JavaScript`);
    assertEquals('rtl', bidiInput.getDirection());

    bidiInput.setValue('');
    assertEquals(null, bidiInput.getDirection());
  },
});
