/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.CookieEditorTest');
goog.setTestOnly();

const CookieEditor = goog.require('goog.ui.CookieEditor');
const EventType = goog.require('goog.events.EventType');
const GoogEvent = goog.require('goog.events.Event');
const cookies = goog.require('goog.net.cookies');
const dom = goog.require('goog.dom');
const events = goog.require('goog.testing.events');
const testSuite = goog.require('goog.testing.testSuite');

const COOKIE_KEY = 'my_fabulous_cookie';
let COOKIE_VALUES;

cookies.get = (key) => COOKIE_VALUES[key];

cookies.set = (key, value) => COOKIE_VALUES[key] = value;

/** @suppress {missingReturn} suppression added to enable type checking */
cookies.remove = (key, value) => {
  delete COOKIE_VALUES[key];
};

/** @suppress {visibility} suppression added to enable type checking */
function newCookieEditor(cookieValue = undefined) {
  // Set cookie.
  if (cookieValue) {
    cookies.set(COOKIE_KEY, cookieValue);
  }

  // Render editor.
  const editor = new CookieEditor();
  editor.selectCookie(COOKIE_KEY);
  editor.render(dom.getElement('test_container'));
  assertEquals(
      'wrong text area value', cookieValue || '',
      editor.textAreaElem_.value || '');

  return editor;
}

testSuite({
  setUp() {
    dom.removeChildren(dom.getElement('test_container'));
    COOKIE_VALUES = {};
  },

  testRender() {
    // Render editor.
    const editor = newCookieEditor();

    // All expected elements created?
    const elem = editor.getElement();
    assertNotNullNorUndefined('missing element', elem);
    assertNotNullNorUndefined('missing clear button', editor.clearButtonElem_);
    assertNotNullNorUndefined(
        'missing update button', editor.updateButtonElem_);
    assertNotNullNorUndefined('missing text area', editor.textAreaElem_);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testEditCookie() {
    // Render editor.
    const editor = newCookieEditor();

    // Invalid value.
    let newValue = 'my bad value;';
    editor.textAreaElem_.value = newValue;
    events.fireBrowserEvent(
        new GoogEvent(EventType.CLICK, editor.updateButtonElem_));
    assertTrue('unexpected cookie value', !cookies.get(COOKIE_KEY));

    // Valid value.
    newValue = 'my fabulous value';
    editor.textAreaElem_.value = newValue;
    events.fireBrowserEvent(
        new GoogEvent(EventType.CLICK, editor.updateButtonElem_));
    assertEquals('wrong cookie value', newValue, cookies.get(COOKIE_KEY));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testClearCookie() {
    // Render editor.
    const value = 'I will be cleared';
    const editor = newCookieEditor(value);

    // Clear value.
    events.fireBrowserEvent(
        new GoogEvent(EventType.CLICK, editor.clearButtonElem_));
    assertTrue('unexpected cookie value', !cookies.get(COOKIE_KEY));
  },
});
