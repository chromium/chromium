/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.editor.TableTest');
goog.setTestOnly();

const Table = goog.require('goog.editor.Table');
const TagName = goog.require('goog.dom.TagName');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');

function tableSanityCheck(editableTable, rowCount, colCount) {
  assertEquals(
      'Table has expected number of rows', rowCount, editableTable.rows.length);
  let row;
  for (let i = 0; row = editableTable.rows[i]; i++) {
    assertEquals(
        `Row ${i} has expected number of columns`, colCount,
        row.columns.length);
  }
}


/** @suppress {missingProperties} suppression added to enable type checking */
function _testInsertRowResult(element, editableTable, newTr, index) {
  let originalRowCount;
  if (element == testElements.basic) {
    originalRowCount = 4;
  } else if (element == testElements.torture) {
    originalRowCount = 9;
  }

  assertEquals(
      'Row was added to table', originalRowCount + 1,
      dom.getElementsByTagName(TagName.TR, element).length);
  assertEquals(
      `Row was added at position ${index}`,
      dom.getElementsByTagName(TagName.TR, element)[index], newTr);
  assertEquals(
      'Row knows its own position', index, editableTable.rows[index].index);
  assertEquals(
      `EditableTable shows row at position ${index}`, newTr,
      editableTable.rows[index].element);
  assertEquals(
      'New row has correct number of TDs', 3,
      dom.getElementsByTagName(TagName.TD, newTr).length);
}

function _testInsertColumnResult(newCells, element, editableTable, index) {
  let row;
  for (let rowNo = 0; row = editableTable.rows[rowNo]; rowNo++) {
    assertEquals('Row includes new column', 4, row.columns.length);
  }
  assertEquals(
      'New cell in correct position', newCells[0],
      editableTable.rows[0].columns[index].element);
}

/** @suppress {missingProperties} suppression added to enable type checking */
function _testRemoveColumn(index) {
  /** @suppress {missingProperties} suppression added to enable type checking */
  let tr = dom.getElementsByTagName(TagName.TR, testElements.basic)[0];
  const sampleCell = dom.getElementsByTagName(TagName.TH, tr)[index];
  testObjects.basic.removeColumn(index);
  tableSanityCheck(testObjects.basic, 4, 2);
  /** @suppress {missingProperties} suppression added to enable type checking */
  tr = dom.getElementsByTagName(TagName.TR, testElements.basic)[0];
  assertNotEquals(
      'Test cell removed from column', sampleCell,
      dom.getElementsByTagName(TagName.TH, tr)[index]);
}


/*
  // TODO(user): write more unit tests for selection stuff.
  // The following code is left in here for reference in implementing
  // this TODO.

    var tds = goog.dom.getElementsByTagName(
        goog.dom.TagName.TD, goog.dom.getElement('test1'));
    var range = goog.dom.Range.createFromNodes(tds[7], tds[9]);
    range.select();
    var cellSelection = new goog.editor.Table.CellSelection(range);
    assertEquals(0, cellSelection.getFirstColumnIndex());
    assertEquals(2, cellSelection.getLastColumnIndex());
    assertEquals(2, cellSelection.getFirstRowIndex());
    assertEquals(2, cellSelection.getLastRowIndex());
    assertTrue(cellSelection.isRectangle());

    range = goog.dom.Range.createFromNodes(tds[7], tds[12]);
    range.select();
    var cellSelection2 = new goog.editor.Table.CellSelection(range);
    assertFalse(cellSelection2.isRectangle());
*/

let testElements;
let testObjects;

