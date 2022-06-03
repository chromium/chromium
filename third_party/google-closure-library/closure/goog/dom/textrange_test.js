/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.TextRangeTest');
goog.setTestOnly();

const Coordinate = goog.require('goog.math.Coordinate');
const DomControlRange = goog.require('goog.dom.ControlRange');
const DomTextRange = goog.require('goog.dom.TextRange');
const ExpectedFailures = goog.require('goog.testing.ExpectedFailures');
const Range = goog.require('goog.dom.Range');
const dom = goog.require('goog.dom');
const product = goog.require('goog.userAgent.product');
const style = goog.require('goog.style');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

let logo;
let logo2;
let logo3;
let logo3Rtl;
let table;
let table2;
let table2div;
let test3;
let test3Rtl;
let expectedFailures;

function getTest3ElementTopLeft() {
  const topLeft = style.getPageOffset(test3.firstChild);

  if (userAgent.EDGE_OR_IE) {
    // On IE the selection is as tall as its tallest element.
    const logoPosition = style.getPageOffset(logo3);
    topLeft.y = logoPosition.y;
  }
  return topLeft;
}

function getTest3ElementBottomRight() {
  const pageOffset = style.getPageOffset(test3.lastChild);
  const bottomRight = new Coordinate(
      pageOffset.x + test3.lastChild.offsetWidth,
      pageOffset.y + test3.lastChild.offsetHeight);

  return bottomRight;
}

