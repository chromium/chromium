// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This implements a table header.
 */

import {dispatchSimpleEvent, getPropertyDescriptor} from 'chrome://resources/ash/common/cr_deprecated.js';

import {Table} from './table.js';
import {TableSplitter} from './table_splitter.js';

/**
 * Creates a new table header.
 * @extends {HTMLDivElement}
 */
export class TableHeader {
  constructor() {
    /** @private @type {Table} */
    // @ts-ignore: error TS7008: Member 'table_' implicitly has an 'any' type.
    this.table_ = null;

    /** @private @type {number} */
    this.batchCount_ = 0;

    /** @private @type {Element} */
    // @ts-ignore: error TS2339: Property 'headerInner_' does not exist on type
    // 'TableHeader'.
    this.headerInner_;
  }

  /**
   * Initializes the element.
   * @param {Element} el
   */
  static decorate(el) {
    // @ts-ignore: error TS2339: Property '__proto__' does not exist on type
    // 'Element'.
    el.__proto__ = TableHeader.prototype;
    // @ts-ignore: error TS2352: Conversion of type 'Element' to type
    // 'TableHeader' may be a mistake because neither type sufficiently overlaps
    // with the other. If this was intentional, convert the expression to
    // 'unknown' first.
    el = /** @type {TableHeader} */ (el);

    el.className = 'table-header';
    // @ts-ignore: error TS2339: Property 'batchCount_' does not exist on type
    // 'Element'.
    el.batchCount_ = 0;

    // @ts-ignore: error TS2339: Property 'headerInner_' does not exist on type
    // 'Element'.
    el.headerInner_ = el.ownerDocument.createElement('div');
    // @ts-ignore: error TS2339: Property 'headerInner_' does not exist on type
    // 'Element'.
    el.headerInner_.className = 'table-header-inner';
    // @ts-ignore: error TS2339: Property 'headerInner_' does not exist on type
    // 'Element'.
    el.appendChild(el.headerInner_);
    // @ts-ignore: error TS2339: Property 'handleTouchStart_' does not exist on
    // type 'Element'.
    el.addEventListener('touchstart', el.handleTouchStart_.bind(el), false);
  }

  /**
   * Updates table header width. Header width depends on list having a
   * vertical scrollbar.
   */
  updateWidth() {
    // Header should not span over the vertical scrollbar of the list.
    // @ts-ignore: error TS2339: Property 'querySelector' does not exist on type
    // 'Table'.
    const list = this.table_.querySelector('list');
    // @ts-ignore: error TS2339: Property 'headerInner_' does not exist on type
    // 'TableHeader'.
    this.headerInner_.style.width = list.clientWidth + 'px';
  }

  /**
   * Resizes columns.
   */
  resize() {
    // @ts-ignore: error TS2339: Property 'querySelectorAll' does not exist on
    // type 'TableHeader'.
    const headerCells = this.querySelectorAll('.table-header-cell');
    if (this.needsFullRedraw_(headerCells)) {
      this.redraw();
      return;
    }

    const cm = this.table_.columnModel;
    for (let i = 0; i < cm.size; i++) {
      headerCells[i].style.width = cm.getWidth(i) + 'px';
    }
    // @ts-ignore: error TS2339: Property 'querySelectorAll' does not exist on
    // type 'TableHeader'.
    this.placeSplitters_(this.querySelectorAll('.table-header-splitter'));
  }

  startBatchUpdates() {
    this.batchCount_++;
  }

  endBatchUpdates() {
    this.batchCount_--;
    if (this.batchCount_ == 0) {
      this.redraw();
    }
  }

  /**
   * Redraws table header.
   */
  redraw() {
    if (this.batchCount_ != 0) {
      return;
    }

    const cm = this.table_.columnModel;
    const dm = this.table_.dataModel;

    this.updateWidth();
    // @ts-ignore: error TS2339: Property 'headerInner_' does not exist on type
    // 'TableHeader'.
    this.headerInner_.textContent = '';

    if (!cm || !dm) {
      return;
    }

    for (let i = 0; i < cm.size; i++) {
      // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on
      // type 'TableHeader'.
      const cell = this.ownerDocument.createElement('div');
      cell.style.width = cm.getWidth(i) + 'px';
      // Don't display cells for hidden columns. Don't omit the cell
      // completely, as it's much simpler if the number of cell elements and
      // columns are in sync.
      cell.hidden = !cm.isVisible(i);
      cell.className = 'table-header-cell';
      if (dm.isSortable(cm.getId(i))) {
        cell.addEventListener('click', this.createSortFunction_(i).bind(this));
      }

      cell.appendChild(this.createHeaderLabel_(i));
      // @ts-ignore: error TS2339: Property 'headerInner_' does not exist on
      // type 'TableHeader'.
      this.headerInner_.appendChild(cell);
    }
    this.appendSplitters_();
  }

  /**
   * Appends column splitters to the table header.
   */
  appendSplitters_() {
    const cm = this.table_.columnModel;
    const splitters = [];
    for (let i = 0; i < cm.size; i++) {
      // splitter should use CSS for background image.
      const splitter = new TableSplitter({table: this.table_});
      splitter.columnIndex = i;
      // @ts-ignore: error TS2339: Property 'addEventListener' does not exist on
      // type 'TableSplitter'.
      splitter.addEventListener('dblclick', this.handleDblClick_.bind(this, i));
      // Don't display splitters for hidden columns.  Don't omit the splitter
      // completely, as it's much simpler if the number of splitter elements
      // and columns are in sync.
      // @ts-ignore: error TS2339: Property 'hidden' does not exist on type
      // 'TableSplitter'.
      splitter.hidden = !cm.isVisible(i);

      // @ts-ignore: error TS2339: Property 'headerInner_' does not exist on
      // type 'TableHeader'.
      this.headerInner_.appendChild(splitter);
      splitters.push(splitter);
    }
    // @ts-ignore: error TS2345: Argument of type 'TableSplitter[]' is not
    // assignable to parameter of type 'NodeList | HTMLElement[]'.
    this.placeSplitters_(splitters);
  }