testSuite({
  setUp() {
    const inputTables = dom.getElementsByTagName(TagName.TABLE);
    testElements = {};
    testObjects = {};
    for (let i = 0; i < inputTables.length; i++) {
      const originalTable = inputTables[i];
      if (originalTable.id.substring(0, 5) == 'test-') {
        const tableName = originalTable.id.substring(5);
        const testTable = originalTable.cloneNode(true);
        testTable.id = tableName;
        testElements[tableName] = testTable;
        document.body.appendChild(testTable);
        testObjects[tableName] = new Table(testTable);
      }
    }
  },

  tearDown() {
    for (const tableName in testElements) {
      document.body.removeChild(testElements[tableName]);
      delete testElements[tableName];
      delete testObjects[tableName];
    }
    testElements = null;
    testObjects = null;
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testBasicTable() {
    // Do some basic sanity checking on the editable table structure
    tableSanityCheck(testObjects.basic, 4, 3);
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const originalRows =
        dom.getElementsByTagName(TagName.TR, testElements.basic);
    assertEquals(
        'Basic table row count, compared to source', originalRows.length,
        testObjects.basic.rows.length);
    assertEquals(
        'Basic table row count, known value', 4, testObjects.basic.rows.length);
    assertEquals(
        'Basic table first row element', originalRows[0],
        testObjects.basic.rows[0].element);
    assertEquals(
        'Basic table last row element', originalRows[3],
        testObjects.basic.rows[3].element);
    assertEquals(
        'Basic table first row length', 3,
        testObjects.basic.rows[0].columns.length);
    assertEquals(
        'Basic table last row length', 3,
        testObjects.basic.rows[3].columns.length);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testTortureTable() {
    // Do basic sanity checking on torture table structure
    tableSanityCheck(testObjects.torture, 9, 3);
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const originalRows =
        dom.getElementsByTagName(TagName.TR, testElements.torture);
    assertEquals(
        'Torture table row count, compared to source', originalRows.length,
        testObjects.torture.rows.length);
    assertEquals(
        'Torture table row count, known value', 9,
        testObjects.torture.rows.length);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testInsertRowAtBeginning() {
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const tr = testObjects.basic.insertRow(0);
    _testInsertRowResult(testElements.basic, testObjects.basic, tr, 0);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testInsertRowInMiddle() {
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const tr = testObjects.basic.insertRow(2);
    _testInsertRowResult(testElements.basic, testObjects.basic, tr, 2);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testInsertRowAtEnd() {
    assertEquals(
        'Table has expected number of existing rows', 4,
        testObjects.basic.rows.length);
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const tr = testObjects.basic.insertRow(4);
    _testInsertRowResult(testElements.basic, testObjects.basic, tr, 4);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testInsertRowAtEndNoIndexArgument() {
    assertEquals(
        'Table has expected number of existing rows', 4,
        testObjects.basic.rows.length);
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const tr = testObjects.basic.insertRow();
    _testInsertRowResult(testElements.basic, testObjects.basic, tr, 4);
  },

  /**
     @suppress {missingProperties,strictMissingProperties} suppression
     added to enable type checking
   */
  testInsertRowAtBeginningRowspan() {
    // Test inserting a row when the existing DOM row at that index has
    // a cell with a rowspan. This should be just like a regular insert -
    // the rowspan shouldn't have any effect.
    assertEquals(
        'Cell has starting rowspan', 2,
        dom.getFirstElementChild(
               dom.getElementsByTagName(TagName.TR, testElements.torture)[0])
            .rowSpan);
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const tr = testObjects.torture.insertRow(0);
    // Among other things this verifies that the new row has 3 child TDs.
    _testInsertRowResult(testElements.torture, testObjects.torture, tr, 0);
  },

  /**
     @suppress {missingProperties,strictMissingProperties} suppression
     added to enable type checking
   */
  testInsertRowAtEndingRowspan() {
    // Test inserting a row when there's a cell in a previous DOM row
    // with a rowspan that extends into the row with the given index
    // and ends there. This should be just like a regular insert -
    // the rowspan shouldn't have any effect.
    assertEquals(
        'Cell has ending rowspan', 4,
        dom.getLastElementChild(
               dom.getElementsByTagName(TagName.TR, testElements.torture)[5])
            .rowSpan);
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const tr = testObjects.torture.insertRow();
    // Among other things this verifies that the new row has 3 child TDs.
    _testInsertRowResult(testElements.torture, testObjects.torture, tr, 9);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testInsertRowAtSpanningRowspan() {
    // Test inserting a row at an index where there's a cell with a rowspan
    // that begins in a previous row and continues into the next row. In
    // this case the existing cell's rowspan should be extended, and the new
    // tr should have one less child element.
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const rowSpannedCell = testObjects.torture.rows[7].columns[2];
    assertTrue(
        'Existing cell has overlapping rowspan',
        rowSpannedCell.startRow == 5 && rowSpannedCell.endRow == 8);
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const tr = testObjects.torture.insertRow(7);
    assertEquals(
        'New DOM row has one less cell', 2,
        dom.getElementsByTagName(TagName.TD, tr).length);
    assertEquals(
        'Rowspanned cell listed in new EditableRow\'s columns',
        testObjects.torture.rows[6].columns[2].element,
        testObjects.torture.rows[7].columns[2].element);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testInsertColumnAtBeginning() {
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const startColCount = testObjects.basic.rows[0].columns.length;
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const newCells = testObjects.basic.insertColumn(0);
    assertEquals(
        'New cell added for each row', testObjects.basic.rows.length,
        newCells.length);
    assertEquals(
        'Insert column incremented column length', startColCount + 1,
        testObjects.basic.rows[0].columns.length);
    _testInsertColumnResult(newCells, testElements.basic, testObjects.basic, 0);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testInsertColumnAtEnd() {
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const startColCount = testObjects.basic.rows[0].columns.length;
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const newCells = testObjects.basic.insertColumn(3);
    assertEquals(
        'New cell added for each row', testObjects.basic.rows.length,
        newCells.length);
    assertEquals(
        'Insert column incremented column length', startColCount + 1,
        testObjects.basic.rows[0].columns.length);
    _testInsertColumnResult(newCells, testElements.basic, testObjects.basic, 3);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testInsertColumnAtEndNoIndexArgument() {
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const startColCount = testObjects.basic.rows[0].columns.length;
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const newCells = testObjects.basic.insertColumn();
    assertEquals(
        'New cell added for each row', testObjects.basic.rows.length,
        newCells.length);
    assertEquals(
        'Insert column incremented column length', startColCount + 1,
        testObjects.basic.rows[0].columns.length);
    _testInsertColumnResult(newCells, testElements.basic, testObjects.basic, 3);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testInsertColumnInMiddle() {
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const startColCount = testObjects.basic.rows[0].columns.length;
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const newCells = testObjects.basic.insertColumn(2);
    assertEquals(
        'New cell added for each row', testObjects.basic.rows.length,
        newCells.length);
    assertEquals(
        'Insert column incremented column length', startColCount + 1,
        testObjects.basic.rows[0].columns.length);
    _testInsertColumnResult(newCells, testElements.basic, testObjects.basic, 2);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testInsertColumnAtBeginningColSpan() {
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const cells = testObjects.torture.insertColumn(0);
    tableSanityCheck(testObjects.torture, 9, 4);
    assertEquals(
        'New cell was added before colspanned cell', 1,
        testObjects.torture.rows[3].columns[0].colSpan);
    assertEquals(
        'New cell was added and returned',
        testObjects.torture.rows[3].columns[0].element, cells[3]);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testInsertColumnAtEndingColSpan() {
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const cells = testObjects.torture.insertColumn();
    tableSanityCheck(testObjects.torture, 9, 4);
    assertEquals(
        'New cell was added after colspanned cell', 1,
        testObjects.torture.rows[0].columns[3].colSpan);
    assertEquals(
        'New cell was added and returned',
        testObjects.torture.rows[0].columns[3].element, cells[0]);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testInsertColumnAtSpanningColSpan() {
    assertEquals(
        'Existing cell has expected colspan', 3,
        testObjects.torture.rows[4].columns[1].colSpan);
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const cells = testObjects.torture.insertColumn(1);
    tableSanityCheck(testObjects.torture, 9, 4);
    assertEquals(
        'Existing cell increased colspan', 4,
        testObjects.torture.rows[4].columns[1].colSpan);
    assertEquals(
        '3 cells weren\'t created due to existing colspans', 6, cells.length);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testRemoveFirstRow() {
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const originalRow =
        dom.getElementsByTagName(TagName.TR, testElements.basic)[0];
    testObjects.basic.removeRow(0);
    tableSanityCheck(testObjects.basic, 3, 3);
    assertNotEquals(
        'Row was removed from table element', originalRow,
        dom.getElementsByTagName(TagName.TR, testElements.basic)[0]);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testRemoveLastRow() {
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const originalRow =
        dom.getElementsByTagName(TagName.TR, testElements.basic)[3];
    testObjects.basic.removeRow(3);
    tableSanityCheck(testObjects.basic, 3, 3);
    assertNotEquals(
        'Row was removed from table element', originalRow,
        dom.getElementsByTagName(TagName.TR, testElements.basic)[3]);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testRemoveMiddleRow() {
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const originalRow =
        dom.getElementsByTagName(TagName.TR, testElements.basic)[2];
    testObjects.basic.removeRow(2);
    tableSanityCheck(testObjects.basic, 3, 3);
    assertNotEquals(
        'Row was removed from table element', originalRow,
        dom.getElementsByTagName(TagName.TR, testElements.basic)[2]);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testRemoveRowAtBeginingRowSpan() {
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const originalRow = testObjects.torture.removeRow(0);
    tableSanityCheck(testObjects.torture, 8, 3);
    assertNotEquals(
        'Row was removed from table element', originalRow,
        dom.getElementsByTagName(TagName.TR, testElements.basic)[0]);
    assertEquals(
        'Rowspan correctly adjusted', 1,
        testObjects.torture.rows[0].columns[0].rowSpan);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testRemoveRowAtEndingRowSpan() {
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const originalRow =
        dom.getElementsByTagName(TagName.TR, testElements.torture)[8];
    testObjects.torture.removeRow(8);
    tableSanityCheck(testObjects.torture, 8, 3);
    assertNotEquals(
        'Row was removed from table element', originalRow,
        dom.getElementsByTagName(TagName.TR, testElements.basic)[8]);
    assertEquals(
        'Rowspan correctly adjusted', 3,
        testObjects.torture.rows[7].columns[2].rowSpan);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testRemoveRowAtSpanningRowSpan() {
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const originalRow =
        dom.getElementsByTagName(TagName.TR, testElements.torture)[7];
    testObjects.torture.removeRow(7);
    tableSanityCheck(testObjects.torture, 8, 3);
    assertNotEquals(
        'Row was removed from table element', originalRow,
        dom.getElementsByTagName(TagName.TR, testElements.basic)[7]);
    assertEquals(
        'Rowspan correctly adjusted', 3,
        testObjects.torture.rows[6].columns[2].rowSpan);
  },

  testRemoveFirstColumn() {
    _testRemoveColumn(0);
  },

  testRemoveMiddleColumn() {
    _testRemoveColumn(1);
  },

  testRemoveLastColumn() {
    _testRemoveColumn(2);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testRemoveColumnAtStartingColSpan() {
    testObjects.torture.removeColumn(0);
    tableSanityCheck(testObjects.torture, 9, 2);
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const tr = dom.getElementsByTagName(TagName.TR, testElements.torture)[5];
    assertEquals(
        'Colspan was decremented correctly', 1,
        dom.getElementsByTagName(TagName.TH, tr)[0].colSpan);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testRemoveColumnAtEndingColSpan() {
    testObjects.torture.removeColumn(2);
    tableSanityCheck(testObjects.torture, 9, 2);
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const tr = dom.getElementsByTagName(TagName.TR, testElements.torture)[1];
    assertEquals(
        'Colspan was decremented correctly', 1,
        dom.getElementsByTagName(TagName.TD, tr)[0].colSpan);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testRemoveColumnAtSpanningColSpan() {
    testObjects.torture.removeColumn(2);
    tableSanityCheck(testObjects.torture, 9, 2);
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const tr = dom.getElementsByTagName(TagName.TR, testElements.torture)[4];
    assertEquals(
        'Colspan was decremented correctly', 2,
        dom.getElementsByTagName(TagName.TH, tr)[0].colSpan);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testMergeCellsInRow() {
    testObjects.basic.mergeCells(0, 0, 0, 2);
    tableSanityCheck(testObjects.basic, 4, 3);
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const trs = dom.getElementsByTagName(TagName.TR, testElements.basic);
    assertEquals(
        'Cells merged', 1, dom.getElementsByTagName(TagName.TH, trs[0]).length);
    assertEquals(
        'Merged cell has correct colspan', 3,
        dom.getElementsByTagName(TagName.TH, trs[0])[0].colSpan);
    assertEquals(
        'Merged cell has correct rowspan', 1,
        dom.getElementsByTagName(TagName.TH, trs[0])[0].rowSpan);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testMergeCellsInColumn() {
    testObjects.basic.mergeCells(0, 0, 2, 0);
    tableSanityCheck(testObjects.basic, 4, 3);
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const trs = dom.getElementsByTagName(TagName.TR, testElements.basic);
    assertEquals(
        'Other cells still in row', 3,
        dom.getElementsByTagName(TagName.TH, trs[0]).length);
    assertEquals(
        'Merged cell has correct colspan', 1,
        dom.getElementsByTagName(TagName.TH, trs[0])[0].colSpan);
    assertEquals(
        'Merged cell has correct rowspan', 3,
        dom.getElementsByTagName(TagName.TH, trs[0])[0].rowSpan);
    assert(
        'Cell appears in multiple rows after merge',
        testObjects.basic.rows[0].columns[0] ==
            testObjects.basic.rows[2].columns[0]);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testMergeCellsInRowAndColumn() {
    testObjects.basic.mergeCells(1, 1, 3, 2);
    tableSanityCheck(testObjects.basic, 4, 3);
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const trs = dom.getElementsByTagName(TagName.TR, testElements.basic);
    const mergedCell = dom.getElementsByTagName(TagName.TD, trs[1])[1];
    assertEquals('Merged cell has correct rowspan', 3, mergedCell.rowSpan);
    assertEquals('Merged cell has correct colspan', 2, mergedCell.colSpan);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testMergeCellsAlreadyMerged() {
    testObjects.torture.mergeCells(5, 0, 8, 2);
    tableSanityCheck(testObjects.torture, 9, 3);
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const trs = dom.getElementsByTagName(TagName.TR, testElements.torture);
    const mergedCell = dom.getElementsByTagName(TagName.TH, trs[5])[0];
    assertEquals('Merged cell has correct rowspan', 4, mergedCell.rowSpan);
    assertEquals('Merged cell has correct colspan', 3, mergedCell.colSpan);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testIllegalMergeNonRectangular() {
    // This should fail because it involves trying to merge two parts
    // of a 3-colspan cell with other cells
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const mergeSucceeded = testObjects.torture.mergeCells(3, 1, 5, 2);
    if (mergeSucceeded) {
      throw 'EditableTable allowed impossible merge!';
    }
    tableSanityCheck(testObjects.torture, 9, 3);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testIllegalMergeSingleCell() {
    // This should fail because it involves merging a single cell
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const mergeSucceeded = testObjects.torture.mergeCells(0, 1, 0, 1);
    if (mergeSucceeded) {
      throw 'EditableTable allowed impossible merge!';
    }
    tableSanityCheck(testObjects.torture, 9, 3);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testSplitCell() {
    testObjects.torture.splitCell(1, 1);
    tableSanityCheck(testObjects.torture, 9, 3);
    /**
     * @suppress {missingProperties} suppression added to enable type
     * checking
     */
    const trs = dom.getElementsByTagName(TagName.TR, testElements.torture);
    assertEquals(
        'Cell was split into multiple columns in row 1', 3,
        trs[1].getElementsByTagName('*').length);
    assertEquals(
        'Cell was split into multiple columns in row 2', 3,
        trs[2].getElementsByTagName('*').length);
  },

  /**
     @suppress {missingProperties} suppression added to enable type
     checking
   */
  testChildTableRowsNotCountedInParentTable() {
    tableSanityCheck(testObjects.nested, 2, 3);
    for (let i = 0; i < testObjects.nested.rows.length; i++) {
      /**
       * @suppress {missingProperties} suppression added to enable type
       * checking
       */
      const tr = testObjects.nested.rows[i].element;
      // A tr's parent is tbody, parent of that is table - check to
      // make sure the ancestor table is as expected. This means
      // that none of the child table's rows have been erroneously
      // loaded into the EditableTable.
      assertEquals(
          'Row is child of parent table', testElements.nested,
          tr.parentNode.parentNode);
    }
  },
});
