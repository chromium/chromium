/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.editor.plugins.TableEditorTest');
goog.setTestOnly();

const ExpectedFailures = goog.require('goog.testing.ExpectedFailures');
const FieldMock = goog.require('goog.testing.editor.FieldMock');
const Range = goog.require('goog.dom.Range');
const TableEditor = goog.require('goog.editor.plugins.TableEditor');
const TagName = goog.require('goog.dom.TagName');
const TestCase = goog.require('goog.testing.TestCase');
const TestHelper = goog.require('goog.testing.editor.TestHelper');
const dom = goog.require('goog.dom');
const googObject = goog.require('goog.object');
const googString = goog.require('goog.string');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

let field;
let plugin;
let fieldMock;
let expectedFailures;
let testHelper;

/**
 * Helper routine which returns the number of cells in the table.
 * @param {Element} table The table in question.
 * @return {number} Number of cells.
 * @suppress {strictMissingProperties} suppression added to enable type checking
 */
function getCellCount(table) {
  return table.cells ? table.cells.length :
                       table.rows[0].cells.length * table.rows.length;
}

/**
 * Helper method which creates a table and puts the cursor on the first TD.
 * In IE, the cursor isn't positioned in the first cell (TD) and we simulate
 * that behavior explicitly to be consistent across all browsers.
 * @param {Object} op_tableProps Optional table properties.
 * @suppress {checkTypes,visibility} suppression added to enable type checking
 */
