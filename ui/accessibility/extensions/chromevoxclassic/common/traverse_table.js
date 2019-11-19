// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TODO(stoarca): This class has become obsolete except for the shadow table.
 * Chop most of it away.
 * @fileoverview A DOM traversal interface for navigating data in tables.
 */

goog.provide('cvox.TraverseTable');

goog.require('cvox.DomPredicates');
goog.require('cvox.DomUtil');
goog.require('cvox.SelectionUtil');
goog.require('cvox.TableUtil');
goog.require('cvox.TraverseUtil');



/**
 * An object that represents an active table cell inside the shadow table.
 * @constructor
 */
function ShadowTableNode() {
  /**
   * The cells that are row headers of the corresponding active table cell
   * @type {!Array}
   */
  this.rowHeaderCells = [];

  /**
   * The cells that are column headers of the corresponding active table cell
   * @type {!Array}
   */
  this.colHeaderCells = [];
}


/**
 * Whether or not the active cell is spanned by a preceding cell.
 * @type {boolean}
 */
ShadowTableNode.prototype.spanned;


/**
 * Whether or not this cell is spanned by a rowSpan.
 * @type {?boolean}
 */
ShadowTableNode.prototype.rowSpan;


/**
 * Whether or not this cell is spanned by a colspan
 * @type {?boolean}
 */
ShadowTableNode.prototype.colSpan;


/**
 * The row index of the corresponding active table cell
 * @type {number}
 */
ShadowTableNode.prototype.i = -1;


/**
 * The column index of the corresponding active table cell
 * @type {number}
 */
ShadowTableNode.prototype.j = -1;


/**
 * The corresponding <TD> or <TH> node in the active table.
 * @type {?Node}
 */
ShadowTableNode.prototype.activeCell;


/**
 * Initializes the traversal with the provided table node.
 *
 * @constructor
 * @param {Node} tableNode The table to be traversed.
 */
cvox.TraverseTable = function(tableNode) {

  /**
   * The active table <TABLE> node. In this context, "active" means that this is
   * the table the TraverseTable object is navigating.
   * @type {Node}
   * @private
   */
  this.activeTable_ = null;

  /**
   * A 2D array "shadow table" that contains pointers to nodes in the active
   * table. More specifically, each cell of the shadow table contains a special
   * object ShadowTableNode that has as one of its member variables the
   * corresponding cell in the active table.
   *
   * The shadow table will allow us efficient navigation of tables with
   * rowspans and colspans without needing to repeatedly scan the table. For
   * example, if someone requests a cell at (1,3), predecessor cells with
   * rowspans/colspans mean the cell you eventually return could actually be
   * one located at (0,2) that spans out to (1,3).
   *
   * This shadow table will contain a ShadowTableNode with the (0, 2) index at
   * the (1,3) position, eliminating the need to check for predecessor cells
   * with rowspan/colspan every time we traverse the table.
   *
   * @type {!Array<Array<ShadowTableNode>>}
   * @private
   */
  this.shadowTable_ = [];

  /**
   * An array of shadow table nodes that have been determined to contain header
   * cells or information about header cells. This array is collected at
   * initialization and then only recalculated if the table changes.
   * This array is used by findHeaderCells() to determine table row headers
   * and column headers.
   * @type {Array<ShadowTableNode>}
   * @private
   */
  this.candidateHeaders_ = [];

  /**
   * An array that associates cell IDs with their corresponding shadow nodes.
   * If there are two shadow nodes for the same cell (i.e. when a cell spans
   * other cells) then the first one will be associated with the ID. This means
   * that shadow nodes that have spanned set to true will not be included in
   * this array.
   * @type {Array<ShadowTableNode>}
   * @private
   */
  this.idToShadowNode_ = [];

  this.initialize(tableNode);
};


/**
 * The cell cursor, represented by an array that stores the row and
 * column location [i, j] of the active cell. These numbers are 0-based.
 * In this context, "active" means that this is the cell the user is
 * currently looking at.
 * @type {Array}
 */
cvox.TraverseTable.prototype.currentCellCursor;


/**
 * The number of columns in the active table. This is calculated at
 * initialization and then only recalculated if the table changes.
 *
 * Please Note: We have chosen to use the number of columns in the shadow
 * table as the canonical column count. This is important for tables that
 * have colspans - the number of columns in the active table will always be
 * less than the true number of columns.
 * @type {?number}
 */
cvox.TraverseTable.prototype.colCount = null;


/**
 * The number of rows in the active table. This is calculated at
 * initialization and then only recalculated if the table changes.
 * @type {?number}
 */
cvox.TraverseTable.prototype.rowCount = null;


/**
 * The row headers in the active table. This is calculated at
 * initialization and then only recalculated if the table changes.
 *
 * Please Note:
 *  Row headers are defined here as <TH> or <TD> elements. <TD> elements when
 *  serving as header cells must have either:
 *  - The scope attribute defined
 *  - Their IDs referenced in the header content attribute of another <TD> or
 *  <TH> element.
 *
 *  The HTML5 spec specifies that only header <TH> elements can be row headers
 *  ( http://dev.w3.org/html5/spec/tabular-data.html#row-header ) but the
 *  HTML4 spec says that <TD> elements can act as both
 *  ( http://www.w3.org/TR/html401/struct/tables.html#h-11.2.6 ). In the
 *  interest of providing meaningful header information for all tables, here
 *  we take the position that <TD> elements can act as both.
 *
 * @type {Array}
 */
