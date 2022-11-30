/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.TextRangeIteratorTest');
goog.setTestOnly();

const StopIteration = goog.require('goog.iter.StopIteration');
const TagName = goog.require('goog.dom.TagName');
const TextRangeIterator = goog.require('goog.dom.TextRangeIterator');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');
const testingDom = goog.require('goog.testing.dom');

let test;
let test2;

testSuite({
  setUpPage() {
    test = dom.getElement('test');
    test2 = dom.getElement('test2');
  },

  testBasic() {
    testingDom.assertNodesMatch(new TextRangeIterator(test, 0, test, 2), [
      '#a1', 'T', '#b1', 'e', '#b1', 'xt', '#a1', '#span1', '#span1', '#p1'
    ]);
  },

  testAdjustStart() {
    const iterator = new TextRangeIterator(test, 0, test, 2);
    iterator.setStartNode(dom.getElement('span1'));

    testingDom.assertNodesMatch(iterator, ['#span1', '#span1', '#p1']);
  },

  testAdjustEnd() {
    const iterator = new TextRangeIterator(test, 0, test, 2);
    iterator.setEndNode(dom.getElement('span1'));

    testingDom.assertNodesMatch(
        iterator, ['#a1', 'T', '#b1', 'e', '#b1', 'xt', '#a1', '#span1']);
  },

  testOffsets() {
    const iterator =
        new TextRangeIterator(test2.firstChild, 1, test2.lastChild, 2);

    // foo
    let node = iterator.nextValueOrThrow();
    assertEquals(
        'Should have start offset at iteration step 1', 1,
        iterator.getStartTextOffset());
    assertEquals(
        'Should not have end offset at iteration step 1', node.nodeValue.length,
        iterator.getEndTextOffset());

    // <br>
    node = iterator.nextValueOrThrow();
    assertEquals(
        'Should not have start offset at iteration step 2', -1,
        iterator.getStartTextOffset());
    assertEquals(
        'Should not have end offset at iteration step 2', -1,
        iterator.getEndTextOffset());

    // </br>
    node = iterator.nextValueOrThrow();
    assertEquals(
        'Should not have start offset at iteration step 3', -1,
        iterator.getStartTextOffset());
    assertEquals(
        'Should not have end offset at iteration step 3', -1,
        iterator.getEndTextOffset());

    // bar
    node = iterator.nextValueOrThrow();
    assertEquals(
        'Should not have start offset at iteration step 4', 0,
        iterator.getStartTextOffset());
    assertEquals(
        'Should have end offset at iteration step 4', 2,
        iterator.getEndTextOffset());
  },

  testSingleNodeOffsets() {
    const iterator =
        new TextRangeIterator(test2.firstChild, 1, test2.firstChild, 2);

    iterator.nextValueOrThrow();
    assertEquals('Should have start offset', 1, iterator.getStartTextOffset());
    assertEquals('Should have end offset', 2, iterator.getEndTextOffset());
  },

  testEndNodeOffsetAtEnd() {
    const iterator = new TextRangeIterator(
        dom.getElement('b1').firstChild, 0, dom.getElement('b1'), 1);
    testingDom.assertNodesMatch(iterator, ['e', '#b1']);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testSkipTagDoesNotSkipEnd() {
    // Iterate over 'Tex'.
    const iterator = new TextRangeIterator(
        test.firstChild.firstChild, 0, test.firstChild.lastChild, 1);

    let node = iterator.nextValueOrThrow();
    assertEquals('T', node.nodeValue);

    node = iterator.nextValueOrThrow();
    assertEquals(String(TagName.B), node.tagName);

    iterator.skipTag();

    node = iterator.nextValueOrThrow();
    assertEquals('xt', node.nodeValue);
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testSkipTagSkipsEnd() {
    // Iterate over 'Te'.
    /** @suppress {checkTypes} suppression added to enable type checking */
    const iterator = new TextRangeIterator(
        test.firstChild.firstChild, 0,
        dom.getElementsByTagName(TagName.B, test)[0].firstChild, 1);

    let node = iterator.nextValueOrThrow();
    assertEquals('T', node.nodeValue);

    node = iterator.nextValueOrThrow();
    assertEquals(String(TagName.B), node.tagName);

    const ex = assertThrows('Should stop iteration when skipping B', () => {
      iterator.skipTag();
    });
    assertEquals(StopIteration, ex);
  },

  testReverseIteration() {
    testingDom.assertNodesMatch(new TextRangeIterator(test, 0, test, 2, true), [
      '#p1',
      'Text',
      '#p1',
      '#span1',
      '#span1',
      '#a1',
      'xt',
      '#b1',
      'e',
      '#b1',
      'T',
      '#a1',
    ]);
  },

  testReverseIterationWithOffsets() {
    const iterator =
        new TextRangeIterator(test2.firstChild, 1, test2.lastChild, 2, true);

    // bar
    let node = iterator.nextValueOrThrow();
    assertEquals(
        'Should have start offset at iteration step 1', 0,
        iterator.getStartTextOffset());
    assertEquals(
        'Should not have end offset at iteration step 1', 2,
        iterator.getEndTextOffset());

    // </br>
    node = iterator.nextValueOrThrow();
    assertEquals(
        'Should not have start offset at iteration step 2', -1,
        iterator.getStartTextOffset());
    assertEquals(
        'Should not have end offset at iteration step 2', -1,
        iterator.getEndTextOffset());

    // <br>
    node = iterator.nextValueOrThrow();
    assertEquals(
        'Should not have start offset at iteration step 3', -1,
        iterator.getStartTextOffset());
    assertEquals(
        'Should not have end offset at iteration step 3', -1,
        iterator.getEndTextOffset());

    // foo
    node = iterator.nextValueOrThrow();
    assertEquals(
        'Should not have start offset at iteration step 4', 1,
        iterator.getStartTextOffset());
    assertEquals(
        'Should have end offset at iteration step 4', node.nodeValue.length,
        iterator.getEndTextOffset());
  },
});
