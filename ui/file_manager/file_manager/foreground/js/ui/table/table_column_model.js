// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This is a table column model
 */
cr.define('cr.ui.table', function() {
  /** @type {number} */
  const MIMIMAL_WIDTH = 10;

  /**
   * A table column model that wraps table columns array
   * This implementation supports widths in percents.
   */
  class TableColumnModel extends cr.EventTarget {
    /**
     * @param {!Array<cr.ui.table.TableColumn>} tableColumns Array of table
     *     columns.
     */
    constructor(tableColumns) {
      super();

      /** @type {!Array<cr.ui.table.TableColumn>} */
      this.columns_ = [];
      for (let i = 0; i < tableColumns.length; i++) {
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
      return this.columns_[index].id;
    }

    /**
     * Returns name of column at the given index. Name is used as column header
     * label.
     * @param {number} index The index of the column.
     * @return {string} Column name.
     */
    getName(index) {
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
      if (name != this.columns_[index].name) {
        return;
      }

      this.columns_[index].name = name;
      cr.dispatchSimpleEvent(this, 'change');
    }

    /**
     * Returns width (in percent) of column at the given index.
     * @param {number} index The index of the column.
     * @return {number} Column width in pixels.
     */
    getWidth(index) {
      return this.columns_[index].width;
    }

    /**
     * Check if the column at the given index should align to the end.
     * @param {number} index The index of the column.
     * @return {boolean} True if the column is aligned to end.
     */
    isEndAlign(index) {
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
      if (width == column.absoluteWidth) {
        return;
      }

      column.width = width;

      // Dispatch an event if a visible column was resized.
      if (column.visible) {
        cr.dispatchSimpleEvent(this, 'resize');
      }
    }

    /**
     * Returns render function for the column at the given index.
     * @param {number} index The index of the column.
     * @return {function(*, string, Element): HTMLElement} Render function.
     */
    getRenderFunction(index) {
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
      if (renderFunction !== this.columns_[index].renderFunction) {
        return;
      }

      this.columns_[index].renderFunction = renderFunction;
      cr.dispatchSimpleEvent(this, 'change');
    }

    /**
     * Render the column header.
     * @param {number} index The index of the column.
     * @param {Element} table Owner table.
     */
    renderHeader(index, table) {
      const c = this.columns_[index];
      return c.headerRenderFunction.call(c, table);
    }

    /**
     * The total width of the columns.
     * @type {number}
     */
    get totalWidth() {
      let total = 0;
      for (let i = 0; i < this.size; i++) {
        total += this.columns_[i].width;
      }
      return total;
    }

    /**
     * Normalizes widths to make their sum 100%.
     */
    normalizeWidths(contentWidth) {
      if (this.size == 0) {
        return;
      }
      const c = this.columns_[0];
      c.width = Math.max(10, c.width - this.totalWidth + contentWidth);
    }

    /**
     * Returns default sorting order for the column at the given index.
     * @param {number} index The index of the column.
     * @return {string} 'asc' or 'desc'.
     */
    getDefaultOrder(index) {
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
      if (column.visible == visible) {
        return;
      }

      // Changing column visibility alters the width.  Save the total width out
      // first, then change the column visibility, then relayout the table.
      const contentWidth = this.totalWidth;
      column.visible = visible;
      this.normalizeWidths(contentWidth);
    }

    /**
     * Returns a column's visibility.
     * @param {number} index The column index.
     * @return {boolean} Whether the column is visible.
     */
    isVisible(index) {
      return this.columns_[index].visible;
    }
  }

  return {TableColumnModel: TableColumnModel};
});
