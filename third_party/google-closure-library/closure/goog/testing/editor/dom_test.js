/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.editor.domTest');
goog.setTestOnly();

const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const editorDom = goog.require('goog.testing.editor.dom');
const functions = goog.require('goog.functions');
const testSuite = goog.require('goog.testing.testSuite');

let root;
let childNode1;
let childNode2;
let childNode3;
let parentNode;

let first;
let last;
let middle;

function setUpNonEmptyTests() {
  childNode1 = dom.createElement(TagName.DIV);
  childNode2 = dom.createElement(TagName.DIV);
  childNode3 = dom.createElement(TagName.DIV);
  parentNode =
      dom.createDom(TagName.DIV, null, childNode1, childNode2, childNode3);
  dom.appendChild(root, parentNode);

  childNode1.appendChild(dom.createTextNode('One'));
  childNode1.appendChild(dom.createTextNode(''));

  childNode2.appendChild(dom.createElement(TagName.BR));
  childNode2.appendChild(dom.createTextNode('TwoA'));
  childNode2.appendChild(dom.createTextNode('TwoB'));
  childNode2.appendChild(dom.createElement(TagName.BR));

  childNode3.appendChild(dom.createTextNode(''));
  childNode3.appendChild(dom.createTextNode('Three'));
}

function setUpAssertRangeBetweenText() {
  // Create the following structure: <[01]><[]><[23]>
  // Where <> delimits spans, [] delimits text nodes, 01 and 23 are text.
  // We will test all 10 positions in between 0 and 2. All should pass.
  first = dom.createDom(TagName.SPAN, null, '01');
  middle = dom.createElement(TagName.SPAN);
  const emptyTextNode = dom.createTextNode('');
  dom.appendChild(middle, emptyTextNode);
  last = dom.createDom(TagName.SPAN, null, '23');
  dom.appendChild(root, first);
  dom.appendChild(root, middle);
  dom.appendChild(root, last);
}

function createFakeRange(
    startNode, startOffset, endNode = undefined, endOffset = undefined) {
  endNode = endNode || startNode;
  endOffset = endOffset || startOffset;
  return {
    getStartNode: functions.constant(startNode),
    getStartOffset: functions.constant(startOffset),
    getEndNode: functions.constant(endNode),
    getEndOffset: functions.constant(endOffset),
  };
}