  /**
   * Place splitters to right positions.
   * @param {Array<HTMLElement>|NodeList} splitters Array of splitters.
   */
  placeSplitters_(splitters) {
    const cm = this.table_.columnModel;
    let place = 0;
    for (let i = 0; i < cm.size; i++) {
      // Don't account for the widths of hidden columns.
      // @ts-ignore: error TS2339: Property 'hidden' does not exist on type
      // 'Node | HTMLElement'.
      if (splitters[i].hidden) {
        continue;
      }
      place += cm.getWidth(i);
      // @ts-ignore: error TS2339: Property 'style' does not exist on type 'Node
      // | HTMLElement'.
      splitters[i].style.marginInlineStart = place + 'px';
    }
  }

  /**
   * Renders column header. Appends text label and sort arrow if needed.
   * @param {number} index Column index.
   */
  createHeaderLabel_(index) {
    const cm = this.table_.columnModel;
    // @ts-ignore: error TS6133: 'dm' is declared but its value is never read.
    const dm = this.table_.dataModel;

    // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on type
    // 'TableHeader'.
    const labelDiv = this.ownerDocument.createElement('div');
    labelDiv.className = 'table-header-label';
    labelDiv.classList.add(cm.getId(index));

    if (cm.isEndAlign(index)) {
      labelDiv.style.textAlign = 'end';
    }
    // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on type
    // 'TableHeader'.
    const span = this.ownerDocument.createElement('span');
    // @ts-ignore: error TS2345: Argument of type 'Table' is not assignable to
    // parameter of type 'Element'.
    span.appendChild(cm.renderHeader(index, this.table_));
    span.style.padding = '0';

    labelDiv.appendChild(span);
    return labelDiv;
  }

  /**
   * Creates sort function for given column.
   * @param {number} index The index of the column to sort by.
   */
  createSortFunction_(index) {
    return function() {
      // @ts-ignore: error TS2683: 'this' implicitly has type 'any' because it
      // does not have a type annotation.
      this.table_.sort(index);
    }.bind(this);
  }

  /**
   * Handles the touchstart event. If the touch happened close enough
   * to a splitter starts dragging.
   * @param {Event} e The touch event.
   */
  handleTouchStart_(e) {
    e = /** @type {TouchEvent} */ (e);
    // @ts-ignore: error TS2339: Property 'touches' does not exist on type
    // 'Event'.
    if (e.touches.length != 1) {
      return;
    }
    // @ts-ignore: error TS2339: Property 'touches' does not exist on type
    // 'Event'.
    const clientX = e.touches[0].clientX;

    let minDistance = TableHeader.TOUCH_DRAG_AREA_WIDTH;
    let candidate;

    // @ts-ignore: error TS2315: Type 'NodeList' is not generic.
    const splitters = /** @type {NodeList<TableSplitter>} */ (
        // @ts-ignore: error TS2339: Property 'querySelectorAll' does not exist
        // on type 'TableHeader'.
        this.querySelectorAll('.table-header-splitter'));
    for (let i = 0; i < splitters.length; i++) {
      const r = splitters[i].getBoundingClientRect();
      if (clientX <= r.left && r.left - clientX <= minDistance) {
        minDistance = r.left - clientX;
        candidate = splitters[i];
      }
      if (clientX >= r.right && clientX - r.right <= minDistance) {
        minDistance = clientX - r.right;
        candidate = splitters[i];
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
   * @param {number} index Column index.
   * @param {Event} e The double click event.
   */
  // @ts-ignore: error TS6133: 'e' is declared but its value is never read.
  handleDblClick_(index, e) {
    this.table_.fitColumn(index);
    // @ts-ignore: error TS2345: Argument of type 'this' is not assignable to
    // parameter of type 'EventTarget'.
    dispatchSimpleEvent(this, 'column-resize-end', /*bubbles=*/ true);
  }

  /**
   * Determines whether a full redraw is required.
   * @param {!NodeList} headerCells
   * @return {boolean}
   */
  needsFullRedraw_(headerCells) {
    const cm = this.table_.columnModel;
    // If the number of columns in the model has changed, a full redraw is
    // needed.
    if (headerCells.length != cm.size) {
      return true;
    }
    // If the column visibility has changed, a full redraw is required.
    for (let i = 0; i < cm.size; i++) {
      // @ts-ignore: error TS2339: Property 'hidden' does not exist on type
      // 'Node'.
      if (cm.isVisible(i) == headerCells[i].hidden) {
        return true;
      }
    }
    return false;
  }
}

TableHeader.prototype.__proto__ = HTMLDivElement.prototype;

/**
 * The table associated with the header.
 * @type {Element}
 */
// @ts-ignore: error TS2565: Property 'table' is used before being assigned.
TableHeader.prototype.table;
Object.defineProperty(
    TableHeader.prototype, 'table', getPropertyDescriptor('table'));

/**
 * Rectangular area around the splitters sensitive to touch events
 * (in pixels).
 */
TableHeader.TOUCH_DRAG_AREA_WIDTH = 30;
