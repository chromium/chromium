/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.ModalAriaVisibilityHelperTest');
goog.setTestOnly();

const ModalAriaVisibilityHelper = goog.require('goog.ui.ModalAriaVisibilityHelper');
const State = goog.require('goog.a11y.aria.State');
const aria = goog.require('goog.a11y.aria');
const dom = goog.require('goog.dom');
const googString = goog.require('goog.string');
const testSuite = goog.require('goog.testing.testSuite');

function assertUnalteredElements() {
  assertEmptyAriaHiddenState('div-2-1');
  assertAriaHiddenState('div-3', 'false');
  assertAriaHiddenState('div-5', 'true');
}

/**
 * @param {string} id Id of the element.
 * @return {!ModalAriaVisibilityHelper}
 * @suppress {checkTypes} suppression added to enable type checking
 */
function createHelper(id) {
  return new ModalAriaVisibilityHelper(dom.getElement(id), dom.getDomHelper());
}

/** @suppress {checkTypes} suppression added to enable type checking */
function clearAriaState(id) {
  aria.removeState(dom.getElement(id), State.HIDDEN);
}

/**
 * @param {string} id Id of the element.
 * @suppress {checkTypes} suppression added to enable type checking
 */
function assertEmptyAriaHiddenState(id) {
  const element = dom.getElement(id);
  assertTrue(googString.isEmptyOrWhitespace(
      googString.makeSafe(aria.getState(element, State.HIDDEN))));
}

/**
 * @param {string} id Id of the element.
 * @param {string} expectedState
 * @suppress {checkTypes} suppression added to enable type checking
 */
function assertAriaHiddenState(id, expectedState) {
  const element = dom.getElement(id);
  assertEquals(expectedState, aria.getState(element, State.HIDDEN));
}
testSuite({
  tearDown() {
    clearAriaState('div-1');
    clearAriaState('div-2');
    clearAriaState('div-4');
  },

  testHide() {
    const helper = createHelper('div-1');
    helper.setBackgroundVisibility(true /* hide */);

    assertUnalteredElements();
    assertEmptyAriaHiddenState('div-1');
    assertAriaHiddenState('div-2', 'true');
    assertAriaHiddenState('div-4', 'true');
  },

  testUnhide() {
    const helper = createHelper('div-1');
    helper.setBackgroundVisibility(false /* hide */);

    assertUnalteredElements();
    assertEmptyAriaHiddenState('div-1');
    assertEmptyAriaHiddenState('div-2');
    assertEmptyAriaHiddenState('div-4');
  },

  testMultipleCalls() {
    const helper = createHelper('div-2');
    helper.setBackgroundVisibility(true /* hide */);

    assertUnalteredElements();
    assertAriaHiddenState('div-1', 'true');
    assertEmptyAriaHiddenState('div-2');
    assertAriaHiddenState('div-4', 'true');

    helper.setBackgroundVisibility(false /* hide */);

    assertUnalteredElements();
    assertEmptyAriaHiddenState('div-1');
    assertEmptyAriaHiddenState('div-2');
    assertEmptyAriaHiddenState('div-4');

    helper.setBackgroundVisibility(true /* hide */);

    assertUnalteredElements();
    assertAriaHiddenState('div-1', 'true');
    assertEmptyAriaHiddenState('div-2');
    assertAriaHiddenState('div-4', 'true');
  },
});
