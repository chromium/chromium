// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A class for walking tables.
 * NOTE: This class has a very different interface than the other walkers.
 * This means it does not lend itself easily to e.g. decorators.
 * TODO (stoarca): This might be able to be fixed by breaking it up into
 * separate walkers for cell, row and column.
 */


goog.provide('cvox.TableWalker');

goog.require('cvox.AbstractWalker');
goog.require('cvox.BrailleUtil');
goog.require('cvox.DescriptionUtil');
goog.require('cvox.DomUtil');
goog.require('cvox.NavDescription');
goog.require('cvox.QueueMode');
goog.require('cvox.TraverseTable');

/**
 * @constructor
 * @extends {cvox.AbstractWalker}
 */
cvox.TableWalker = function() {
  cvox.AbstractWalker.call(this);

  /**
   * Only used as a cache for faster lookup.
   * @type {!cvox.TraverseTable}
   */
  this.tt = new cvox.TraverseTable(null);
};
goog.inherits(cvox.TableWalker, cvox.AbstractWalker);

/**
 * @override
 */
cvox.TableWalker.prototype.next = function(sel) {
  // TODO (stoarca): See bug 6677953
  return this.nextRow(sel);
};

/**
 * @override
 */
cvox.TableWalker.prototype.sync = function(sel) {
  return this.goTo_(sel, goog.bind(function(position) {
      return this.tt.goToCell(position);
  }, this));
};

/**
 * @override
 * @suppress {checkTypes} actual parameter 2 of
 * Msgs.prototype.getMsg does not match formal parameter
 * found   : Array<number>
 * required: (Array<string>|null|undefined)
 */
cvox.TableWalker.prototype.getDescription = function(prevSel, sel) {
  var position = this.syncPosition_(sel);
  if (!position) {
    return [];
  }
  this.tt.goToCell(position);
  var descs = cvox.DescriptionUtil.getCollectionDescription(prevSel, sel);
  if (descs.length == 0) {
    descs.push(new cvox.NavDescription({
      annotation: Msgs.getMsg('empty_cell')
    }));
  }
  return descs;
};

/**
 * @override
 */
cvox.TableWalker.prototype.getBraille = function(prevSel, sel) {
  var ret = new cvox.NavBraille({});
  var position = this.syncPosition_(sel);
  if (position) {
    var text =
        cvox.BrailleUtil.getTemplated(prevSel.start.node, sel.start.node);
    text.append(' ' + ++position[0] + '/' + ++position[1]);
  }
  return new cvox.NavBraille({text: text});
};

/**
 * @override
 */
cvox.TableWalker.prototype.getGranularityMsg = goog.abstractMethod;


/** Table Actions. */


/**
 * Returns the first cell of the table that this selection is inside.
 * @param {!cvox.CursorSelection} sel The selection.
 * @return {cvox.CursorSelection} The selection for first cell of the table.
 * @export
 */
cvox.TableWalker.prototype.goToFirstCell = function(sel) {
  return this.goTo_(sel, goog.bind(function(position) {
    return this.tt.goToCell([0, 0]);
  }, this));
};

/**
 * Returns the last cell of the table that this selection is inside.
 * @param {!cvox.CursorSelection} sel The selection.
 * @return {cvox.CursorSelection} The selection for the last cell of the table.
 * @export
 */
cvox.TableWalker.prototype.goToLastCell = function(sel) {
  return this.goTo_(sel, goog.bind(function(position) {
    return this.tt.goToLastCell();
  }, this));
};

/**
 * Returns the first cell of the row that the selection is in.
 * @param {!cvox.CursorSelection} sel The selection.
 * @return {cvox.CursorSelection} The selection for the first cell in the row.
 * @export
 */
cvox.TableWalker.prototype.goToRowFirstCell = function(sel) {
  return this.goTo_(sel, goog.bind(function(position) {
    return this.tt.goToCell([position[0], 0]);
  }, this));
};

/**
 * Returns the last cell of the row that the selection is in.
 * @param {!cvox.CursorSelection} sel The selection.
 * @return {cvox.CursorSelection} The selection for the last cell in the row.
 * @export
 */
cvox.TableWalker.prototype.goToRowLastCell = function(sel) {
  return this.goTo_(sel, goog.bind(function(position) {
    return this.tt.goToRowLastCell();
  }, this));
};

/**
 * Returns the first cell of the column that the selection is in.
 * @param {!cvox.CursorSelection} sel The selection.
 * @return {cvox.CursorSelection} The selection for the first cell in the col.
 * @export
 */
cvox.TableWalker.prototype.goToColFirstCell = function(sel) {
  return this.goTo_(sel, goog.bind(function(position) {
    return this.tt.goToCell([0, position[1]]);
  }, this));
};

