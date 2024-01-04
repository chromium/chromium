// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This implements a table header.
 */

import {dispatchSimpleEvent} from 'chrome://resources/ash/common/cr_deprecated.js';
import {assert} from 'chrome://resources/js/assert.js';

import {jsSetter} from '../../../../common/js/cr_ui.js';

import {type Table} from './table.js';
import {createTableSplitter, type TableSplitter} from './table_splitter.js';

/**
 * Rectangular area around the splitters sensitive to touch events
 * (in pixels).
 */
const TOUCH_DRAG_AREA_WIDTH = 30;

/**
 * Creates a new table header.
 */
export class TableHeader extends HTMLDivElement {
  private table_: Table|null = null;
  private batchCount_ = 0;
  private headerInner_: HTMLElement|null = null;

  /**
   * Initializes the element.
   */
  initialize() {
    this.className = 'table-header';
    this.batchCount_ = 0;

    this.headerInner_ = this.ownerDocument.createElement('div');
    this.headerInner_.className = 'table-header-inner';
    this.appendChild(this.headerInner_);
    this.addEventListener(
        'touchstart', this.handleTouchStart_.bind(this), false);
  }

  /**
   * Updates table header width. Header width depends on list having a
   * vertical scrollbar.
   */
  updateWidth() {
    assert(this.headerInner_);
    // Header should not span over the vertical scrollbar of the list.
    const list = (this.table as unknown as HTMLElement)
                     .querySelector<HTMLElement>('list')!;
    this.headerInner_.style.width = list.clientWidth + 'px';
  }

  /**
   * Resizes columns.
   */
  resize() {
    const headerCells =
        Array.from(this.querySelectorAll<TableHeader>('.table-header-cell'));
    if (this.needsFullRedraw_(headerCells)) {
      this.redraw();
      return;
    }

    const cm = this.table.columnModel;
    for (let i = 0; i < cm.size; i++) {
      headerCells[i]!.style.width = cm.getWidth(i) + 'px';
    }
    this.placeSplitters_(
        Array.from(this.querySelectorAll('.table-header-splitter')));
  }

  startBatchUpdates() {
    this.batchCount_++;
  }

  endBatchUpdates() {
    this.batchCount_--;
    if (this.batchCount_ === 0) {
      this.redraw();
    }
  }

  /**
   * Redraws table header.
   */
  redraw() {
    if (this.batchCount_ !== 0) {
      return;
    }

    assert(this.table_);
    assert(this.headerInner_);
    const cm = this.table_.columnModel;
    const dm = this.table_.dataModel;

    this.updateWidth();
    this.headerInner_.textContent = '';

    if (!cm || !dm) {
      return;
    }

    for (let i = 0; i < cm.size; i++) {
      const cell = this.ownerDocument.createElement('div');
      cell.style.width = cm.getWidth(i) + 'px';
      // Don't display cells for hidden columns. Don't omit the cell
      // completely, as it's much simpler if the number of cell elements and
      // columns are in sync.
      cell.hidden = !cm.isVisible(i);
      cell.className = 'table-header-cell';
      const tableHeader = this;
      if (dm.isSortable(cm.getId(i)!)) {
        cell.addEventListener(
            'click', this.createSortFunction_(i).bind(tableHeader));
      }

      cell.appendChild(this.createHeaderLabel_(i));
      this.headerInner_.appendChild(cell);
    }
    this.appendSplitters_();
  }

  /**
   * Appends column splitters to the table header.
   */
  private appendSplitters_() {
    assert(this.table_);
    const cm = this.table_.columnModel;
    const splitters = [];
    for (let i = 0; i < cm.size; i++) {
      // splitter should use CSS for background image.
      const splitter = createTableSplitter(this.table_);
      splitter.columnIndex = i;
      splitter.addEventListener('dblclick', this.handleDblClick_.bind(this, i));
      // Don't display splitters for hidden columns.  Don't omit the splitter
      // completely, as it's much simpler if the number of splitter elements
      // and columns are in sync.
      (splitter as unknown as HTMLElement).hidden = !cm.isVisible(i);

      this.headerInner_!.appendChild(splitter as unknown as HTMLElement);
      splitters.push(splitter);
    }
    this.placeSplitters_(splitters as unknown[] as HTMLElement[]);
  }

  /**
   * Place splitters to right positions.
   * @param splitters Array of splitters.
   */
  private placeSplitters_(splitters: HTMLElement[]) {
    const cm = this.table.columnModel;
    let place = 0;
    for (let i = 0; i < cm.size; i++) {
      // Don't account for the widths of hidden columns.
      if (splitters[i]?.hidden) {
        continue;
      }
      place += cm.getWidth(i);
      splitters[i]!.style.marginInlineStart = place + 'px';
    }
  }

  /**
   * Renders column header. Appends text label and sort arrow if needed.
   * @param index Column index.
   */
  private createHeaderLabel_(index: number) {
    const cm = this.table.columnModel;

    const labelDiv = this.ownerDocument.createElement('div');
    labelDiv.className = 'table-header-label';
    labelDiv.classList.add(cm.getId(index)!);

    if (cm.isEndAlign(index)) {
      labelDiv.style.textAlign = 'end';
    }
    const span = this.ownerDocument.createElement('span');
    span.appendChild(cm.renderHeader(index, this.table));
    span.style.padding = '0';

    labelDiv.appendChild(span);
    return labelDiv;
  }

  /**
   * Creates sort function for given column.
   * @param index The index of the column to sort by.
   */
  private createSortFunction_(index: number) {
    return () => {
      this.table.sort(index);
    };
  }

  /**
   * Handles the touchstart event. If the touch happened close enough
   * to a splitter starts dragging.
   */
  private handleTouchStart_(e: TouchEvent) {
    if (e.touches.length !== 1) {
      return;
    }
    const clientX = e.touches[0]!.clientX;

    let minDistance = TOUCH_DRAG_AREA_WIDTH;
    let candidate;

    const splitters = Array.from(
        this.querySelectorAll<TableSplitter>('.table-header-splitter'));
    for (const splitter of splitters) {
      const r = splitter.getBoundingClientRect();
      if (clientX <= r.left && r.left - clientX <= minDistance) {
        minDistance = r.left - clientX;
        candidate = splitter;
      }
      if (clientX >= r.right && clientX - r.right <= minDistance) {
        minDistance = clientX - r.right;
        candidate = splitter;
      }
    }
    if (candidate) {
      candidate.startDrag(clientX, true);
    }
    // Splitter itself shouldn't handle this event.
    e.stopPropagation();
  }

  /**
   * Handles the double click on a column separator event.
   * Adjusts column width.
   * @param index Column index.
   * @param _e The double click event.
   */
  private handleDblClick_(index: number, _e: Event) {
    this.table.fitColumn(index);
    dispatchSimpleEvent(this, 'column-resize-end', /*bubbles=*/ true);
  }

  /**
   * Determines whether a full redraw is required.
   */
  private needsFullRedraw_(headerCells: TableHeader[]): boolean {
    const cm = this.table.columnModel;
    // If the number of columns in the model has changed, a full redraw is
    // needed.
    if (headerCells.length !== cm.size) {
      return true;
    }
    // If the column visibility has changed, a full redraw is required.
    for (let i = 0; i < cm.size; i++) {
      if (cm.isVisible(i) === headerCells[i]!.hidden) {
        return true;
      }
    }
    return false;
  }

  get table(): Table {
    return this.table_!;
  }

  set table(value: Table) {
    jsSetter(this, 'table', value);
  }
}