cvox.TraverseTable.prototype.tableRowHeaders = null;


/**
 * The column headers in the active table. This is calculated at
 * initialization and then only recalculated if the table changes.
 *
 * Please Note: see comment for tableRowHeaders.
 *
 * @type {Array}
 */
cvox.TraverseTable.prototype.tableColHeaders = null;


// TODO (stoarca): tighten up interface to {!Node}
/**
 * Initializes the class member variables.
 * @param {Node} tableNode The table to be traversed.
 */
cvox.TraverseTable.prototype.initialize = function(tableNode) {
  if (!tableNode) {
    return;
  }
  if (tableNode == this.activeTable_) {
    return;
  }
  this.activeTable_ = tableNode;
  this.currentCellCursor = null;

  this.tableRowHeaders = [];
  this.tableColHeaders = [];

  this.buildShadowTable_();

  this.colCount = this.shadowColCount_();
  this.rowCount = this.countRows_();

  this.findHeaderCells_();

  // Listen for changes to the active table. If the active table changes,
  // rebuild the shadow table.
  // TODO (stoarca): Is this safe? When this object goes away, doesn't the
  // eventListener stay on the node? Someone with better knowledge of js
  // please confirm. If so, this is a leak.
  this.activeTable_.addEventListener('DOMSubtreeModified',
      goog.bind(function() {
        this.buildShadowTable_();
        this.colCount = this.shadowColCount_();
        this.rowCount = this.countRows_();

        this.tableRowHeaders = [];
        this.tableColHeaders = [];
        this.findHeaderCells_();

        if (this.colCount == 0 && this.rowCount == 0) {
          return;
        }

        if (this.getCell() == null) {
          this.attachCursorToNearestCell_();
        }
      }, this), false);
};


/**
 * Finds the cell cursor containing the specified node within the table.
 * Returns null if there is no close cell.
 * @param {Node} node The node for which to find the cursor.
 * @return {Array<number>} The table index for the node.
 */
cvox.TraverseTable.prototype.findNearestCursor = function(node) {
  if (!node) {
    return null;
  }
  // TODO (stoarca): The current structure for representing the
  // shadow table is not optimal for this query, but it's not urgent
  // since this only gets executed at most once per user action.

  // In case node is in a table but above any individual cell, we go down as
  // deep as we can, being careful to avoid going into nested tables.
  var n = node;

  while (n.firstElementChild &&
         !(n.firstElementChild.tagName == 'TABLE' ||
           cvox.AriaUtil.isGrid(n.firstElementChild))) {
    n = n.firstElementChild;
  }
  while (!cvox.DomPredicates.cellPredicate(cvox.DomUtil.getAncestors(n))) {
    n = cvox.DomUtil.directedNextLeafNode(n);
    // TODO(stoarca): Ugly logic. Captions should be part of tables.
    // There have been a bunch of bugs as a result of
    // DomUtil.findTableNodeInList excluding captions from tables because
    // it makes them non-contiguous.
    if (!cvox.DomUtil.getContainingTable(n, {allowCaptions: true})) {
      return null;
    }
  }
  for (var i = 0; i < this.rowCount; ++i) {
    for (var j = 0; j < this.colCount; ++j) {
      if (this.shadowTable_[i][j]) {
        if (cvox.DomUtil.isDescendantOfNode(
            n, this.shadowTable_[i][j].activeCell)) {
          return [i, j];
        }
      }
    }
  }
  return null;
};

/**
 * Finds the valid cell nearest to the current cell cursor and moves the cell
 * cursor there. To be used when the table has changed and the current cell
 * cursor is now invalid (doesn't exist anymore).
 * @private
 */
cvox.TraverseTable.prototype.attachCursorToNearestCell_ = function() {
  if (!this.currentCellCursor) {
    // We have no idea.  Just go 'somewhere'. Other code paths in this
    // function go to the last cell, so let's do that!
    this.goToLastCell();
    return;
  }

  var currentCursor = this.currentCellCursor;

  // Does the current row still exist in the table?
  var currentRow = this.shadowTable_[currentCursor[0]];
  if (currentRow) {
    // Try last cell of current row
    this.currentCellCursor = [currentCursor[0], (currentRow.length - 1)];
  } else {
    // Current row does not exist anymore. Does current column still exist?
    // Try last cell of current column
    var numRows = this.shadowTable_.length;
    if (numRows == 0) {
      // Table has been deleted!
      this.currentCellCursor = null;
      return;
    }
    var aboveCell =
        this.shadowTable_[numRows - 1][currentCursor[1]];
    if (aboveCell) {
      this.currentCellCursor = [(numRows - 1), currentCursor[1]];
    } else {
      // Current column does not exist anymore either.
      // Move cursor to last cell in table.
      this.goToLastCell();
    }
  }
};


/**
 * Builds or rebuilds the shadow table by iterating through all of the cells
 * ( <TD> or <TH> or role='gridcell' nodes) of the active table.
 * @return {!Array} The shadow table.
 * @private
 */
