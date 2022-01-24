/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.dom.ControlRangeTest');
goog.setTestOnly();

const DomControlRange = goog.require('goog.dom.ControlRange');
const DomTextRange = goog.require('goog.dom.TextRange');
const RangeType = goog.require('goog.dom.RangeType');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');
const testingDom = goog.require('goog.testing.dom');
const userAgent = goog.require('goog.userAgent');

let logo;
let table;

function helpTestBounds(range) {
  assertEquals('Start node is logo', logo, range.getStartNode());
  assertEquals('Start offset is 0', 0, range.getStartOffset());
  assertEquals('End node is table', table, range.getEndNode());
  assertEquals('End offset is 1', 1, range.getEndOffset());
}

testSuite({
  setUpPage() {
    logo = dom.getElement('logo');
    table = dom.getElement('table');
  },

  testCreateFromElement() {
    if (!userAgent.IE) {
      return;
    }
    assertNotNull(
        'Control range object can be created for element',
        DomControlRange.createFromElements(logo));
  },

  testCreateFromRange() {
    if (!userAgent.IE) {
      return;
    }
    const range = document.body.createControlRange();
    range.addElement(table);
    assertNotNull(
        'Control range object can be created for element',
        DomControlRange.createFromBrowserRange(range));
  },

  /** @suppress {missingProperties} suppression added to enable type checking */
  testSelect() {
    if (!userAgent.IE || userAgent.isVersionOrHigher('11')) {
      return;
    }

    const range = DomControlRange.createFromElements(table);
    range.select();

    assertEquals(
        'Control range should be selected', 'Control', document.selection.type);
    assertEquals(
        'Control range should have length 1', 1,
        document.selection.createRange().length);
    assertEquals(
        'Control range should select table', table,
        document.selection.createRange().item(0));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testControlRangeIterator() {
    if (!userAgent.IE) {
      return;
    }
    const range = DomControlRange.createFromElements(logo, table);
    // Each node is included twice - once as a start tag, once as an end.
    testingDom.assertNodesMatch(range, [
      '#logo', '#logo', '#table', '#tbody', '#tr1',   '#td11',  'a', '#td11',
      '#td12', 'b',     '#td12',  '#tr1',   '#tr2',   '#td21',  'c', '#td21',
      '#td22', 'd',     '#td22',  '#tr2',   '#tbody', '#table',
    ]);
  },

  testBounds() {
    if (!userAgent.IE) {
      return;
    }

    // Initialize in both orders.
    helpTestBounds(DomControlRange.createFromElements(logo, table));
    helpTestBounds(DomControlRange.createFromElements(table, logo));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testCollapse() {
    if (!userAgent.IE) {
      return;
    }

    const range = DomControlRange.createFromElements(logo, table);
    assertFalse('Not initially collapsed', range.isCollapsed());
    range.collapse();
    assertTrue('Successfully collapsed', range.isCollapsed());
  },

  testGetContainer() {
    if (!userAgent.IE) {
      return;
    }

    let range = DomControlRange.createFromElements(logo);
    assertEquals(
        'Single element range is contained by itself', logo,
        range.getContainer());

    range = DomControlRange.createFromElements(logo, table);
    assertEquals(
        'Two element range is contained by body', document.body,
        range.getContainer());
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testSave() {
    if (!userAgent.IE) {
      return;
    }

    let range = DomControlRange.createFromElements(logo, table);
    const savedRange = range.saveUsingDom();

    range.collapse();
    assertTrue('Successfully collapsed', range.isCollapsed());

    range = savedRange.restore();
    assertEquals(
        'Restored a control range', RangeType.CONTROL, range.getType());
    assertFalse('Not collapsed after restore', range.isCollapsed());
    helpTestBounds(range);
  },

  testRemoveContents() {
    if (!userAgent.IE) {
      return;
    }

    const img = dom.createDom(TagName.IMG);
    /**
     * @suppress {strictMissingProperties} suppression added to enable type
     * checking
     */
    img.src = logo.src;

    const div = dom.getElement('test1');
    dom.removeChildren(div);
    div.appendChild(img);
    assertEquals('Div has 1 child', 1, div.childNodes.length);

    const range = DomControlRange.createFromElements(img);
    range.removeContents();
    assertEquals('Div has 0 children', 0, div.childNodes.length);
    assertTrue('Range is collapsed', range.isCollapsed());
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testReplaceContents() {
    // Test a control range.
    if (!userAgent.IE) {
      return;
    }

    const outer = dom.getElement('test1');
    outer.innerHTML = '<div contentEditable="true">' +
        'Hello <input type="text" value="World">' +
        '</div>';
    const range = DomControlRange.createFromElements(
        dom.getElementsByTagName(TagName.INPUT, outer)[0]);
    DomControlRange.createFromElements(table);
    range.replaceContentsWithNode(dom.createTextNode('World'));
    assertEquals('Hello World', outer.firstChild.innerHTML);
  },

  testContainsRange() {
    if (!userAgent.IE) {
      return;
    }

    const table2 = dom.getElement('table2');
    const table2td = dom.getElement('table2td');
    const logo2 = dom.getElement('logo2');

    let range = DomControlRange.createFromElements(logo, table);
    let range2 = DomControlRange.createFromElements(logo);
    assertTrue(
        'Control range contains the other control range',
        range.containsRange(range2));
    assertTrue(
        'Control range partially contains the other control range',
        range2.containsRange(range, true));

    range2 = DomControlRange.createFromElements(table2);
    assertFalse(
        'Control range does not contain the other control range',
        range.containsRange(range2));

    range = DomControlRange.createFromElements(table2);
    range2 = DomTextRange.createFromNodeContents(table2td);
    assertTrue(
        'Control range contains text range', range.containsRange(range2));

    range2 = DomTextRange.createFromNodeContents(table);
    assertFalse(
        'Control range does not contain text range',
        range.containsRange(range2));

    range = DomControlRange.createFromElements(logo2);
    range2 = DomTextRange.createFromNodeContents(table2);
    assertFalse(
        'Control range does not fully contain text range',
        range.containsRange(range2, false));

    range2 = DomControlRange.createFromElements(table2);
    assertTrue(
        'Control range contains the other control range (2)',
        range2.containsRange(range));
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testCloneRange() {
    if (!userAgent.IE) {
      return;
    }
    const range = DomControlRange.createFromElements(logo);
    assertNotNull('Control range object created for element', range);

    const cloneRange = range.clone();
    assertNotNull('Cloned control range object', cloneRange);
    assertArrayEquals(
        'Control range and clone have same elements', range.getElements(),
        cloneRange.getElements());
  },
});
