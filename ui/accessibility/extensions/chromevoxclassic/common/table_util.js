// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview DOM utility functions to aid in table navigation.
 */

goog.provide('cvox.TableUtil');

goog.require('cvox.XpathUtil');


/**
 * Utility function to check if a particular table cell is a candidate
 * header cell.
 * @param {Node} cell The table cell.
 * @return {boolean} Whether or not the table cell is acting as a header cell.
 */
cvox.TableUtil.checkIfHeader = function(cell) {
  /*
   * Headers are defined here as <TH> or <TD> elements. <TD> elements when
   * serving as header cells must have either:
   *  - The scope attribute defined
   *  - Their IDs referenced in the header content attribute of another <TD> or
   *  <TH> element.
   * This function does not check whether this cell is referenced by another
   * <TD>. So this function by itself will not be able to gather all possible
   * header cells when applied across all table cells.
   *
   * Please Note:
   * The HTML5 spec specifies that only header <TH> elements can be headers
   * ( http://dev.w3.org/html5/spec/tabular-data.html#row-header ) but the
   * HTML4 spec says that <TD> elements can act as headers if they have a
   * scope attribute defined
   * ( http://www.w3.org/TR/html401/struct/tables.html#h-11.2.6 ). In the
   * interest of providing meaningful header information for all tables, here
   * we take the position that <TD> elements can act as headers.
   */
  return ((cell.tagName == 'TH') ||
      cell.hasAttribute('scope') || (cell.hasAttribute('role') &&
          ((cell.getAttribute('role') == 'rowheader') ||
              (cell.getAttribute('role') == 'columnheader'))));
};


/**
 * Utility function to determine colgroup structure. Builds an array that
 * associates a column number to a particular col group.
 * @param {Array} colGroups An array of all the colgroup elements in a
 * particular table.
 * @return {Array} An array that maps indexes representing table columns
 * to indexes into the colGroups array.
 */
cvox.TableUtil.determineColGroups = function(colGroups) {
  var colToColGroup = [];

  if (colGroups.length == 0) {
    return colToColGroup;
  }
  // A colgroup has either a series of col element children or a span
  // attribute. If it has col children, ignore the span attribute
  for (var colGroupCtr = 0; colGroupCtr < colGroups.length;
       colGroupCtr++) {

    var currentColGroup = colGroups[colGroupCtr];

    var childCols = cvox.TableUtil.getColNodes(currentColGroup);
    if (childCols.length > 0) {
      for (var colNodeCtr = 0; colNodeCtr < childCols.length;
          colNodeCtr++) {
        var colElement = childCols[colNodeCtr];

        if (colElement.hasAttribute('span')) {
          var span = parseInt(colElement.getAttribute('span'), 10);

          for (var s = 0; s < span; s++) {
            colToColGroup.push(colGroupCtr);
          }
        } else {
          colToColGroup.push(colGroupCtr);
        }
      }
    } else {
      // No children of the current colgroup. Does it have a span attribute?
      if (currentColGroup.hasAttribute('span')) {
        var span = parseInt(currentColGroup.getAttribute('span'), 10);

        for (var s = 0; s < span; s++) {
          colToColGroup.push(colGroupCtr);
        }
      } else {
        // Default span value is 1
        colToColGroup.push(colGroupCtr);
      }
    }
  }
  return colToColGroup;

};


/**
 * Utility function to push an element into a given array only if that element
 * is not already contained in the array.
 * @param {Array} givenArray The given array.
 * @param {Node} givenElement The given element.
 */
cvox.TableUtil.pushIfNotContained = function(givenArray, givenElement) {
  if (givenArray.indexOf(givenElement) == -1) {
    givenArray.push(givenElement);
  }
};


/**
 * Returns a JavaScript array of all the non-nested rows in the given table.
 *
 * @param {Node} table A table node.
 * @return {Array} An array of all the child rows of the active table.
 */
cvox.TableUtil.getChildRows = function(table) {
  return cvox.XpathUtil.evalXPath('child::tbody/tr | child::thead/tr | ' +
      'child::*[attribute::role="row"]', table);
};


/**
 * Returns a JavaScript array of all the child cell <TD> or <TH> or
 * role='gridcell' nodes of the given row.
 *
 * @param {Node} rowNode The specified row node.
 * @return {Array} An array of all the child cells of the given row node.
 */
cvox.TableUtil.getChildCells = function(rowNode) {
  return cvox.XpathUtil.evalXPath('child::td | child::th | ' +
      'child::*[attribute::role="gridcell"] |' +
      'child::*[attribute::role="rowheader"] |' +
      'child::*[attribute::role="columnheader"]', rowNode);
};


/**
 * Returns a JavaScript array containing the cell in the active table
 * with the given ID.
 *
 * @param {Node} table A table node.
 * @param {string} cellID The specified ID.
 * @return {Array} An array containing the cell with the specified ID.
 */
cvox.TableUtil.getCellWithID = function(table, cellID) {
  return cvox.XpathUtil.evalXPath('id(\'' + cellID + '\')', table);
};


/**
 * Returns a Javascript array containing the colgroup elements in the
 * active table.
 *
 * @param {Node} table A table node.
 * @return {Array} An array of all the colgroup elements in the active table.
 */
cvox.TableUtil.getColGroups = function(table) {
  return cvox.XpathUtil.evalXPath('child::colgroup', table);
};


/**
 * Returns a Javascript array containing the child col elements of the given
 * colgroup element.
 *
 * @param {Node} colGroupNode The specified <COLGROUP> element.
 * @return {Array} An array of all of the child col elements of the given
 * colgroup element.
 */
cvox.TableUtil.getColNodes = function(colGroupNode) {
  return cvox.XpathUtil.evalXPath('child::col', colGroupNode);
};

