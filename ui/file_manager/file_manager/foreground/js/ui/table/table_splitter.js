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

import {dispatchSimpleEvent, getPropertyDescriptor} from 'chrome://resources/ash/common/cr_deprecated.js';

import {Splitter} from '../splitter.js';

import {Table} from './table.js';

/**
 * Creates a new table splitter element.
 */
export class TableSplitter extends Splitter {
  /**
   * @param {Object=} opt_propertyBag Optional properties.
   */
  constructor(opt_propertyBag) {
    super();
    // cr.ui magic overwrites __proto__, so here we restore it back.
    this.__proto__ = TableSplitter.prototype;

    /** @private {Table} */
    this.table_;
    this.table = (opt_propertyBag && opt_propertyBag.table) || null;

    /** @private {number} */
    this.columnIndex_ = -1;

    /** @private {number} */
    this.columnWidth_ = 0;

    /** @private {number} */
    this.nextColumnWidth_ = 0;

    this.decorate();
  }

  /**
   * Initializes the element.
   */
  decorate() {
    super.decorate();

    const icon = document.createElement('cr-icon-button');
    icon.setAttribute('iron-icon', 'files32:small-dragger');
    icon.setAttribute('tabindex', '-1');
    icon.setAttribute('aria-hidden', 'true');
    icon.classList.add('splitter-icon');
    this.appendChild(icon);

    this.classList.add('table-header-splitter');
  }

  /**
   * Handles start of the splitter dragging.
   * Saves starting width of the column and changes the cursor.
   * @override
   */
  handleSplitterDragStart() {
    const cm = this.table_.columnModel;
    this.ownerDocument.documentElement.classList.add('col-resize');

    this.columnWidth_ = cm.getWidth(this.columnIndex);
    this.nextColumnWidth_ = cm.getWidth(this.columnIndex + 1);

    this.table_.columnModel.handleSplitterDragStart();
  }

  /**
   * Handles spliter moves. Sets new width of the column.
   * @override
   */
  handleSplitterDragMove(deltaX) {
    if (this.table_.columnModel.setWidthAndKeepTotal) {
      this.table_.columnModel.setWidthAndKeepTotal(
          this.columnIndex, this.columnWidth_ + deltaX, true);
    }
  }

  /**
   * Handles end of the splitter dragging. Restores cursor.
   * @override
   */
  handleSplitterDragEnd() {
    this.ownerDocument.documentElement.classList.remove('col-resize');
    dispatchSimpleEvent(this, 'column-resize-end', true);
    this.table_.columnModel.handleSplitterDragEnd();
  }
}

/**
 * The column index.
 * @type {number}
 */
TableSplitter.prototype.columnIndex;
Object.defineProperty(
    TableSplitter.prototype, 'columnIndex',
    getPropertyDescriptor('columnIndex'));

/**
 * The table associated with the splitter.
 * @type {Element}
 */
TableSplitter.prototype.table;
Object.defineProperty(
    TableSplitter.prototype, 'table', getPropertyDescriptor('table'));