cvox.TraverseTable.prototype.buildShadowTable_ = function() {
  // Clear shadow table
  this.shadowTable_ = [];

  // Build shadow table structure. Initialize it as a 2D array.
  var allRows = cvox.TableUtil.getChildRows(this.activeTable_);
  var currentRowParent = null;
  var currentRowGroup = null;

  var colGroups = cvox.TableUtil.getColGroups(this.activeTable_);
  var colToColGroup = cvox.TableUtil.determineColGroups(colGroups);

  for (var ctr = 0; ctr < allRows.length; ctr++) {
    this.shadowTable_.push([]);
  }

  // Iterate through active table by row
  for (var i = 0; i < allRows.length; i++) {
    var childCells = cvox.TableUtil.getChildCells(allRows[i]);

    // Keep track of position in active table
    var activeTableCol = 0;
    // Keep track of position in shadow table
    var shadowTableCol = 0;

    while (activeTableCol < childCells.length) {

      // Check to make sure we haven't already filled this cell.
      if (this.shadowTable_[i][shadowTableCol] == null) {

        var activeTableCell = childCells[activeTableCol];

        // Default value for colspan and rowspan is 1
        var colsSpanned = 1;
        var rowsSpanned = 1;

        if (activeTableCell.hasAttribute('colspan')) {

          colsSpanned =
              parseInt(activeTableCell.getAttribute('colspan'), 10);

          if ((isNaN(colsSpanned)) || (colsSpanned <= 0)) {
            // The HTML5 spec defines colspan MUST be greater than 0:
            // http://dev.w3.org/html5/spec/Overview.html#attr-tdth-colspan
            //
            // This is a change from the HTML4 spec:
            // http://www.w3.org/TR/html401/struct/tables.html#adef-colspan
            //
            // We will degrade gracefully by treating a colspan=0 as
            // equivalent to a colspan=1.
            // Tested in method testColSpan0 in rowColSpanTable_test.js
            colsSpanned = 1;
          }
        }
        if (activeTableCell.hasAttribute('rowspan')) {
          rowsSpanned =
              parseInt(activeTableCell.getAttribute('rowspan'), 10);

          if ((isNaN(rowsSpanned)) || (rowsSpanned <= 0)) {
            // The HTML5 spec defines that rowspan can be any non-negative
            // integer, including 0:
            // http://dev.w3.org/html5/spec/Overview.html#attr-tdth-rowspan
            //
            // However, Chromium treats rowspan=0 as rowspan=1. This appears
            // to be a bug from WebKit:
            // https://bugs.webkit.org/show_bug.cgi?id=10300
            // Inherited from a bug (since fixed) in KDE:
            // http://bugs.kde.org/show_bug.cgi?id=41063
            //
            // We will follow Chromium and treat rowspan=0 as equivalent to
            // rowspan=1.
            //
            // Tested in method testRowSpan0 in rowColSpanTable_test.js
            //
            // Filed as a bug in Chromium: http://crbug.com/58223
            rowsSpanned = 1;
          }
        }
        for (var r = 0; r < rowsSpanned; r++) {
          for (var c = 0; c < colsSpanned; c++) {
            var shadowNode = new ShadowTableNode();
            if ((r == 0) && (c == 0)) {
              // This position is not spanned.
              shadowNode.spanned = false;
              shadowNode.rowSpan = false;
              shadowNode.colSpan = false;
              shadowNode.i = i;
              shadowNode.j = shadowTableCol;
              shadowNode.activeCell = activeTableCell;
              shadowNode.rowHeaderCells = [];
              shadowNode.colHeaderCells = [];
              shadowNode.isRowHeader = false;
              shadowNode.isColHeader = false;
            } else {
              // This position is spanned.
              shadowNode.spanned = true;
              shadowNode.rowSpan = (rowsSpanned > 1);
              shadowNode.colSpan = (colsSpanned > 1);
              shadowNode.i = i;
              shadowNode.j = shadowTableCol;
              shadowNode.activeCell = activeTableCell;
              shadowNode.rowHeaderCells = [];
              shadowNode.colHeaderCells = [];
              shadowNode.isRowHeader = false;
              shadowNode.isColHeader = false;
            }
            // Check this shadowNode to see if it is a candidate header cell
            if (cvox.TableUtil.checkIfHeader(shadowNode.activeCell)) {
              this.candidateHeaders_.push(shadowNode);
            } else if (shadowNode.activeCell.hasAttribute('headers')) {
              // This shadowNode has information about other header cells
              this.candidateHeaders_.push(shadowNode);
            }

            // Check and update row group status.
            if (currentRowParent == null) {
              // This is the first row
              currentRowParent = allRows[i].parentNode;
              currentRowGroup = 0;
            } else {
              if (allRows[i].parentNode != currentRowParent) {
                // We're in a different row group now
                currentRowParent = allRows[i].parentNode;
                currentRowGroup = currentRowGroup + 1;
              }
            }
            shadowNode.rowGroup = currentRowGroup;

            // Check and update col group status
            if (colToColGroup.length > 0) {
              shadowNode.colGroup = colToColGroup[shadowTableCol];
            } else {
              shadowNode.colGroup = 0;
            }

            if (! shadowNode.spanned) {
              if (activeTableCell.id != null) {
                this.idToShadowNode_[activeTableCell.id] = shadowNode;
              }
            }

            this.shadowTable_[i + r][shadowTableCol + c] = shadowNode;
          }
        }
        shadowTableCol += colsSpanned;
        activeTableCol++;
      } else {
        // This position has already been filled (by a previous cell that has
        // a colspan or a rowspan)
        shadowTableCol += 1;
      }
    }
  }
  return this.shadowTable_;
};


