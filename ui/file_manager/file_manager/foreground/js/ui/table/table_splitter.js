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

import {isJellyEnabled} from '../../../../common/js/flags.js';
import {Splitter} from '../splitter.js';

import {Table} from './table.js';

/**
 * Creates a new table splitter element.
 */
// @ts-ignore: error TS2507: Type '(arg0?: Object | undefined) => Element' is
// not a constructor function type.
export class TableSplitter extends Splitter {
  /**
   * @param {Object=} opt_propertyBag Optional properties.
   */
  constructor(opt_propertyBag) {
    super();
    // cr.ui magic overwrites __proto__, so here we restore it back.
    this.__proto__ = TableSplitter.prototype;

    /** @private @type {Table} */
    this.table_;
    // @ts-ignore: error TS2339: Property 'table' does not exist on type
    // 'Object'.
    this.table = (opt_propertyBag && opt_propertyBag.table) || null;

    /** @private @type {number} */
    this.columnIndex_ = -1;

    /** @private @type {number} */
    this.columnWidth_ = 0;

    /** @private @type {number} */
    this.nextColumnWidth_ = 0;

    this.decorate();
  }

  /**
   * Initializes the element.
   */
  decorate() {
    super.decorate();

    const icon = document.createElement('cr-icon-button');
    if (isJellyEnabled()) {
      icon.setAttribute('iron-icon', 'files32:bar-dragger');
    } else {
      icon.setAttribute('iron-icon', 'files32:small-dragger');
    }
    icon.setAttribute('tabindex', '-1');
    icon.setAttribute('aria-hidden', 'true');
    icon.classList.add('splitter-icon');
    // @ts-ignore: error TS2339: Property 'appendChild' does not exist on type
    // 'TableSplitter'.
    this.appendChild(icon);

    // @ts-ignore: error TS2339: Property 'classList' does not exist on type
    // 'TableSplitter'.
    this.classList.add('table-header-splitter');
  }

  /**
   * Handles start of the splitter dragging.
   * Saves starting width of the column and changes the cursor.
   * @override
   */
  // @ts-ignore: error TS4121: This member cannot have a JSDoc comment with an
  // '@override' tag because its containing class 'TableSplitter' does not
  // extend another class.
  handleSplitterDragStart() {
    const cm = this.table_.columnModel;
    // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on type
    // 'TableSplitter'.
    this.ownerDocument.documentElement.classList.add('col-resize');

    this.columnWidth_ = cm.getWidth(this.columnIndex);
    this.nextColumnWidth_ = cm.getWidth(this.columnIndex + 1);

    // @ts-ignore: error TS2339: Property 'handleSplitterDragStart' does not
    // exist on type 'TableColumnModel'.
    this.table_.columnModel.handleSplitterDragStart();
  }

  /**
   * Handles spliter moves. Sets new width of the column.
   * @override
   */
  // @ts-ignore: error TS7006: Parameter 'deltaX' implicitly has an 'any' type.
  handleSplitterDragMove(deltaX) {
    // @ts-ignore: error TS2339: Property 'setWidthAndKeepTotal' does not exist
    // on type 'TableColumnModel'.
    if (this.table_.columnModel.setWidthAndKeepTotal) {
      // @ts-ignore: error TS2339: Property 'setWidthAndKeepTotal' does not
      // exist on type 'TableColumnModel'.
      this.table_.columnModel.setWidthAndKeepTotal(
          this.columnIndex, this.columnWidth_ + deltaX, true);
    }
  }

  /**
   * Handles end of the splitter dragging. Restores cursor.
   * @override
   */
  // @ts-ignore: error TS4121: This member cannot have a JSDoc comment with an
  // '@override' tag because its containing class 'TableSplitter' does not
  // extend another class.
  handleSplitterDragEnd() {
    // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist on type
    // 'TableSplitter'.
    this.ownerDocument.documentElement.classList.remove('col-resize');
    // @ts-ignore: error TS2345: Argument of type 'this' is not assignable to
    // parameter of type 'EventTarget'.
    dispatchSimpleEvent(this, 'column-resize-end', /*bubbles=*/ true);
    // @ts-ignore: error TS2339: Property 'handleSplitterDragEnd' does not exist
    // on type 'TableColumnModel'.
    this.table_.columnModel.handleSplitterDragEnd();
  }
}

/**
 * The column index.
 * @type {number}
 */
// @ts-ignore: error TS2565: Property 'columnIndex' is used before being
// assigned.
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