/**
 * Returns the last cell of the column that the selection is in.
 * @param {!cvox.CursorSelection} sel The selection.
 * @return {cvox.CursorSelection} The selection for the last cell in the col.
 * @export
 */
cvox.TableWalker.prototype.goToColLastCell = function(sel) {
  return this.goTo_(sel, goog.bind(function(position) {
    return this.tt.goToColLastCell();
  }, this));
};

/**
 * Returns the first cell in the row after the current selection.
 * @param {!cvox.CursorSelection} sel The selection.
 * @return {cvox.CursorSelection} The selection for the first cell in the next
 * row.
 * @export
 */
cvox.TableWalker.prototype.nextRow = function(sel) {
  return this.goTo_(sel, goog.bind(function(position) {
    return this.tt.goToCell([position[0] + (sel.isReversed() ? -1 : 1),
                              position[1]]);
  }, this));
};

/**
 * Returns the first cell in the column after the current selection.
 * @param {!cvox.CursorSelection} sel The selection.
 * @return {cvox.CursorSelection} The selection for the first cell in the
 * next col.
 * @export
 */
cvox.TableWalker.prototype.nextCol = function(sel) {
  return this.goTo_(sel, goog.bind(function(position) {
    return this.tt.goToCell([position[0],
                              position[1] + (sel.isReversed() ? -1 : 1)]);
  }, this));
};

/**
 * @param {!cvox.CursorSelection} sel The current selection.
 * @return {cvox.CursorSelection} The resulting selection.
 * @export
 */
cvox.TableWalker.prototype.announceHeaders = function(sel) {
  cvox.ChromeVox.tts.speak(this.getHeaderText_(sel),
                           cvox.QueueMode.FLUSH,
                           cvox.AbstractTts.PERSONALITY_ANNOTATION);
  return sel;
};

/**
 * @param {!cvox.CursorSelection} sel The current selection.
 * @return {cvox.CursorSelection} The resulting selection.
 * @export
 */
cvox.TableWalker.prototype.speakTableLocation = function(sel) {
  cvox.ChromeVox.navigationManager.speakDescriptionArray(
      this.getLocationDescription_(sel),
      cvox.QueueMode.FLUSH,
      null);
  return sel;
};


/**
 * @param {!cvox.CursorSelection} sel The current selection.
 * @return {cvox.CursorSelection} The resulting selection.
 * @export
 */
cvox.TableWalker.prototype.exitShifterContent = function(sel) {
  var tableNode = this.getTableNode_(sel);
  if (!tableNode) {
    return null;
  }
  var nextNode = cvox.DomUtil.directedNextLeafNode(tableNode, false);
  return cvox.CursorSelection.fromNode(nextNode);
};


/** End of actions. */


/**
 * Returns the text content of the header(s) of the cell that contains sel.
 * @param {!cvox.CursorSelection} sel The selection.
 * @return {!string} The header text.
 * @private
 */
cvox.TableWalker.prototype.getHeaderText_ = function(sel) {
  this.tt.initialize(this.getTableNode_(sel));
  var position = this.tt.findNearestCursor(sel.start.node);
  if (!position) {
    return Msgs.getMsg('not_inside_table');
  }
  if (!this.tt.goToCell(position)) {
    return Msgs.getMsg('not_inside_table');
  }
  return (
      this.getRowHeaderText_(position) +
      ' ' +
      this.getColHeaderText_(position));
};

/**
 * Returns the location description.
 * @param {!cvox.CursorSelection} sel A valid selection.
 * @return {Array<cvox.NavDescription>} The location description.
 * @suppress {checkTypes} actual parameter 2 of
 * Msgs.prototype.getMsg does not match
 * formal parameter
 * found   : Array<number>
 * required: (Array<string>|null|undefined)
 * @private
 */
cvox.TableWalker.prototype.getLocationDescription_ = function(sel) {
  var locationInfo = this.getLocationInfo(sel);
  if (locationInfo == null) {
    return null;
  }
  return [new cvox.NavDescription({
    text: Msgs.getMsg('table_location', locationInfo)
  })];
};

/**
 * Returns the text content of the row header(s) of the cell that contains sel.
 * @param {!Array<number>} position The selection.
 * @return {!string} The header text.
 * @private
 */