testSuite({
  setUpPage() {
    root = dom.getElement('root');
  },

  tearDown() {
    dom.removeChildren(root);
  },

  testGetNextNonEmptyTextNode() {
    setUpNonEmptyTests();

    const nodeOne = editorDom.getNextNonEmptyTextNode(parentNode);
    assertEquals(
        'Should have found the next non-empty text node', 'One',
        nodeOne.nodeValue);
    const nodeTwoA = editorDom.getNextNonEmptyTextNode(nodeOne);
    assertEquals(
        'Should have found the next non-empty text node', 'TwoA',
        nodeTwoA.nodeValue);
    const nodeTwoB = editorDom.getNextNonEmptyTextNode(nodeTwoA);
    assertEquals(
        'Should have found the next non-empty text node', 'TwoB',
        nodeTwoB.nodeValue);
    const nodeThree = editorDom.getNextNonEmptyTextNode(nodeTwoB);
    assertEquals(
        'Should have found the next non-empty text node', 'Three',
        nodeThree.nodeValue);
    const nodeNull = editorDom.getNextNonEmptyTextNode(nodeThree, parentNode);
    assertNull('Should not have found any non-empty text node', nodeNull);

    const nodeStop = editorDom.getNextNonEmptyTextNode(nodeOne, childNode1);
    assertNull('Should have stopped before finding a node', nodeStop);

    const nodeBeforeStop =
        editorDom.getNextNonEmptyTextNode(nodeTwoA, childNode2);
    assertEquals(
        'Should have found the next non-empty text node', 'TwoB',
        nodeBeforeStop.nodeValue);
  },

  testGetPreviousNonEmptyTextNode() {
    setUpNonEmptyTests();

    const nodeThree = editorDom.getPreviousNonEmptyTextNode(parentNode);
    assertEquals(
        'Should have found the previous non-empty text node', 'Three',
        nodeThree.nodeValue);
    const nodeTwoB = editorDom.getPreviousNonEmptyTextNode(nodeThree);
    assertEquals(
        'Should have found the previous non-empty text node', 'TwoB',
        nodeTwoB.nodeValue);
    const nodeTwoA = editorDom.getPreviousNonEmptyTextNode(nodeTwoB);
    assertEquals(
        'Should have found the previous non-empty text node', 'TwoA',
        nodeTwoA.nodeValue);
    const nodeOne = editorDom.getPreviousNonEmptyTextNode(nodeTwoA);
    assertEquals(
        'Should have found the previous non-empty text node', 'One',
        nodeOne.nodeValue);
    const nodeNull = editorDom.getPreviousNonEmptyTextNode(nodeOne, parentNode);
    assertNull('Should not have found any non-empty text node', nodeNull);

    const nodeStop =
        editorDom.getPreviousNonEmptyTextNode(nodeThree, childNode3);
    assertNull('Should have stopped before finding a node', nodeStop);

    const nodeBeforeStop =
        editorDom.getPreviousNonEmptyTextNode(nodeTwoB, childNode2);
    assertEquals(
        'Should have found the previous non-empty text node', 'TwoA',
        nodeBeforeStop.nodeValue);
  },

  testAssertRangeBetweenText0() {
    setUpAssertRangeBetweenText();
    editorDom.assertRangeBetweenText(
        '0', '1', createFakeRange(first.firstChild, 1));
  },

  testAssertRangeBetweenText1() {
    setUpAssertRangeBetweenText();
    editorDom.assertRangeBetweenText(
        '1', '2', createFakeRange(first.firstChild, 2));
  },

  testAssertRangeBetweenText2() {
    setUpAssertRangeBetweenText();
    editorDom.assertRangeBetweenText('1', '2', createFakeRange(first, 1));
  },

  testAssertRangeBetweenText3() {
    setUpAssertRangeBetweenText();
    editorDom.assertRangeBetweenText('1', '2', createFakeRange(root, 1));
  },

  testAssertRangeBetweenText4() {
    setUpAssertRangeBetweenText();
    editorDom.assertRangeBetweenText('1', '2', createFakeRange(middle, 0));
  },

  testAssertRangeBetweenText5() {
    setUpAssertRangeBetweenText();
    editorDom.assertRangeBetweenText(
        '1', '2', createFakeRange(middle.firstChild, 0));
  },

  testAssertRangeBetweenText6() {
    setUpAssertRangeBetweenText();
    editorDom.assertRangeBetweenText('1', '2', createFakeRange(middle, 1));
  },

  testAssertRangeBetweenText7() {
    setUpAssertRangeBetweenText();
    editorDom.assertRangeBetweenText('1', '2', createFakeRange(root, 2));
  },

  testAssertRangeBetweenText8() {
    setUpAssertRangeBetweenText();
    editorDom.assertRangeBetweenText('1', '2', createFakeRange(last, 0));
  },

  testAssertRangeBetweenText9() {
    setUpAssertRangeBetweenText();
    editorDom.assertRangeBetweenText(
        '1', '2', createFakeRange(last.firstChild, 0));
  },

  testAssertRangeBetweenTextBefore() {
    setUpAssertRangeBetweenText();
    // Test that it works when the cursor is at the beginning of all text.
    editorDom.assertRangeBetweenText(
        '', '0', createFakeRange(first.firstChild, 0),
        root);  // Restrict to root div so it won't find /n's and script.
  },

  testAssertRangeBetweenTextAfter() {
    setUpAssertRangeBetweenText();
    // Test that it works when the cursor is at the end of all text.
    editorDom.assertRangeBetweenText(
        '3', '', createFakeRange(last.firstChild, 2),
        root);  // Restrict to root div so it won't find /n's and script.
  },

  testAssertRangeBetweenTextFail1() {
    setUpAssertRangeBetweenText();
    const e = assertThrowsJsUnitException(() => {
      editorDom.assertRangeBetweenText(
          '1', '3', createFakeRange(first.firstChild, 2));
    });
    assertContains(
        'Assert reason incorrect', 'Expected <3> after range but found <23>',
        e.message);
  },

  testAssertRangeBetweenTextFail2() {
    setUpAssertRangeBetweenText();
    const e = assertThrowsJsUnitException(() => {
      editorDom.assertRangeBetweenText(
          '1', '2', createFakeRange(first.firstChild, 2, last.firstChild, 1));
    });
    assertContains(
        'Assert reason incorrect', 'Expected <2> after range but found <3>',
        e.message);
  },

  testAssertRangeBetweenTextBeforeFail() {
    setUpAssertRangeBetweenText();
    // Test that it gives the right message when the cursor is at the beginning
    // of all text but you're expecting something before it.
    const e = assertThrowsJsUnitException(() => {
      editorDom.assertRangeBetweenText(
          '-1', '0', createFakeRange(first.firstChild, 0),
          root);  // Restrict to root div so it won't find /n's and script.
    });
    assertContains(
        'Assert reason incorrect',
        'Expected <-1> before range but found nothing', e.message);
  },

  testAssertRangeBetweenTextAfterFail() {
    setUpAssertRangeBetweenText();
    // Test that it gives the right message when the cursor is at the end
    // of all text but you're expecting something after it.
    const e = assertThrowsJsUnitException(() => {
      editorDom.assertRangeBetweenText(
          '3', '4', createFakeRange(last.firstChild, 2),
          root);  // Restrict to root div so it won't find /n's and script.
    });
    assertContains(
        'Assert reason incorrect', 'Expected <4> after range but found nothing',
        e.message);
  },
});
