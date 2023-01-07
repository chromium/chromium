/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.TableSorterTest');
goog.setTestOnly();

const TableSorter = goog.require('goog.ui.TableSorter');
const TagName = goog.require('goog.dom.TagName');
const classlist = goog.require('goog.dom.classlist');
const dom = goog.require('goog.dom');
const events = goog.require('goog.testing.events');
const testSuite = goog.require('goog.testing.testSuite');

let oldHtml;
let alphaHeader;
let numberHeader;
let notSortableHeader;
let table;
let tableSorter;

function assertOrder(arr, opt_table) {
  const tbl = opt_table || table;
  const actual = [];
  Array.prototype.forEach.call(
      dom.getElementsByTagName(TagName.TD, tbl), (td, idx) => {
        const txt = dom.getTextContent(td);
        if (txt) {
          actual.push(txt);
        }
      });
  assertArrayEquals(arr, actual);
}
testSuite({
  setUpPage() {
    oldHtml = dom.getElement('content').innerHTML;
  },

  setUp() {
    dom.getElement('content').innerHTML = oldHtml;
    table = dom.getElement('sortable');
    /** @suppress {checkTypes} suppression added to enable type checking */
    alphaHeader = dom.getElementsByTagName(TagName.TH, table)[0];
    /** @suppress {checkTypes} suppression added to enable type checking */
    numberHeader = dom.getElementsByTagName(TagName.TH, table)[1];
    /** @suppress {checkTypes} suppression added to enable type checking */
    notSortableHeader = dom.getElementsByTagName(TagName.TH, table)[2];

    tableSorter = new TableSorter();
    tableSorter.setSortFunction(0, TableSorter.alphaSort);
    tableSorter.setSortFunction(2, TableSorter.noSort);
    tableSorter.decorate(table);
  },

  tearDown() {
    tableSorter.dispose();
    table = null;
  },

  testConstructor() {
    assertNotNull('Should have successful construction', tableSorter);
    assertNotNull('Should be in document', tableSorter);
  },

  testForwardAlpha() {
    events.fireClickEvent(alphaHeader);
    assertOrder(['A', '10', 'B', '0', 'C', '10', 'C', '17', 'C', '3']);
    assertTrue(classlist.contains(alphaHeader, 'goog-tablesorter-sorted'));
    assertEquals(0, tableSorter.getSortColumn());
    assertFalse(tableSorter.isSortReversed());
  },

  testBackwardAlpha() {
    events.fireClickEvent(alphaHeader);
    events.fireClickEvent(alphaHeader);
    assertOrder(['C', '10', 'C', '17', 'C', '3', 'B', '0', 'A', '10']);
    assertFalse(classlist.contains(alphaHeader, 'goog-tablesorter-sorted'));
    assertTrue(
        classlist.contains(alphaHeader, 'goog-tablesorter-sorted-reverse'));
    assertEquals(0, tableSorter.getSortColumn());
    assertTrue(tableSorter.isSortReversed());
  },

  testForwardNumeric() {
    events.fireClickEvent(numberHeader);
    assertOrder(['B', '0', 'C', '3', 'C', '10', 'A', '10', 'C', '17']);
    assertTrue(classlist.contains(numberHeader, 'goog-tablesorter-sorted'));
    assertEquals(1, tableSorter.getSortColumn());
    assertFalse(tableSorter.isSortReversed());
  },

  testBackwardNumeric() {
    events.fireClickEvent(numberHeader);
    events.fireClickEvent(numberHeader);
    assertOrder(['C', '17', 'C', '10', 'A', '10', 'C', '3', 'B', '0']);
    assertTrue(
        classlist.contains(numberHeader, 'goog-tablesorter-sorted-reverse'));
    assertEquals(1, tableSorter.getSortColumn());
    assertTrue(tableSorter.isSortReversed());
  },

  testAlphaThenNumeric() {
    this.testForwardAlpha();
    events.fireClickEvent(numberHeader);
    assertOrder(['B', '0', 'C', '3', 'A', '10', 'C', '10', 'C', '17']);
    assertFalse(classlist.contains(alphaHeader, 'goog-tablesorter-sorted'));
    assertEquals(1, tableSorter.getSortColumn());
    assertFalse(tableSorter.isSortReversed());
  },

  testNotSortableUnchanged() {
    events.fireClickEvent(notSortableHeader);
    assertEquals(0, classlist.get(notSortableHeader).length);
    assertEquals(-1, tableSorter.getSortColumn());
  },

  testSortWithNonDefaultSortableHeaderRowIndex() {
    // Check that clicking on non-sortable header doesn't trigger any sorting.
    assertOrder(['C', '10', 'A', '10', 'C', '17', 'B', '0', 'C', '3']);
    events.fireClickEvent(dom.getElement('not-sortable'));
    assertOrder(['C', '10', 'A', '10', 'C', '17', 'B', '0', 'C', '3']);
  },

  testsetSortableHeaderRowIndexAfterDecorateThrows() {
    const func = () => {
      tableSorter.setSortableHeaderRowIndex(0);
    };
    const msg = assertThrows('failFunc should throw.', func)['message'];
    assertEquals('Component already rendered', msg);
  },

  testSortOnSecondHeaderRow() {
    // Test a table with multiple table headers.
    // Using setSortableHeaderRowIndex one can specify table header columns to
    // use in sorting.
    const tableSorter2 = new TableSorter();
    tableSorter2.setSortableHeaderRowIndex(1);
    tableSorter2.decorate(dom.getElement('sortable-2'));

    // Initial order.
    assertOrder(
        ['4', '5', '6', '1', '2', '3', '3', '1', '9'],
        dom.getElement('sortable-2'));

    // Sort on first column.
    events.fireClickEvent(dom.getElement('sorttable-2-col-1'));
    assertOrder(
        ['1', '2', '3', '3', '1', '9', '4', '5', '6'],
        dom.getElement('sortable-2'));

    // Sort on second column.
    events.fireClickEvent(dom.getElement('sorttable-2-col-2'));
    assertOrder(
        ['3', '1', '9', '1', '2', '3', '4', '5', '6'],
        dom.getElement('sortable-2'));

    // Sort on third column.
    events.fireClickEvent(dom.getElement('sorttable-2-col-3'));
    assertOrder(
        ['1', '2', '3', '4', '5', '6', '3', '1', '9'],
        dom.getElement('sortable-2'));

    // Reverse sort on third column.
    events.fireClickEvent(dom.getElement('sorttable-2-col-3'));
    assertOrder(
        ['3', '1', '9', '4', '5', '6', '1', '2', '3'],
        dom.getElement('sortable-2'));

    tableSorter2.dispose();
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testSortAfterSwapping() {
    // First click
    events.fireClickEvent(alphaHeader);
    assertOrder(['A', '10', 'B', '0', 'C', '10', 'C', '17', 'C', '3']);
    assertEquals(0, tableSorter.getSortColumn());

    // Move first column to the end
    let r;
    for (let i = 0; (r = table.rows[i]); i++) {
      const cell = r.cells[0];
      cell.parentNode.appendChild(cell);
    }
    // Make sure the above worked as expected
    assertOrder(['10', 'A', '0', 'B', '10', 'C', '17', 'C', '3', 'C']);

    // Our column is now the second one
    assertEquals(2, tableSorter.getSortColumn());

    // Second click, should reverse
    tableSorter.setSortFunction(2, TableSorter.alphaSort);
    events.fireClickEvent(alphaHeader);
    assertOrder(['10', 'C', '17', 'C', '3', 'C', '0', 'B', '10', 'A']);
  },

  testTwoBodies() {
    const table3 = dom.getElement('sortable-3');
    const header = dom.getElement('sortable-3-col');
    const sorter3 = new TableSorter();
    sorter3.setSortFunction(0, TableSorter.alphaSort);
    try {
      sorter3.decorate(table3);
      events.fireClickEvent(header);
      assertOrder(['A', 'B', 'C', 'A', 'B', 'C'], table3);
      events.fireClickEvent(header);
      assertOrder(['C', 'B', 'A', 'C', 'B', 'A'], table3);
    } finally {
      sorter3.dispose();
    }
  },

  testNaNs() {
    const table = dom.getElement('sortable-4');
    const header = dom.getElement('sortable-4-col');
    const sorter = new TableSorter();
    try {
      // All non-numbers compare equal, i.e. Bar == Foo, so order of those
      // elements should not change (since we are using stable sort).
      sorter.decorate(table);
      events.fireClickEvent(header);
      assertOrder(['2', '3', '11', 'Bar', 'Foo'], table);
      events.fireClickEvent(header);
      assertOrder(['Bar', 'Foo', '11', '3', '2'], table);
    } finally {
      sorter.dispose();
    }
  },
});