cvox.TableWalker.prototype.getRowHeaderText_ = function(position) {
  // TODO(stoarca): OPTMZ Replace with join();
  var rowHeaderText = '';

  var rowHeaders = this.tt.getCellRowHeaders();
  if (rowHeaders.length == 0) {
    var firstCellInRow = this.tt.getCellAt([position[0], 0]);
    rowHeaderText += cvox.DomUtil.collapseWhitespace(
        cvox.DomUtil.getValue(firstCellInRow) + ' ' +
            cvox.DomUtil.getName(firstCellInRow));
    return Msgs.getMsg('row_header') + rowHeaderText;
  }

  for (var i = 0; i < rowHeaders.length; ++i) {
    rowHeaderText += cvox.DomUtil.collapseWhitespace(
        cvox.DomUtil.getValue(rowHeaders[i]) + ' ' +
            cvox.DomUtil.getName(rowHeaders[i]));
  }
  if (rowHeaderText == '') {
    return Msgs.getMsg('empty_row_header');
  }
  return Msgs.getMsg('row_header') + rowHeaderText;
};

/**
 * Returns the text content of the col header(s) of the cell that contains sel.
 * @param {!Array<number>} position The selection.
 * @return {!string} The header text.
 * @private
 */
cvox.TableWalker.prototype.getColHeaderText_ = function(position) {
  // TODO(stoarca): OPTMZ Replace with join();
  var colHeaderText = '';

  var colHeaders = this.tt.getCellColHeaders();
  if (colHeaders.length == 0) {
    var firstCellInCol = this.tt.getCellAt([0, position[1]]);
    colHeaderText += cvox.DomUtil.collapseWhitespace(
        cvox.DomUtil.getValue(firstCellInCol) + ' ' +
        cvox.DomUtil.getName(firstCellInCol));
    return Msgs.getMsg('column_header') + colHeaderText;
  }

  for (var i = 0; i < colHeaders.length; ++i) {
    colHeaderText += cvox.DomUtil.collapseWhitespace(
        cvox.DomUtil.getValue(colHeaders[i]) + ' ' +
            cvox.DomUtil.getName(colHeaders[i]));
  }
  if (colHeaderText == '') {
    return Msgs.getMsg('empty_row_header');
  }
  return Msgs.getMsg('column_header') + colHeaderText;
};

/**
 * Returns the location info of sel within the containing table.
 * @param {!cvox.CursorSelection} sel The selection.
 * @return {Array<number>} The location info:
 *  [row index, row count, col index, col count].
 */
cvox.TableWalker.prototype.getLocationInfo = function(sel) {
  this.tt.initialize(this.getTableNode_(sel));
  var position = this.tt.findNearestCursor(sel.start.node);
  if (!position) {
    return null;
  }
  // + 1 to account for 0-indexed
  return [
    position[0] + 1,
    this.tt.rowCount,
    position[1] + 1,
    this.tt.colCount
  ].map(function(x) {return Msgs.getNumber(x);});
};

/**
 * Returns true if sel is inside a table.
 * @param {!cvox.CursorSelection} sel The selection.
 * @return {boolean} True if inside a table node.
 */
cvox.TableWalker.prototype.isInTable = function(sel) {
  return this.getTableNode_(sel) != null;
};

/**
 * Wrapper for going to somewhere so that boilerplate is not repeated.
 * @param {!cvox.CursorSelection} sel The selection from which to base the
 * movement.
 * @param {function(Array<number>):boolean} f The function to use for moving.
 * Returns true on success and false on failure.
 * @return {cvox.CursorSelection} The resulting selection.
 * @private
 */
cvox.TableWalker.prototype.goTo_ = function(sel, f) {
  if (!sel.end.node) {
    return null;
  }
  this.tt.initialize(this.getTableNode_(sel));
  var position = this.tt.findNearestCursor(sel.end.node);
  if (!position) {
    return null;
  }
  this.tt.goToCell(position);
  if (!f(position)) {
    return null;
  }
  return cvox.CursorSelection.fromNode(this.tt.getCell()).
      setReversed(sel.isReversed());
};

/**
 * Returns the nearest table node containing the end of the selection
 * @param {!cvox.CursorSelection} sel The selection.
 * @return {Node} The table node containing sel. null if not in a table.
 * @private
 */
cvox.TableWalker.prototype.getTableNode_ = function(sel) {
  return cvox.DomUtil.getContainingTable(sel.end.node);
};

/**
 * Sync the backing traversal utility to the given selection.
 * @param {!cvox.CursorSelection} sel The selection.
 * @return {Array<number>} The position [x, y] of the selection.
 * @private
 */
cvox.TableWalker.prototype.syncPosition_ = function(sel) {
  var tableNode = this.getTableNode_(sel);
  this.tt.initialize(tableNode);
  // we need to align the TraverseTable with our sel because our walker
  // uses parts of it (for example isSpanned relies on being at a specific cell)
  return this.tt.findNearestCursor(sel.end.node);
};
