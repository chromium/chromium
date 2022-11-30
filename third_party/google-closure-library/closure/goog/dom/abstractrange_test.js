/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.AbstractRangeTest');
goog.setTestOnly();

const AbstractRange = goog.require('goog.dom.AbstractRange');
const Range = goog.require('goog.dom.Range');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testCorrectDocument() {
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    const a = dom.getElement('a').contentWindow;
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    const b = dom.getElement('b').contentWindow;

    a.document.body.focus();
    let selection = AbstractRange.getBrowserSelectionForWindow(a);
    assertNotNull('Selection must not be null', selection);
    /** @suppress {checkTypes} suppression added to enable type checking */
    let range = Range.createFromBrowserSelection(selection);
    assertEquals(
        'getBrowserSelectionForWindow must return selection in the ' +
            'correct document',
        a.document, range.getDocument());

    // This is intended to trip up Internet Explorer --
    // see http://b/2048934
    b.document.body.focus();
    selection = AbstractRange.getBrowserSelectionForWindow(a);
    // Some (non-IE) browsers keep a separate selection state for each document
    // in the same browser window. That's fine, as long as the selection object
    // requested from the window object is correctly associated with that
    // window's document.
    if (selection != null && selection.rangeCount != 0) {
      range = Range.createFromBrowserSelection(selection);
      assertEquals(
          'getBrowserSelectionForWindow must return selection in ' +
              'the correct document',
          a.document, range.getDocument());
    } else {
      assertTrue(selection == null || selection.rangeCount == 0);
    }
  },

  testSelectionIsControlRange() {
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    const c = dom.getElement('c').contentWindow;
    // Only IE supports control ranges
    if (c.document.body.createControlRange) {
      const controlRange = c.document.body.createControlRange();
      controlRange.add(dom.getElementsByTagName(TagName.IMG, c.document)[0]);
      controlRange.select();
      const selection = AbstractRange.getBrowserSelectionForWindow(c);
      assertNotNull('Selection must not be null', selection);
    }
  },
});