/**
 * Finds header cells from the list of candidate headers and classifies them
 * in two ways:
 * -- Identifies them for the entire table by adding them to
 * this.tableRowHeaders and this.tableColHeaders.
 * -- Identifies them for each shadow table node by adding them to the node's
 * rowHeaderCells or colHeaderCells arrays.
 *
 * @private
 */
cvox.TraverseTable.prototype.findHeaderCells_ = function() {
  // Forming relationships between data cells and header cells:
  // http://dev.w3.org/html5/spec/tabular-data.html
  // #header-and-data-cell-semantics

  for (var i = 0; i < this.candidateHeaders_.length; i++) {

    var currentShadowNode = this.candidateHeaders_[i];
    var currentCell = currentShadowNode.activeCell;

    var assumedScope = null;
    var specifiedScope = null;

    if (currentShadowNode.spanned) {
      continue;
    }

    if ((currentCell.tagName == 'TH') &&
        !(currentCell.hasAttribute('scope'))) {
      // No scope specified - compute scope ourselves.
      // Go left/right - if there's a header node, then this is a column
      // header
      if (currentShadowNode.j > 0) {
        if (this.shadowTable_[currentShadowNode.i][currentShadowNode.j - 1].
            activeCell.tagName == 'TH') {
          assumedScope = 'col';
        }
      } else if (currentShadowNode.j < this.shadowTable_[currentShadowNode.i].
          length - 1) {
        if (this.shadowTable_[currentShadowNode.i][currentShadowNode.j + 1].
            activeCell.tagName == 'TH') {
          assumedScope = 'col';
        }
      } else {
        // This row has a width of 1 cell, just assume this is a colum header
        assumedScope = 'col';
      }

      if (assumedScope == null) {
        // Go up/down - if there's a header node, then this is a row header
        if (currentShadowNode.i > 0) {
          if (this.shadowTable_[currentShadowNode.i - 1][currentShadowNode.j].
              activeCell.tagName == 'TH') {
            assumedScope = 'row';
          }
        } else if (currentShadowNode.i < this.shadowTable_.length - 1) {
          if (this.shadowTable_[currentShadowNode.i + 1][currentShadowNode.j].
              activeCell.tagName == 'TH') {
            assumedScope = 'row';
          }
        } else {
          // This column has a height of 1 cell, just assume that this is
          // a row header
          assumedScope = 'row';
        }
      }
    } else if (currentCell.hasAttribute('scope')) {
      specifiedScope = currentCell.getAttribute('scope');
    } else if (currentCell.hasAttribute('role') &&
        (currentCell.getAttribute('role') == 'rowheader')) {
      specifiedScope = 'row';
    } else if (currentCell.hasAttribute('role') &&
        (currentCell.getAttribute('role') == 'columnheader')) {
     specifiedScope = 'col';
    }

    if ((specifiedScope == 'row') || (assumedScope == 'row')) {
      currentShadowNode.isRowHeader = true;

      // Go right until you hit the edge of the table or a data
      // cell after another header cell.
      // Add this cell to each shadowNode.rowHeaderCells attribute as you go.
      for (var rightCtr = currentShadowNode.j;
           rightCtr < this.shadowTable_[currentShadowNode.i].length;
           rightCtr++) {

        var rightShadowNode = this.shadowTable_[currentShadowNode.i][rightCtr];
        var rightCell = rightShadowNode.activeCell;

        if ((rightCell.tagName == 'TH') ||
            (rightCell.hasAttribute('scope'))) {

          if (rightCtr < this.shadowTable_[currentShadowNode.i].length - 1) {
            var checkDataCell =
                this.shadowTable_[currentShadowNode.i][rightCtr + 1];
          }
        }
        rightShadowNode.rowHeaderCells.push(currentCell);
      }
      this.tableRowHeaders.push(currentCell);
    } else if ((specifiedScope == 'col') || (assumedScope == 'col')) {
      currentShadowNode.isColHeader = true;

      // Go down until you hit the edge of the table or a data cell
      // after another header cell.
      // Add this cell to each shadowNode.colHeaders attribute as you go.

      for (var downCtr = currentShadowNode.i;
           downCtr < this.shadowTable_.length;
           downCtr++) {

        var downShadowNode = this.shadowTable_[downCtr][currentShadowNode.j];
        if (downShadowNode == null) {
          break;
        }
        var downCell = downShadowNode.activeCell;

        if ((downCell.tagName == 'TH') ||
            (downCell.hasAttribute('scope'))) {

          if (downCtr < this.shadowTable_.length - 1) {
            var checkDataCell =
                this.shadowTable_[downCtr + 1][currentShadowNode.j];
          }
        }
        downShadowNode.colHeaderCells.push(currentCell);
      }
      this.tableColHeaders.push(currentCell);
    } else if (specifiedScope == 'rowgroup') {
       currentShadowNode.isRowHeader = true;

      // This cell is a row header for the rest of the cells in this row group.
      var currentRowGroup = currentShadowNode.rowGroup;

      // Get the rest of the cells in this row first
      for (var cellsInRow = currentShadowNode.j + 1;
           cellsInRow < this.shadowTable_[currentShadowNode.i].length;
           cellsInRow++) {
        this.shadowTable_[currentShadowNode.i][cellsInRow].
            rowHeaderCells.push(currentCell);
      }

      // Now propagate to rest of row group
      for (var downCtr = currentShadowNode.i + 1;
           downCtr < this.shadowTable_.length;
           downCtr++) {

        if (this.shadowTable_[downCtr][0].rowGroup != currentRowGroup) {
          break;
        }

        for (var rightCtr = 0;
             rightCtr < this.shadowTable_[downCtr].length;
             rightCtr++) {

          this.shadowTable_[downCtr][rightCtr].
              rowHeaderCells.push(currentCell);
        }
      }
      this.tableRowHeaders.push(currentCell);

    } else if (specifiedScope == 'colgroup') {
      currentShadowNode.isColHeader = true;

      // This cell is a col header for the rest of the cells in this col group.
      var currentColGroup = currentShadowNode.colGroup;

      // Get the rest of the cells in this colgroup first
      for (var cellsInCol = currentShadowNode.j + 1;
           cellsInCol < this.shadowTable_[currentShadowNode.i].length;
           cellsInCol++) {
        if (this.shadowTable_[currentShadowNode.i][cellsInCol].colGroup ==
            currentColGroup) {
          this.shadowTable_[currentShadowNode.i][cellsInCol].
              colHeaderCells.push(currentCell);
        }
      }

      // Now propagate to rest of col group
      for (var downCtr = currentShadowNode.i + 1;
           downCtr < this.shadowTable_.length;
           downCtr++) {

        for (var rightCtr = 0;
             rightCtr < this.shadowTable_[downCtr].length;
             rightCtr++) {

          if (this.shadowTable_[downCtr][rightCtr].colGroup ==
              currentColGroup) {
            this.shadowTable_[downCtr][rightCtr].
                colHeaderCells.push(currentCell);
          }
        }
      }
      this.tableColHeaders.push(currentCell);
    }
    if (currentCell.hasAttribute('headers')) {
      this.findAttrbHeaders_(currentShadowNode);
    }
    if (currentCell.hasAttribute('aria-describedby')) {
      this.findAttrbDescribedBy_(currentShadowNode);
    }
  }
};