function createTableAndSelectCell(tableProps = undefined) {
  Range.createCaret(field, 1).select();
  plugin.execCommandInternal(TableEditor.COMMAND.TABLE, tableProps);
  if (userAgent.IE) {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const range = Range.createFromNodeContents(
        dom.getElementsByTagName(TagName.TD, field)[0]);
    range.select();
  }
}
testSuite({
  setUpPage() {
    field = dom.getElement('field');
    expectedFailures = new ExpectedFailures();
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  setUp() {
    testHelper = new TestHelper(dom.getElement('field'));
    testHelper.setUpEditableElement();
    field.focus();
    plugin = new TableEditor();
    fieldMock = new FieldMock();
    plugin.registerFieldObject(fieldMock);
    if (userAgent.IE && (userAgent.compare(userAgent.VERSION, '7.0') >= 0)) {
      TestCase.protectedTimeout_ = window.setTimeout;
    }
  },

  tearDown() {
    testHelper.tearDownEditableElement();
    expectedFailures.handleTearDown();
  },

  testEnable() {
    fieldMock.$replay();

    plugin.enable(fieldMock);
    assertTrue('Plugin should be enabled', plugin.isEnabled(fieldMock));

    fieldMock.$verify();
  },

  testIsSupportedCommand() {
    googObject.forEach(TableEditor.COMMAND, (command) => {
      assertTrue(
          googString.subs('Plugin should support %s', command),
          plugin.isSupportedCommand(command));
    });
    assertFalse(
        'Plugin shouldn\'t support a bogus command',
        plugin.isSupportedCommand('+fable'));
  },

  testCreateTable() {
    fieldMock.$replay();
    createTableAndSelectCell();
    /** @suppress {visibility} suppression added to enable type checking */
    const table = plugin.getCurrentTable_();
    assertNotNull('Table should not be null', table);
    assertEquals(
        'Table should have the default number of rows', 2, table.rows.length);
    assertEquals(
        'Table should have the default number of cells', 8,
        getCellCount(table));
    fieldMock.$verify();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testInsertRowBefore() {
    fieldMock.$replay();
    createTableAndSelectCell();
    /** @suppress {visibility} suppression added to enable type checking */
    const table = plugin.getCurrentTable_();
    /**
     * @suppress {missingProperties} suppression added to enable type checking
     */
    const selectedRow = fieldMock.getRange().getContainerElement().parentNode;
    assertNull(
        'Selected row shouldn\'t have a previous sibling',
        selectedRow.previousSibling);
    assertEquals('Table should have two rows', 2, table.rows.length);
    plugin.execCommandInternal(TableEditor.COMMAND.INSERT_ROW_BEFORE);
    assertEquals('A row should have been inserted', 3, table.rows.length);

    // Assert that we inserted a row above the currently selected row.
    assertNotNull(
        'Selected row should have a previous sibling',
        selectedRow.previousSibling);
    fieldMock.$verify();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testInsertRowAfter() {
    fieldMock.$replay();
    createTableAndSelectCell({width: 2, height: 1});
    /**
     * @suppress {missingProperties} suppression added to enable type checking
     */
    const selectedRow = fieldMock.getRange().getContainerElement().parentNode;
    /** @suppress {visibility} suppression added to enable type checking */
    const table = plugin.getCurrentTable_();
    assertEquals('Table should have one row', 1, table.rows.length);
    assertNull(
        'Selected row shouldn\'t have a next sibling', selectedRow.nextSibling);
    plugin.execCommandInternal(TableEditor.COMMAND.INSERT_ROW_AFTER);
    assertEquals('A row should have been inserted', 2, table.rows.length);
    // Assert that we inserted a row after the currently selected row.
    assertNotNull(
        'Selected row should have a next sibling', selectedRow.nextSibling);
    fieldMock.$verify();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testInsertRowAfterDeeplyNestedCell() {
    fieldMock.$replay();
    createTableAndSelectCell({width: 2, height: 1});

    // Add two nested divs with text to the first cell.
    /** @suppress {checkTypes} suppression added to enable type checking */
    const firstCell = dom.getElementsByTagName(TagName.TD, field)[0];
    const div1 = dom.createElement(TagName.DIV);
    const div2 = dom.createElement(TagName.DIV);
    const text = dom.createTextNode('Some text');
    firstCell.appendChild(div1);
    div1.appendChild(div2);
    div2.appendChild(text);

    // Change the selection to select the text in the cell.
    Range.createCaret(text, 1).select();

    /** @suppress {checkTypes} suppression added to enable type checking */
    const selectedRow = dom.getElementsByTagName(TagName.TR, field)[0];

    /** @suppress {visibility} suppression added to enable type checking */
    const table = plugin.getCurrentTable_();
    assertEquals('Table should have one row', 1, table.rows.length);
    assertNull(
        'Selected row shouldn\'t have a next sibling', selectedRow.nextSibling);
    plugin.execCommandInternal(TableEditor.COMMAND.INSERT_ROW_AFTER);
    assertEquals('A row should have been inserted', 2, table.rows.length);
    // Assert that we inserted a row after the currently selected row.
    assertNotNull(
        'Selected row should have a next sibling', selectedRow.nextSibling);
    fieldMock.$verify();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testInsertColumnBefore() {
    fieldMock.$replay();
    createTableAndSelectCell({width: 1, height: 1});
    /** @suppress {visibility} suppression added to enable type checking */
    const table = plugin.getCurrentTable_();
    /**
     * @suppress {missingProperties} suppression added to enable type checking
     */
    const selectedCell = fieldMock.getRange().getContainerElement();
    assertEquals('Table should have one cell', 1, getCellCount(table));
    assertNull(
        'Selected cell shouldn\'t have a previous sibling',
        selectedCell.previousSibling);
    plugin.execCommandInternal(TableEditor.COMMAND.INSERT_COLUMN_BEFORE);
    assertEquals('A cell should have been inserted', 2, getCellCount(table));
    assertNotNull(
        'Selected cell should have a previous sibling',
        selectedCell.previousSibling);
    fieldMock.$verify();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testInsertColumnAfter() {
    fieldMock.$replay();
    createTableAndSelectCell({width: 1, height: 1});
    /** @suppress {visibility} suppression added to enable type checking */
    const table = plugin.getCurrentTable_();
    /**
     * @suppress {missingProperties} suppression added to enable type checking
     */
    const selectedCell = fieldMock.getRange().getContainerElement();
    assertEquals('Table should have one cell', 1, getCellCount(table));
    assertNull(
        'Selected cell shouldn\'t have a next sibling',
        selectedCell.nextSibling);
    plugin.execCommandInternal(TableEditor.COMMAND.INSERT_COLUMN_AFTER);
    assertEquals('A cell should have been inserted', 2, getCellCount(table));
    assertNotNull(
        'Selected cell should have a next sibling', selectedCell.nextSibling);
    fieldMock.$verify();
  },

  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testRemoveRows() {
    fieldMock.$replay();
    createTableAndSelectCell({width: 1, height: 2});
    /** @suppress {visibility} suppression added to enable type checking */
    const table = plugin.getCurrentTable_();
    /**
     * @suppress {missingProperties} suppression added to enable type checking
     */
    const selectedCell = fieldMock.getRange().getContainerElement();
    selectedCell.id = 'selected';
    assertEquals('Table should have two rows', 2, table.rows.length);
    plugin.execCommandInternal(TableEditor.COMMAND.REMOVE_ROWS);
    assertEquals('A row should have been removed', 1, table.rows.length);
    assertNull(
        'The correct row should have been removed', dom.getElement('selected'));

    // Verify that the table is removed if we don't have any rows.
    plugin.execCommandInternal(TableEditor.COMMAND.REMOVE_ROWS);
    assertEquals(
        'The table should have been removed', 0,
        dom.getElementsByTagName(TagName.TABLE, field).length);
    fieldMock.$verify();
  },

  /**
     @suppress {visibility,checkTypes} suppression added to enable type
     checking
   */
  testRemoveColumns() {
    fieldMock.$replay();
    createTableAndSelectCell({width: 2, height: 1});
    /** @suppress {visibility} suppression added to enable type checking */
    const table = plugin.getCurrentTable_();
    /**
     * @suppress {missingProperties} suppression added to enable type checking
     */
    const selectedCell = fieldMock.getRange().getContainerElement();
    selectedCell.id = 'selected';
    assertEquals('Table should have two cells', 2, getCellCount(table));
    plugin.execCommandInternal(TableEditor.COMMAND.REMOVE_COLUMNS);
    assertEquals('A cell should have been removed', 1, getCellCount(table));
    assertNull(
        'The correct cell should have been removed',
        dom.getElement('selected'));

    // Verify that the table is removed if we don't have any columns.
    plugin.execCommandInternal(TableEditor.COMMAND.REMOVE_COLUMNS);
    assertEquals(
        'The table should have been removed', 0,
        dom.getElementsByTagName(TagName.TABLE, field).length);
    fieldMock.$verify();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testSplitCell() {
    fieldMock.$replay();
    createTableAndSelectCell({width: 1, height: 1});
    /** @suppress {visibility} suppression added to enable type checking */
    const table = plugin.getCurrentTable_();
    /**
     * @suppress {missingProperties} suppression added to enable type checking
     */
    const selectedCell = fieldMock.getRange().getContainerElement();
    // Splitting is only supported if we set these attributes.
    selectedCell.rowSpan = '1';
    selectedCell.colSpan = '2';
    dom.setTextContent(selectedCell, 'foo');
    Range.createFromNodeContents(selectedCell).select();
    assertEquals('Table should have one cell', 1, getCellCount(table));
    plugin.execCommandInternal(TableEditor.COMMAND.SPLIT_CELL);
    assertEquals('The cell should have been split', 2, getCellCount(table));
    assertEquals(
        'The cell content should be intact', 'foo', selectedCell.innerHTML);
    assertNotNull(
        'The new cell should be inserted before', selectedCell.previousSibling);
    fieldMock.$verify();
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testMergeCells() {
    fieldMock.$replay();
    createTableAndSelectCell({width: 2, height: 1});
    /** @suppress {visibility} suppression added to enable type checking */
    const table = plugin.getCurrentTable_();
    /**
     * @suppress {missingProperties} suppression added to enable type checking
     */
    const selectedCell = fieldMock.getRange().getContainerElement();
    dom.setTextContent(selectedCell, 'foo');
    dom.setTextContent(selectedCell.nextSibling, 'bar');
    const range = Range.createFromNodeContents(
        dom.getElementsByTagName(TagName.TR, table)[0]);
    range.select();
    plugin.execCommandInternal(TableEditor.COMMAND.MERGE_CELLS);
    expectedFailures.expectFailureFor(userAgent.IE);
    try {
      // In IE8, even after explicitly setting the range to span
      // multiple cells, the browser selection only contains the first TD
      // which causes the merge operation to fail.
      assertEquals('The cells should be merged', 1, getCellCount(table));
      assertEquals(
          'The cell should have expected colspan', 2, selectedCell.colSpan);
      assertHTMLEquals(
          'The content should be merged', 'foo bar', selectedCell.innerHTML);
    } catch (e) {
      expectedFailures.handleException(e);
    }
    fieldMock.$verify();
  },
});
