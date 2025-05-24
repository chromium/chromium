/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.AbstractRangeTest');
goog.setTestOnly();

const AbstractRange = goog.require('goog.dom.AbstractRange');
const Const = goog.require('goog.string.Const');
const Range = goog.require('goog.dom.Range');
const TagName = goog.require('goog.dom.TagName');
const TrustedResourceUrl = goog.require('goog.html.TrustedResourceUrl');
const dom = goog.require('goog.dom');
const safe = goog.require('goog.dom.safe');
const testSuite = goog.require('goog.testing.testSuite');


testSuite({

  testCorrectDocument() {
    const aFrame = createTestFrame();
    document.body.appendChild(aFrame);
    const bFrame = createTestFrame();
    document.body.appendChild(bFrame);
    try {
      const a = aFrame.contentWindow;
      const b = bFrame.contentWindow;
      a.document.body.setAttribute('contenteditable', true);
      a.document.body.textContent = 'asdf';
      b.document.body.setAttribute('contenteditable', true);
      b.document.body.textContent = 'asdf';

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
      selection = /** @type {?{rangeCount: number}} */ (
          AbstractRange.getBrowserSelectionForWindow(a));
      // Some (non-IE) browsers keep a separate selection state for each
      // document in the same browser window. That's fine, as long as the
      // selection object requested from the window object is correctly
      // associated with that window's document.
      if (selection != null && selection.rangeCount != 0) {
        range = Range.createFromBrowserSelection(selection);
        assertEquals(
            'getBrowserSelectionForWindow must return selection in ' +
                'the correct document',
            a.document, range.getDocument());
      } else {
        assertTrue(selection == null || selection.rangeCount == 0);
      }
    } finally {
      dom.removeNode(aFrame);
      dom.removeNode(bFrame);
    }
  },

  testSelectionIsControlRange() {
    const frame = createTestFrame();
    document.body.appendChild(frame);
    try {
      const c = frame.contentWindow;
      c.document.body.setAttribute('contenteditable', true);
      c.document.body.appendChild(c.document.createElement('img'));

      // Only IE supports control ranges
      if (c.document.body.createControlRange) {
        const controlRange = c.document.body.createControlRange();
        controlRange.add(dom.getElementsByTagName(TagName.IMG, c.document)[0]);
        controlRange.select();
        const selection = AbstractRange.getBrowserSelectionForWindow(c);
        assertNotNull('Selection must not be null', selection);
      }
    } finally {
      dom.removeNode(frame);
    }
  },
});

/**
 * @return {!HTMLIFrameElement}
 */
function createTestFrame() {
  const frame = dom.createDom(TagName.IFRAME);
  safe.setIframeSrc(
      frame, TrustedResourceUrl.fromConstant(Const.from('about:blank')));
  return frame;
}