testSuite({
  setUpPage() {
    logo = dom.getElement('logo');
    logo2 = dom.getElement('logo2');
    logo3 = dom.getElement('logo3');
    logo3Rtl = dom.getElement('logo3Rtl');
    table = dom.getElement('table');
    table2 = dom.getElement('table2');
    table2div = dom.getElement('table2div');
    test3 = dom.getElement('test3');
    test3Rtl = dom.getElement('test3Rtl');
    expectedFailures = new ExpectedFailures();
  },

  tearDown() {
    expectedFailures.handleTearDown();
  },

  testCreateFromNodeContents() {
    assertNotNull(
        'Text range object can be created for element node',
        DomTextRange.createFromNodeContents(logo));
    assertNotNull(
        'Text range object can be created for text node',
        DomTextRange.createFromNodeContents(logo2.previousSibling));
  },

  testMoveToNodes() {
    const range = DomTextRange.createFromNodeContents(table2);
    range.moveToNodes(table2div, 0, table2div, 1, false);
    assertEquals(
        'Range should start in table2div', table2div, range.getStartNode());
    assertEquals(
        'Range should end in table2div', table2div, range.getEndNode());
    assertEquals('Range start offset should be 0', 0, range.getStartOffset());
    assertEquals('Range end offset should be 0', 1, range.getEndOffset());
    assertFalse('Range should not be reversed', range.isReversed());
    range.moveToNodes(table2div, 0, table2div, 1, true);
    assertTrue('Range should be reversed', range.isReversed());
    assertEquals('Range text should be "foo"', 'foo', range.getText());
  },

  testContainsTextRange() {
    let range = DomTextRange.createFromNodeContents(table2);
    let range2 = DomTextRange.createFromNodeContents(table2div);
    assertTrue(
        'TextRange contains other TextRange', range.containsRange(range2));
    assertFalse(
        'TextRange does not contain other TextRange',
        range2.containsRange(range));

    range =
        Range.createFromNodes(table2div.firstChild, 1, table2div.lastChild, 1);
    range2 = DomTextRange.createFromNodes(
        table2div.firstChild, 0, table2div.lastChild, 0);
    assertTrue(
        'TextRange partially contains other TextRange',
        range2.containsRange(range, true));
    assertFalse(
        'TextRange does not fully contain other TextRange',
        range2.containsRange(range, false));
  },

  testContainsControlRange() {
    if (userAgent.IE) {
      let range = DomControlRange.createFromElements(table2);
      let range2 = DomTextRange.createFromNodeContents(table2div);
      assertFalse(
          'TextRange does not contain ControlRange',
          range2.containsRange(range));
      range = DomControlRange.createFromElements(logo2);
      assertTrue(
          'TextRange contains ControlRange', range2.containsRange(range));
      range = DomTextRange.createFromNodeContents(table2);
      range2 = DomControlRange.createFromElements(logo, logo2);
      assertTrue(
          'TextRange partially contains ControlRange',
          range2.containsRange(range, true));
      assertFalse(
          'TextRange does not fully contain ControlRange',
          range2.containsRange(range, false));
    }
  },

  testGetStartPosition() {
    // The start node is in the top left.
    const range = DomTextRange.createFromNodeContents(test3);

    try {
      const result = assertNotThrows(goog.bind(range.getStartPosition, range));
      assertObjectRoughlyEquals(getTest3ElementTopLeft(), result, 1);
    } catch (e) {
      expectedFailures.handleException(e);
    }
  },

  testGetStartPositionNotInDocument() {
    const range = DomTextRange.createFromNodeContents(test3);

    dom.removeNode(test3);
    try {
      const result = assertNotThrows(goog.bind(range.getStartPosition, range));
      assertNull(result);
    } catch (e) {
      expectedFailures.handleException(e);
    } finally {
      dom.appendChild(document.body, test3);
    }
  },

  testGetStartPositionReversed() {
    // Simulate the user selecting backwards from right-to-left.
    // The start node is now in the bottom right.
    const firstNode = test3.firstChild.firstChild;
    const lastNode = test3.lastChild.lastChild;
    const range = DomTextRange.createFromNodes(
        lastNode, lastNode.nodeValue.length, firstNode, 0);

    try {
      const result = assertNotThrows(goog.bind(range.getStartPosition, range));
      assertObjectRoughlyEquals(getTest3ElementTopLeft(), result, 1);
    } catch (e) {
      expectedFailures.handleException(e);
    }
  },

  testGetStartPositionRightToLeft() {
    if (product.SAFARI) {
      // TODO(user): Disabled so we can get the rest of the Closure test
      // suite running in a continuous build. Will investigate later.
      return;
    }

    // Even in RTL content the start node is still in the top left.
    const range = DomTextRange.createFromNodeContents(test3Rtl);
    const topLeft = style.getPageOffset(test3Rtl.firstChild);

    if (userAgent.EDGE_OR_IE) {
      // On IE the selection is as tall as its tallest element.
      const logoPosition = style.getPageOffset(logo3Rtl);
      topLeft.y = logoPosition.y;
    }

    try {
      const result = assertNotThrows(goog.bind(range.getStartPosition, range));
      assertObjectRoughlyEquals(topLeft, result, 0.1);
    } catch (e) {
      expectedFailures.handleException(e);
    }
  },

  testGetEndPosition() {
    // The end node is in the bottom right.
    const range = DomTextRange.createFromNodeContents(test3);
    const expected = getTest3ElementBottomRight();

    try {
      const result = assertNotThrows(goog.bind(range.getEndPosition, range));
      assertObjectRoughlyEquals(expected, result, 1);
    } catch (e) {
      expectedFailures.handleException(e);
    }
  },

  testGetEndPositionNotInDocument() {
    const range = DomTextRange.createFromNodeContents(test3);

    dom.removeNode(test3);
    try {
      const result = assertNotThrows(goog.bind(range.getEndPosition, range));
      assertNull(result);
    } catch (e) {
      expectedFailures.handleException(e);
    } finally {
      dom.appendChild(document.body, test3);
    }
  },

  testGetEndPositionReversed() {
    // Simulate the user selecting backwards from right-to-left.
    // The end node is still in the lower right.
    const range = DomTextRange.createFromNodeContents(test3, true);
    const expected = getTest3ElementBottomRight();

    try {
      const result = assertNotThrows(goog.bind(range.getEndPosition, range));

      // For some reason, ie7 is further off than other browsers.
      const estimate = 1;
      assertObjectRoughlyEquals(expected, result, estimate);
    } catch (e) {
      expectedFailures.handleException(e);
    }
  },

  testGetEndPositionRightToLeft() {
    // Even in RTL content the end node is still in the bottom right.
    const range = DomTextRange.createFromNodeContents(test3Rtl);
    const pageOffset = style.getPageOffset(test3Rtl.lastChild);
    const bottomRight = new Coordinate(
        pageOffset.x + test3Rtl.lastChild.offsetWidth,
        pageOffset.y + test3Rtl.lastChild.offsetHeight);



    try {
      const result = assertNotThrows(goog.bind(range.getEndPosition, range));
      assertObjectRoughlyEquals(bottomRight, result, 1);
    } catch (e) {
      expectedFailures.handleException(e);
    }
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testCloneRangeDeep() {
    const range = DomTextRange.createFromNodeContents(logo);
    assertFalse(range.isCollapsed());

    const cloned = range.clone();
    cloned.collapse();
    assertTrue(cloned.isCollapsed());
    assertFalse(range.isCollapsed());
  },
});