/**
 * Finds header cells from the 'headers' attribute of a given shadow node's
 * active cell and classifies them in two ways:
 * -- Identifies them for the entire table by adding them to
 * this.tableRowHeaders and this.tableColHeaders.
 * -- Identifies them for the shadow table node by adding them to the node's
 * rowHeaderCells or colHeaderCells arrays.
 * Please note that header cells found through the 'headers' attribute are
 * difficult to attribute to being either row or column headers because a
 * table cell can declare arbitrary cells as its headers. A guess is made here
 * based on which axis the header cell is closest to.
 *
 * @param {ShadowTableNode} currentShadowNode A shadow node with an active cell
 * that has a 'headers' attribute.
 *
 * @private
 */
cvox.TraverseTable.prototype.findAttrbHeaders_ = function(currentShadowNode) {
  var activeTableCell = currentShadowNode.activeCell;

  var idList = activeTableCell.getAttribute('headers').split(' ');
  for (var idToken = 0; idToken < idList.length; idToken++) {
    // Find cell(s) with this ID, add to header list
    var idCellArray = cvox.TableUtil.getCellWithID(this.activeTable_,
                                                   idList[idToken]);

    for (var idCtr = 0; idCtr < idCellArray.length; idCtr++) {
      if (idCellArray[idCtr].id == activeTableCell.id) {
        // Skip if the ID is the same as the current cell's ID
        break;
      }
      // Check if this list of candidate headers contains a
      // shadowNode with an active cell with this ID already
      var possibleHeaderNode =
          this.idToShadowNode_[idCellArray[idCtr].id];
      if (! cvox.TableUtil.checkIfHeader(possibleHeaderNode.activeCell)) {
        // This listed header cell will not be handled later.
        // Determine whether this is a row or col header for
        // the active table cell

        var iDiff = Math.abs(possibleHeaderNode.i - currentShadowNode.i);
        var jDiff = Math.abs(possibleHeaderNode.j - currentShadowNode.j);
        if ((iDiff == 0) || (iDiff < jDiff)) {
          cvox.TableUtil.pushIfNotContained(currentShadowNode.rowHeaderCells,
                                            possibleHeaderNode.activeCell);
          cvox.TableUtil.pushIfNotContained(this.tableRowHeaders,
                                            possibleHeaderNode.activeCell);
        } else {
          // This is a column header
          cvox.TableUtil.pushIfNotContained(currentShadowNode.colHeaderCells,
                                            possibleHeaderNode.activeCell);
          cvox.TableUtil.pushIfNotContained(this.tableColHeaders,
                                            possibleHeaderNode.activeCell);
        }
      }
    }
  }
};


/**
 * Finds header cells from the 'aria-describedby' attribute of a given shadow
 * node's active cell and classifies them in two ways:
 * -- Identifies them for the entire table by adding them to
 * this.tableRowHeaders and this.tableColHeaders.
 * -- Identifies them for the shadow table node by adding them to the node's
 * rowHeaderCells or colHeaderCells arrays.
 *
 * Please note that header cells found through the 'aria-describedby' attribute
 * must have the role='rowheader' or role='columnheader' attributes in order to
 * be considered header cells.
 *
 * @param {ShadowTableNode} currentShadowNode A shadow node with an active cell
 * that has an 'aria-describedby' attribute.
 *
 * @private
 */
