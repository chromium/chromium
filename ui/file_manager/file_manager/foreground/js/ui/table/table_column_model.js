// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This is a table column model
 */

import {dispatchSimpleEvent} from 'chrome://resources/ash/common/cr_deprecated.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import {TableColumn} from './table_column.js';

/** @type {number} */
const MIMIMAL_WIDTH = 10;

/**
 * A table column model that wraps table columns array
 * This implementation supports widths in percents.
 */
export class TableColumnModel extends EventTarget {
  /**
   * @param {!Array<TableColumn>} tableColumns Array of table
   *     columns.
   */
  constructor(tableColumns) {
    super();

    /** @type {!Array<TableColumn>} */
    this.columns_ = [];
    for (let i = 0; i < tableColumns.length; i++) {
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      this.columns_.push(tableColumns[i].clone());
    }
  }

  /**
   * The number of the columns.
   * @type {number}
   */
  get size() {
    return this.columns_.length;
  }

  /**
   * Returns id of column at the given index.
   * @param {number} index The index of the column.
   * @return {string} Column id.
   */
  getId(index) {
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    return this.columns_[index].id;
  }

  /**
   * Returns name of column at the given index. Name is used as column header
   * label.
   * @param {number} index The index of the column.
   * @return {string} Column name.
   */
  getName(index) {
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    return this.columns_[index].name;
  }

  /**
   * Sets name of column at the given index.
   * @param {number} index The index of the column.
   * @param {string} name Column name.
   */
  setName(index, name) {
    if (index < 0 || index >= this.columns_.length) {
      return;
    }
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    if (name != this.columns_[index].name) {
      return;
    }

    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    this.columns_[index].name = name;
    dispatchSimpleEvent(this, 'change');
  }

  /**
   * Returns width (in percent) of column at the given index.
   * @param {number} index The index of the column.
   * @return {number} Column width in pixels.
   */
  getWidth(index) {
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    return this.columns_[index].width;
  }

  /**
   * Check if the column at the given index should align to the end.
   * @param {number} index The index of the column.
   * @return {boolean} True if the column is aligned to end.
   */
  isEndAlign(index) {
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    return this.columns_[index].endAlign;
  }

  /**
   * Sets width of column at the given index.
   * @param {number} index The index of the column.
   * @param {number} width Column width.
   */
  setWidth(index, width) {
    if (index < 0 || index >= this.columns_.length) {
      return;
    }

    const column = this.columns_[index];
    width = Math.max(width, MIMIMAL_WIDTH);
    // @ts-ignore: error TS18048: 'column' is possibly 'undefined'.
    if (width == column.absoluteWidth) {
      return;
    }

    // @ts-ignore: error TS18048: 'column' is possibly 'undefined'.
    column.width = width;

    // Dispatch an event if a visible column was resized.
    // @ts-ignore: error TS18048: 'column' is possibly 'undefined'.
    if (column.visible) {
      dispatchSimpleEvent(this, 'resize');
    }
  }

  /**
   * Returns render function for the column at the given index.
   * @param {number} index The index of the column.
   * @return {function(*, string, Element): HTMLElement} Render function.
   */
  getRenderFunction(index) {
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    return this.columns_[index].renderFunction;
  }

  /**
   * Sets render function for the column at the given index.
   * @param {number} index The index of the column.
   * @param {function(*, string, Element): HTMLElement} renderFunction
   *     Render function.
   */
  setRenderFunction(index, renderFunction) {
    if (index < 0 || index >= this.columns_.length) {
      return;
    }
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    if (renderFunction !== this.columns_[index].renderFunction) {
      return;
    }

    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    this.columns_[index].renderFunction = renderFunction;
    dispatchSimpleEvent(this, 'change');
  }

  /**
   * Render the column header.
   * @param {number} index The index of the column.
   * @param {Element} table Owner table.
   */
  renderHeader(index, table) {
    const c = this.columns_[index];
    // @ts-ignore: error TS18048: 'c' is possibly 'undefined'.
    return c.headerRenderFunction.call(c, table);
  }

  /**
   * The total width of the columns.
   * @type {number}
   */
  get totalWidth() {
    let total = 0;
    for (let i = 0; i < this.size; i++) {
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      total += this.columns_[i].width;
    }
    return total;
  }

  /**
   * Normalizes widths to make their sum 100%.
   */
  // @ts-ignore: error TS7006: Parameter 'contentWidth' implicitly has an 'any'
  // type.
  normalizeWidths(contentWidth) {
    if (this.size == 0) {
      return;
    }
    const c = this.columns_[0];
    // @ts-ignore: error TS18048: 'c' is possibly 'undefined'.
    c.width = Math.max(10, c.width - this.totalWidth + contentWidth);
  }

  /**
   * Returns default sorting order for the column at the given index.
   * @param {number} index The index of the column.
   * @return {string} 'asc' or 'desc'.
   */
  getDefaultOrder(index) {
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    return this.columns_[index].defaultOrder;
  }

  /**
   * Returns index of the column with given id.
   * @param {string} id The id to find.
   * @return {number} The index of column with given id or -1 if not found.
   */
  indexOf(id) {
    for (let i = 0; i < this.size; i++) {
      if (this.getId(i) == id) {
        return i;
      }
    }
    return -1;
  }

  /**
   * Show/hide a column.
   * @param {number} index The column index.
   * @param {boolean} visible The column visibility.
   */
  setVisible(index, visible) {
    if (index < 0 || index >= this.columns_.length) {
      return;
    }

    const column = this.columns_[index];
    // @ts-ignore: error TS18048: 'column' is possibly 'undefined'.
    if (column.visible == visible) {
      return;
    }

    // Changing column visibility alters the width.  Save the total width out
    // first, then change the column visibility, then relayout the table.
    const contentWidth = this.totalWidth;
    // @ts-ignore: error TS18048: 'column' is possibly 'undefined'.
    column.visible = visible;
    this.normalizeWidths(contentWidth);
  }

  /**
   * Returns a column's visibility.
   * @param {number} index The column index.
   * @return {boolean} Whether the column is visible.
   */
  isVisible(index) {
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    return this.columns_[index].visible;
  }
}
