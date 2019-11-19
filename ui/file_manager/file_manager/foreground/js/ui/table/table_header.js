// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This implements a table header.
 */

cr.define('cr.ui.table', function() {
  /**
   * Creates a new table header.
   * @extends {HTMLDivElement}
   */
  class TableHeader {
    constructor() {
      /** @private {cr.ui.Table} */
      this.table_ = null;

      /** @private {number} */
      this.batchCount_ = 0;

      /** @private {Element} */
      this.headerInner_;
    }

    /**
     * Initializes the element.
     * @param {Element} el
     */
    static decorate(el) {
      el.__proto__ = TableHeader.prototype;
      el = /** @type {cr.ui.table.TableHeader} */ (el);

      el.className = 'table-header';
      el.batchCount_ = 0;

      el.headerInner_ = el.ownerDocument.createElement('div');
      el.headerInner_.className = 'table-header-inner';
      el.appendChild(el.headerInner_);
      el.addEventListener('touchstart', el.handleTouchStart_.bind(el), false);
    }

    /**
     * Updates table header width. Header width depends on list having a
     * vertical scrollbar.
     */
    updateWidth() {
      // Header should not span over the vertical scrollbar of the list.
      const list = this.table_.querySelector('list');
      this.headerInner_.style.width = list.clientWidth + 'px';
    }

    /**
     * Resizes columns.
     */
    resize() {
      const headerCells = this.querySelectorAll('.table-header-cell');
      if (this.needsFullRedraw_(headerCells)) {
        this.redraw();
        return;
      }

      const cm = this.table_.columnModel;
      for (let i = 0; i < cm.size; i++) {
        headerCells[i].style.width = cm.getWidth(i) + 'px';
      }
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
        if (dm.isSortable(cm.getId(i))) {
          cell.addEventListener(
              'click', this.createSortFunction_(i).bind(this));
        }

        cell.appendChild(this.createHeaderLabel_(i));
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
        splitter.addEventListener(
            'dblclick', this.handleDblClick_.bind(this, i));
        // Don't display splitters for hidden columns.  Don't omit the splitter
        // completely, as it's much simpler if the number of splitter elements
        // and columns are in sync.
        splitter.hidden = !cm.isVisible(i);

        this.headerInner_.appendChild(splitter);
        splitters.push(splitter);
      }
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
        if (splitters[i].hidden) {
          continue;
        }
        place += cm.getWidth(i);
        splitters[i].style.marginInlineStart = place + 'px';
      }
    }

    /**
     * Renders column header. Appends text label and sort arrow if needed.
     * @param {number} index Column index.
     */
    createHeaderLabel_(index) {
      const cm = this.table_.columnModel;
      const dm = this.table_.dataModel;

      const labelDiv = this.ownerDocument.createElement('div');
      labelDiv.className = 'table-header-label';

      if (cm.isEndAlign(index)) {
        labelDiv.style.textAlign = 'end';
      }
      const span = this.ownerDocument.createElement('span');
      span.appendChild(cm.renderHeader(index, this.table_));
      span.style.padding = '0';

      if (dm) {
        if (dm.sortStatus.field == cm.getId(index)) {
          if (dm.sortStatus.direction == 'desc') {
            span.className = 'table-header-sort-image-desc';
          } else {
            span.className = 'table-header-sort-image-asc';
          }
        }
      }
      labelDiv.appendChild(span);
      return labelDiv;
    }

    /**
     * Creates sort function for given column.
     * @param {number} index The index of the column to sort by.
     */
    createSortFunction_(index) {
      return function() {
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
      if (e.touches.length != 1) {
        return;
      }
      const clientX = e.touches[0].clientX;

      let minDistance = TableHeader.TOUCH_DRAG_AREA_WIDTH;
      let candidate;

      const splitters = /** @type {NodeList<TableSplitter>} */ (
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
    handleDblClick_(index, e) {
      this.table_.fitColumn(index);
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
  cr.defineProperty(TableHeader, 'table');

  /**
   * Rectangular area around the splitters sensitive to touch events
   * (in pixels).
   */
  TableHeader.TOUCH_DRAG_AREA_WIDTH = 30;

  return {TableHeader: TableHeader};
});
