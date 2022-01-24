/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.SavedRangeTest');
goog.setTestOnly();

const Range = goog.require('goog.dom.Range');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

testSuite({
  testSaved() {
    const node = dom.getElement('test1');
    let range = Range.createFromNodeContents(node);
    const savedRange = range.saveUsingDom();

    range = savedRange.restore(true);
    assertEquals(
        'Restored range should select "Text"', 'Text', range.getText());
    assertFalse('Restored range should not be reversed.', range.isReversed());
    assertFalse(
        'Range should not have disposed itself.', savedRange.isDisposed());

    Range.clearSelection();
    assertFalse(Range.hasSelection(window));

    range = savedRange.restore();
    assertTrue('Range should have auto-disposed.', savedRange.isDisposed());
    assertEquals(
        'Restored range should select "Text"', 'Text', range.getText());
    assertFalse('Restored range should not be reversed.', range.isReversed());
  },

  testReversedSave() {
    const node = dom.getElement('test1').firstChild;
    let range = Range.createFromNodes(node, 4, node, 0);
    const savedRange = range.saveUsingDom();

    range = savedRange.restore();
    assertEquals(
        'Restored range should select "Text"', 'Text', range.getText());
    if (!userAgent.IE) {
      assertTrue('Restored range should be reversed.', range.isReversed());
    }
  },
});
