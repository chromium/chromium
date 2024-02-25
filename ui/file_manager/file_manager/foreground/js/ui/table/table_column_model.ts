// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This is a table column model
 */

import {dispatchSimpleEvent} from 'chrome://resources/ash/common/cr_deprecated.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import type {TableColumn} from './table_column.js';
import {type RenderFunction} from './table_column.js';

const MIMIMAL_WIDTH = 10;

/**
 * A table column model that wraps table columns array
 * This implementation supports widths in percents.
 */
export class TableColumnModel extends EventTarget {
  protected columns_: TableColumn[] = [];

  /**
   * @param tableColumns Array of table columns.
   */
  constructor(tableColumns: TableColumn[]) {
    super();

    for (const column of tableColumns) {
      this.columns_.push(column.clone());
    }
  }

  /**
   * The number of the columns.
   */
  get size() {
    return this.columns_.length;
  }

  /**
   * Returns id of column at the given index.
   * @param index The index of the column.
   * @return Column id.
   */
  getId(index: number): string|undefined {
    return this.columns_[index]?.id;
  }

  /**
   * Returns name of column at the given index. Name is used as column header
   * label.
   * @param index The index of the column.
   * @return Column name.
   */
  getName(index: number): string {
    return this.columns_[index]?.name ?? '';
  }

  /**
   * Sets name of column at the given index.
   * @param index The index of the column.
   * @param name Column name.
   */
  setName(index: number, name: string) {
    if (index < 0 || index >= this.columns_.length) {
      return;
    }
    if (name !== this.columns_[index]!.name) {
      return;
    }

    this.columns_[index]!.name = name;
    dispatchSimpleEvent(this, 'change');
  }

  /**
   * Returns width (in percent) of column at the given index.
   * @param index The index of the column.
   * @return Column width in pixels.
   */
  getWidth(index: number): number {
    return this.columns_[index]?.width ?? 0;
  }

  /**
   * Check if the column at the given index should align to the end.
   * @param index The index of the column.
   * @return True if the column is aligned to end.
   */
  isEndAlign(index: number): boolean {
    return this.columns_[index]?.endAlign ?? false;
  }

  /**
   * Sets width of column at the given index.
   * @param index The index of the column.
   * @param width Column width.
   */
  setWidth(index: number, width: number) {
    if (index < 0 || index >= this.columns_.length) {
      return;
    }

    const column = this.columns_[index]!;
    width = Math.max(width, MIMIMAL_WIDTH);
    if (width === column.absoluteWidth) {
      return;
    }

    column.width = width;

    // Dispatch an event if a visible column was resized.
    if (column.visible) {
      dispatchSimpleEvent(this, 'resize');
    }
  }

  /**
   * Returns render function for the column at the given index.
   * @param index The index of the column.
   * @return Render function.
   */
  getRenderFunction(index: number): RenderFunction {
    return this.columns_[index]!.renderFunction;
  }

  /**
   * Sets render function for the column at the given index.
   * @param index The index of the column.
   */
  setRenderFunction(index: number, renderFunction: RenderFunction) {
    if (index < 0 || index >= this.columns_.length) {
      return;
    }
    if (renderFunction !== this.columns_[index]!.renderFunction) {
      return;
    }

    this.columns_[index]!.renderFunction = renderFunction;
    dispatchSimpleEvent(this, 'change');
  }

  /**
   * Render the column header.
   * @param index The index of the column.
   * @param table Owner table.
   */
  renderHeader(index: number, table: HTMLElement) {
    const c = this.columns_[index]!;
    return c.headerRenderFunction.call(c, table);
  }

  /**
   * The total width of the columns.
   */
  get totalWidth() {
    let total = 0;
    for (let i = 0; i < this.size; i++) {
      total += this.columns_[i]!.width;
    }
    return total;
  }

  /**
   * Normalizes widths to make their sum 100%.
   */
  normalizeWidths(contentWidth: number) {
    if (this.size === 0) {
      return;
    }
    const c = this.columns_[0]!;
    c.width = Math.max(10, c.width - this.totalWidth + contentWidth);
  }

  /**
   * Returns default sorting order for the column at the given index.
   * @param index The index of the column.
   * @return 'asc' or 'desc'.
   */
  getDefaultOrder(index: number): string {
    return this.columns_[index]!.defaultOrder;
  }

  /**
   * Returns index of the column with given id.
   * @param id The id to find.
   * @return The index of column with given id or -1 if not found.
   */
  indexOf(id: string): number {
    for (let i = 0; i < this.size; i++) {
      if (this.getId(i) === id) {
        return i;
      }
    }
    return -1;
  }

  /**
   * Show/hide a column.
   * @param index The column index.
   * @param visible The column visibility.
   */
  setVisible(index: number, visible: boolean) {
    if (index < 0 || index >= this.columns_.length) {
      return;
    }

    const column = this.columns_[index]!;
    if (column.visible === visible) {
      return;
    }

    // Changing column visibility alters the width.  Save the total width
    // out first, then change the column visibility, then relayout the
    // table.
    const contentWidth = this.totalWidth;
    column.visible = visible;
    this.normalizeWidths(contentWidth);
  }

  /**
   * Returns a column's visibility.
   * @param index The column index.
   * @return Whether the column is visible.
   */
  isVisible(index: number): boolean {
    return this.columns_[index]!.visible;
  }
}
