/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.editor.ToolbarFactoryTest');
goog.setTestOnly();

const ExpectedFailures = goog.require('goog.testing.ExpectedFailures');
const TestHelper = goog.require('goog.testing.editor.TestHelper');
const ToolbarFactory = goog.require('goog.ui.editor.ToolbarFactory');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');

let helper;
let expectedFailures;

testSuite({
  setUpPage() {
    helper = new TestHelper(dom.getElement('myField'));
    expectedFailures = new ExpectedFailures();
  },

  setUp() {
    helper.setUpEditableElement();
  },

  tearDown() {
    helper.tearDownEditableElement();
    expectedFailures.handleTearDown();
  },

  /**
   * Makes sure we have the correct conversion table in
   * ToolbarFactory.LEGACY_SIZE_TO_PX_MAP_. Can only be tested in
   * a browser that takes legacy size values as input to execCommand but returns
   * pixel size values from queryCommandValue. That's OK because that's the only
   * situation where this conversion table's precision is critical. (When it's
   * used to size the labels of the font size menu options it's ok if it's a few
   * pixels off.)
   * @suppress {visibility} suppression added to enable type checking
   */
  testGetLegacySizeFromPx() {
    // We will be warned if other browsers start behaving like webkit pre-534.7.
    expectedFailures.expectFailureFor(true);
    try {
      const fieldElem = dom.getElement('myField');
      // Start from 1 because size 0 is bogus (becomes 16px, legacy size 3).
      for (let i = 1; i < ToolbarFactory.LEGACY_SIZE_TO_PX_MAP_.length; i++) {
        helper.select(fieldElem, 0, fieldElem, 1);
        document.execCommand('fontSize', false, i);
        helper.select('foo', 1);
        const value = document.queryCommandValue('fontSize');
        assertEquals(
            `Px size ${value} should convert to legacy size ${i}`, i,
            ToolbarFactory.getLegacySizeFromPx(parseInt(value, 10)));
      }
    } catch (e) {
      expectedFailures.handleException(e);
    }
  },
});