cvox.TraverseTable.prototype.findAttrbDescribedBy_ =
    function(currentShadowNode) {
  var activeTableCell = currentShadowNode.activeCell;

  var idList = activeTableCell.getAttribute('aria-describedby').split(' ');
  for (var idToken = 0; idToken < idList.length; idToken++) {
    // Find cell(s) with this ID, add to header list
    var idCellArray = cvox.TableUtil.getCellWithID(this.activeTable_,
                                                   idList[idToken]);

    for (var idCtr = 0; idCtr < idCellArray.length; idCtr++) {
      if (idCellArray[idCtr].id == activeTableCell.id) {
        // Skip if the ID is the same as the current cell's ID
        break;
      }
      // Check if this list of candidate headers contains a
      // shadowNode with an active cell with this ID already
      var possibleHeaderNode =
          this.idToShadowNode_[idCellArray[idCtr].id];
      if (! cvox.TableUtil.checkIfHeader(possibleHeaderNode.activeCell)) {
        // This listed header cell will not be handled later.
        // Determine whether this is a row or col header for
        // the active table cell

        if (possibleHeaderNode.activeCell.hasAttribute('role') &&
            (possibleHeaderNode.activeCell.getAttribute('role') ==
                'rowheader')) {
          cvox.TableUtil.pushIfNotContained(currentShadowNode.rowHeaderCells,
                                            possibleHeaderNode.activeCell);
          cvox.TableUtil.pushIfNotContained(this.tableRowHeaders,
                                            possibleHeaderNode.activeCell);
        } else if (possibleHeaderNode.activeCell.hasAttribute('role') &&
            (possibleHeaderNode.activeCell.getAttribute('role') ==
                'columnheader')) {
          cvox.TableUtil.pushIfNotContained(currentShadowNode.colHeaderCells,
                                            possibleHeaderNode.activeCell);
          cvox.TableUtil.pushIfNotContained(this.tableColHeaders,
                                            possibleHeaderNode.activeCell);
        }
      }
    }
  }
};


/**
 * Gets the current cell or null if there is no current cell.
 * @return {?Node} The cell <TD> or <TH> or role='gridcell' node.
 */
cvox.TraverseTable.prototype.getCell = function() {
  if (!this.currentCellCursor || !this.shadowTable_) {
    return null;
  }

  var shadowEntry =
      this.shadowTable_[this.currentCellCursor[0]][this.currentCellCursor[1]];

  return shadowEntry && shadowEntry.activeCell;
};


/**
 * Gets the cell at the specified location.
 * @param {Array<number>} index The index <i, j> of the required cell.
 * @return {?Node} The cell <TD> or <TH> or role='gridcell' node at the
 * specified location. Null if that cell does not exist.
 */
cvox.TraverseTable.prototype.getCellAt = function(index) {
  if (((index[0] < this.rowCount) && (index[0] >= 0)) &&
      ((index[1] < this.colCount) && (index[1] >= 0))) {
    var shadowEntry = this.shadowTable_[index[0]][index[1]];
    if (shadowEntry != null) {
      return shadowEntry.activeCell;
    }
  }
  return null;
};


/**
 * Gets the cells that are row headers of the current cell.
 * @return {!Array} The cells that are row headers of the current cell. Empty if
 * the current cell does not have row headers.
 */
cvox.TraverseTable.prototype.getCellRowHeaders = function() {
  var shadowEntry =
      this.shadowTable_[this.currentCellCursor[0]][this.currentCellCursor[1]];

  return shadowEntry.rowHeaderCells;
};


/**
 * Gets the cells that are col headers of the current cell.
 * @return {!Array} The cells that are col headers of the current cell. Empty if
 * the current cell does not have col headers.
 */
cvox.TraverseTable.prototype.getCellColHeaders = function() {
  var shadowEntry =
      this.shadowTable_[this.currentCellCursor[0]][this.currentCellCursor[1]];

  return shadowEntry.colHeaderCells;
};


/**
 * Whether or not the current cell is spanned by another cell.
 * @return {boolean} Whether or not the current cell is spanned by another cell.
 */
cvox.TraverseTable.prototype.isSpanned = function() {
  var shadowEntry =
      this.shadowTable_[this.currentCellCursor[0]][this.currentCellCursor[1]];

  return shadowEntry.spanned;
};


/**
 * Whether or not the current cell is a row header cell.
 * @return {boolean} Whether or not the current cell is a row header cell.
 */
cvox.TraverseTable.prototype.isRowHeader = function() {
  var shadowEntry =
      this.shadowTable_[this.currentCellCursor[0]][this.currentCellCursor[1]];

  return shadowEntry.isRowHeader;
};


/**
 * Whether or not the current cell is a col header cell.
 * @return {boolean} Whether or not the current cell is a col header cell.
 */
cvox.TraverseTable.prototype.isColHeader = function() {
  var shadowEntry =
      this.shadowTable_[this.currentCellCursor[0]][this.currentCellCursor[1]];

  return shadowEntry.isColHeader;
};


/**
 * Gets the active column, represented as an array of <TH> or <TD> nodes that
 * make up a column. In this context, "active" means that this is the column
 * that contains the cell the user is currently looking at.
 * @return {Array} An array of <TH> or <TD> or role='gridcell' nodes.
 */
