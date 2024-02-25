// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This implements a splitter element which can be used to resize
 * table columns.
 *
 * Each splitter is associated with certain column and resizes it when dragged.
 * It is column model responsibility to resize other columns accordingly.
 */

import {dispatchSimpleEvent} from 'chrome://resources/ash/common/cr_deprecated.js';

import {jsSetter} from '../../../../common/js/cr_ui.js';
import type {FileTableColumnModel} from '../file_table.js';
import {Splitter} from '../splitter.js';

import type {Table} from './table.js';

export function createTableSplitter(table: Table): TableSplitter {
  const el = document.createElement('div') as TableSplitter;
  Object.setPrototypeOf(el, TableSplitter.prototype);
  el.initialize(table);
  return el as TableSplitter;
}
/**
 * Creates a new table splitter element.
 */
export class TableSplitter extends Splitter {
  private table_: Table|null = null;
  private columnIndex_: number = -1;
  private columnWidth_: number = 0;

  /**
   * Initializes the element.
   */
  override initialize(table: Table) {
    super.initialize();
    this.table_ = table;

    const icon = document.createElement('cr-icon-button');
    icon.setAttribute('iron-icon', 'files32:bar-dragger');

    icon.setAttribute('tabindex', '-1');
    icon.setAttribute('aria-hidden', 'true');
    icon.classList.add('splitter-icon');
    this.appendChild(icon);

    this.classList.add('table-header-splitter');
  }

  /**
   * Handles start of the splitter dragging.
   * Saves starting width of the column and changes the cursor.
   */
  override handleSplitterDragStart() {
    const cm = this.table.columnModel as FileTableColumnModel;
    this.ownerDocument.documentElement.classList.add('col-resize');

    this.columnWidth_ = cm.getWidth(this.columnIndex!);

    cm.handleSplitterDragStart();
  }

  /**
   * Handles spliter moves. Sets new width of the column.
   */
  override handleSplitterDragMove(deltaX: number) {
    const cm = this.table.columnModel as FileTableColumnModel;
    if ('setWidthAndKeepTotal' in cm) {
      cm.setWidthAndKeepTotal(this.columnIndex, this.columnWidth_ + deltaX);
    }
  }

  /**
   * Handles end of the splitter dragging. Restores cursor.
   */
  override handleSplitterDragEnd() {
    const cm = this.table.columnModel as FileTableColumnModel;
    this.ownerDocument.documentElement.classList.remove('col-resize');
    dispatchSimpleEvent(this, 'column-resize-end', /*bubbles=*/ true);
    cm.handleSplitterDragEnd();
  }

  get columnIndex(): number {
    return this.columnIndex_;
  }

  set columnIndex(value: number) {
    jsSetter(this, 'columnIndex', value);
  }

  get table(): Table {
    return this.table_!;
  }

  set table(value: Table) {
    jsSetter(this, 'table', value);
  }
}
