/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Table editing support.
 * This file provides one of the supporting classes for goog.editor.Table, the
 * goog.editor.TableCell.
 */
goog.module('goog.editor.TableCell');
goog.module.declareLegacyNamespace();

/**
 * Class representing a table cell, which may span across multiple
 * rows and columns
 * @param {!Element} td This cell's underlying TD or TH element.
 * @param {number} startRow Index of the row where this cell begins.
 * @param {number} startCol Index of the column where this cell begins.
 * @constructor
 * @final
 */
const TableCell = function(td, startRow, startCol) {
  this.element = td;
  /** @suppress {strictMissingProperties} Added to tighten compiler checks */
  this.colSpan = parseInt(td.colSpan, 10) || 1;
  /** @suppress {strictMissingProperties} Added to tighten compiler checks */
  this.rowSpan = parseInt(td.rowSpan, 10) || 1;
  this.startRow = startRow;
  this.startCol = startCol;
  this.updateCoordinates_();
};

/**
 * Calculates this cell's endRow/endCol coordinates based on rowSpan/colSpan
 * @private
 */
TableCell.prototype.updateCoordinates_ = function() {
  /** @suppress {strictMissingProperties} Added to tighten compiler checks */
  this.endCol = this.startCol + this.colSpan - 1;
  /** @suppress {strictMissingProperties} Added to tighten compiler checks */
  this.endRow = this.startRow + this.rowSpan - 1;
};


/**
 * Set this cell's colSpan, updating both its colSpan property and the
 * underlying element's colSpan attribute.
 * @param {number} colSpan The new colSpan.
 * @suppress {strictMissingProperties} Added to tighten compiler checks
 */
TableCell.prototype.setColSpan = function(colSpan) {
  if (colSpan != this.colSpan) {
    if (colSpan > 1) {
      /**
       * @suppress {strictMissingProperties} Added to tighten compiler checks
       */
      this.element.colSpan = colSpan;
    } else {
      this.element.colSpan = 1, this.element.removeAttribute('colSpan');
    }
    this.colSpan = colSpan;
    this.updateCoordinates_();
  }
};


/**
 * Set this cell's rowSpan, updating both its rowSpan property and the
 * underlying element's rowSpan attribute.
 * @param {number} rowSpan The new rowSpan.
 */
TableCell.prototype.setRowSpan = function(rowSpan) {
  if (rowSpan != this.rowSpan) {
    if (rowSpan > 1) {
      /**
       * @suppress {strictMissingProperties} Added to tighten compiler checks
       */
      this.element.rowSpan = rowSpan.toString();
    } else {
      /**
       * @suppress {strictMissingProperties} Added to tighten compiler checks
       */
      this.element.rowSpan = '1';
      this.element.removeAttribute('rowSpan');
    }
    this.rowSpan = rowSpan;
    this.updateCoordinates_();
  }
};

exports = TableCell;