cvox.TraverseTable.prototype.getCol = function() {
  var colArray = [];
  for (var i = 0; i < this.shadowTable_.length; i++) {

    if (this.shadowTable_[i][this.currentCellCursor[1]]) {
      var shadowEntry = this.shadowTable_[i][this.currentCellCursor[1]];

      if (shadowEntry.colSpan && shadowEntry.rowSpan) {
        // Look at the last element in the column cell aray.
        var prev = colArray[colArray.length - 1];
        if (prev !=
            shadowEntry.activeCell) {
          // Watch out for positions spanned by a cell with rowspan and
          // colspan. We don't want the same cell showing up multiple times
          // in per-column cell lists.
          colArray.push(
              shadowEntry.activeCell);
        }
      } else if ((shadowEntry.colSpan) || (!shadowEntry.rowSpan)) {
        colArray.push(
            shadowEntry.activeCell);
      }
    }
  }
  return colArray;
};


/**
 * Gets the active row <TR> node. In this context, "active" means that this is
 * the row that contains the cell the user is currently looking at.
 * @return {Node} The active row node.
 */
cvox.TraverseTable.prototype.getRow = function() {
  var childRows = cvox.TableUtil.getChildRows(this.activeTable_);
  return childRows[this.currentCellCursor[0]];
};


/**
 * Gets the table summary text.
 *
 * @return {?string} Either:
 *     1) The table summary text
 *     2) Null if the table does not contain a summary attribute.
 */
cvox.TraverseTable.prototype.summaryText = function() {
  // see http://code.google.com/p/chromium/issues/detail?id=46567
  // for information why this is necessary
  if (!this.activeTable_.hasAttribute('summary')) {
    return null;
  }
  return this.activeTable_.getAttribute('summary');
};


/**
 * Gets the table caption text.
 *
 * @return {?string} Either:
 *     1) The table caption text
 *     2) Null if the table does not include a caption tag.
 */
cvox.TraverseTable.prototype.captionText = function() {
  // If there's more than one outer <caption> element, choose the first one.
  var captionNodes = cvox.XpathUtil.evalXPath('caption\[1]',
      this.activeTable_);
  if (captionNodes.length > 0) {
    return captionNodes[0].innerHTML;
  } else {
    return null;
  }
};


/**
 * Calculates the number of columns in the shadow table.
 * @return {number} The number of columns in the shadow table.
 * @private
 */
cvox.TraverseTable.prototype.shadowColCount_ = function() {
  // As the shadow table is a 2D array, the number of columns is the
  // max number of elements in the second-level arrays.
  var max = 0;
  for (var i = 0; i < this.shadowTable_.length; i++) {
    if (this.shadowTable_[i].length > max) {
      max = this.shadowTable_[i].length;
    }
  }
  return max;
};


/**
 * Calculates the number of rows in the table.
 * @return {number} The number of rows in the table.
 * @private
 */
cvox.TraverseTable.prototype.countRows_ = function() {
  // Number of rows in a table is equal to the number of TR elements contained
  // by the (outer) TBODY elements.
  var rowCount = cvox.TableUtil.getChildRows(this.activeTable_);
  return rowCount.length;
};


/**
 * Calculates the number of columns in the table.
 * This uses the W3C recommended algorithm for calculating number of
 * columns, but it does not take rowspans or colspans into account. This means
 * that the number of columns calculated here might be lower than the actual
 * number of columns in the table if columns are indicated by colspans.
 * @return {number} The number of columns in the table.
 * @private
 */
cvox.TraverseTable.prototype.getW3CColCount_ = function() {
  // See http://www.w3.org/TR/html401/struct/tables.html#h-11.2.4.3

  var colgroupNodes = cvox.XpathUtil.evalXPath('child::colgroup',
      this.activeTable_);
  var colNodes = cvox.XpathUtil.evalXPath('child::col', this.activeTable_);

  if ((colgroupNodes.length == 0) && (colNodes.length == 0)) {
    var maxcols = 0;
    var outerChildren = cvox.TableUtil.getChildRows(this.activeTable_);
    for (var i = 0; i < outerChildren.length; i++) {
      var childrenCount = cvox.TableUtil.getChildCells(outerChildren[i]);
      if (childrenCount.length > maxcols) {
        maxcols = childrenCount.length;
      }
    }
    return maxcols;
  } else {
    var sum = 0;
    for (var i = 0; i < colNodes.length; i++) {
      if (colNodes[i].hasAttribute('span')) {
        sum += colNodes[i].getAttribute('span');
      } else {
        sum += 1;
      }
    }
    for (i = 0; i < colgroupNodes.length; i++) {
      var colChildren = cvox.XpathUtil.evalXPath('child::col',
          colgroupNodes[i]);
      if (colChildren.length == 0) {
        if (colgroupNodes[i].hasAttribute('span')) {
          sum += colgroupNodes[i].getAttribute('span');
        } else {
          sum += 1;
        }
      }
    }
  }
  return sum;
};


/**
 * Moves to the next row in the table. Updates the cell cursor.
 *
 * @return {boolean} Either:
 *    1) True if the update has been made.
 *    2) False if the end of the table has been reached and the update has not
 *       happened.
  */
cvox.TraverseTable.prototype.nextRow = function() {
  if (!this.currentCellCursor) {
    // We have not started moving through the table yet
    return this.goToRow(0);
  } else {
    return this.goToRow(this.currentCellCursor[0] + 1);
  }

};


/**
 * Moves to the previous row in the table. Updates the cell cursor.
 *
 * @return {boolean} Either:
 *    1) True if the update has been made.
 *    2) False if the end of the table has been reached and the update has not
 *       happened.
 */
cvox.TraverseTable.prototype.prevRow = function() {
  if (!this.currentCellCursor) {
    // We have not started moving through the table yet
    return this.goToRow(this.rowCount - 1);
  } else {
    return this.goToRow(this.currentCellCursor[0] - 1);
  }
};


