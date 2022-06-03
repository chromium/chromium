/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Table editing support.
 * This file provides one of the supporting classes for goog.editor.Table, the
 * goog.editor.TableRow.
 */

goog.module('goog.editor.TableRow');
goog.module.declareLegacyNamespace();

/**
 * Class representing a logical table row: a tr element and any cells
 * that appear in that row.
 * @param {!Element} trElement This rows's underlying TR element.
 * @param {number} rowIndex This row's index in its parent table.
 * @constructor
 * @final
 */
const TableRow = function(trElement, rowIndex) {
  this.index = rowIndex;
  this.element = trElement;
  this.columns = [];
};

exports = TableRow;