/**
 * Moves to the next column in the table. Updates the cell cursor.
 *
 * @return {boolean} Either:
 *    1) True if the update has been made.
 *    2) False if the end of the table has been reached and the update has not
 *       happened.
 */
cvox.TraverseTable.prototype.nextCol = function() {
  if (!this.currentCellCursor) {
    // We have not started moving through the table yet
    return this.goToCol(0);
  } else {
    return this.goToCol(this.currentCellCursor[1] + 1);
  }
};


/**
 * Moves to the previous column in the table. Updates the cell cursor.
 *
 * @return {boolean} Either:
 *    1) True if the update has been made.
 *    2) False if the end of the table has been reached and the update has not
 *       happened.
 */
cvox.TraverseTable.prototype.prevCol = function() {
  if (!this.currentCellCursor) {
    // We have not started moving through the table yet
    return this.goToCol(this.shadowColCount_() - 1);
  } else {
    return this.goToCol(this.currentCellCursor[1] - 1);
  }
};


/**
 * Moves to the row at the specified index in the table. Updates the cell
 * cursor.
 * @param {number} index The index of the required row.
 * @return {boolean} Either:
 *    1) True if the index is valid and the update has been made.
 *    2) False if the index is not valid (either less than 0 or greater than
 *       the number of rows in the table).
 */
cvox.TraverseTable.prototype.goToRow = function(index) {
  if (this.shadowTable_[index] != null) {
    if (this.currentCellCursor == null) {
      // We haven't started moving through the table yet
      this.currentCellCursor = [index, 0];
    } else {
      this.currentCellCursor = [index, this.currentCellCursor[1]];
    }
    return true;
  } else {
    return false;
  }
};


/**
 * Moves to the column at the specified index in the table. Updates the cell
 * cursor.
 * @param {number} index The index of the required column.
 * @return {boolean} Either:
 *    1) True if the index is valid and the update has been made.
 *    2) False if the index is not valid (either less than 0 or greater than
 *       the number of rows in the table).
 */
cvox.TraverseTable.prototype.goToCol = function(index) {
  if (index < 0 || index >= this.colCount) {
    return false;
  }
  if (this.currentCellCursor == null) {
    // We haven't started moving through the table yet
    this.currentCellCursor = [0, index];
  } else {
    this.currentCellCursor = [this.currentCellCursor[0], index];
  }
  return true;
};


/**
 * Moves to the cell at the specified index <i, j> in the table. Updates the
 * cell cursor.
 * @param {Array<number>} index The index <i, j> of the required cell.
 * @return {boolean} Either:
 *    1) True if the index is valid and the update has been made.
 *    2) False if the index is not valid (either less than 0, greater than
 *       the number of rows or columns in the table, or there is no cell
 *       at that location).
 */
cvox.TraverseTable.prototype.goToCell = function(index) {
  if (((index[0] < this.rowCount) && (index[0] >= 0)) &&
      ((index[1] < this.colCount) && (index[1] >= 0))) {
    var cell = this.shadowTable_[index[0]][index[1]];
    if (cell != null) {
      this.currentCellCursor = index;
      return true;
    }
  }
  return false;
};


/**
 * Moves to the cell at the last index in the table. Updates the cell cursor.
 * @return {boolean} Either:
 *    1) True if the index is valid and the update has been made.
 *    2) False if the index is not valid (there is no cell at that location).
 */
cvox.TraverseTable.prototype.goToLastCell = function() {
  var numRows = this.shadowTable_.length;
  if (numRows == 0) {
    return false;
  }
  var lastRow = this.shadowTable_[numRows - 1];
  var lastIndex = [(numRows - 1), (lastRow.length - 1)];
  var cell =
      this.shadowTable_[lastIndex[0]][lastIndex[1]];
  if (cell != null) {
    this.currentCellCursor = lastIndex;
    return true;
  }
  return false;
};


/**
 * Moves to the cell at the last index in the current row  of the table. Update
 * the cell cursor.
 * @return {boolean} Either:
 *    1) True if the index is valid and the update has been made.
 *    2) False if the index is not valid (there is no cell at that location).
 */
cvox.TraverseTable.prototype.goToRowLastCell = function() {
  var currentRow = this.currentCellCursor[0];
  var lastIndex = [currentRow, (this.shadowTable_[currentRow].length - 1)];
  var cell =
      this.shadowTable_[lastIndex[0]][lastIndex[1]];
  if (cell != null) {
    this.currentCellCursor = lastIndex;
    return true;
  }
  return false;
};


/**
 * Moves to the cell at the last index in the current column  of the table.
 * Update the cell cursor.
 * @return {boolean} Either:
 *    1) True if the index is valid and the update has been made.
 *    2) False if the index is not valid (there is no cell at that location).
 */
cvox.TraverseTable.prototype.goToColLastCell = function() {
  var currentCol = this.getCol();
  var lastIndex = [(currentCol.length - 1), this.currentCellCursor[1]];
  var cell =
      this.shadowTable_[lastIndex[0]][lastIndex[1]];
  if (cell != null) {
    this.currentCellCursor = lastIndex;
    return true;
  }
  return false;
};


/**
 * Resets the table cursors.
 *
 */
cvox.TraverseTable.prototype.resetCursor = function() {
  this.currentCellCursor = null;
};
