// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This extends List for use in the table.
 */

import {getPropertyDescriptor} from 'chrome://resources/ash/common/cr_deprecated.js';

import {List} from '../list.js';
import {ListItem} from '../list_item.js';

import {Table} from './table.js';


/**
 * Creates a new table list element.
 */
export class TableList extends List {
  /**
   * @param {Object=} opt_propertyBag Optional properties.
   */
  constructor(opt_propertyBag) {
    // @ts-ignore: error TS2554: Expected 0 arguments, but got 1.
    super(opt_propertyBag);

    /** @private @type {?Table} */
    this.table_ = null;

    /**
     * Actually defined and managed by List.
     * @private @type {?HTMLElement}
     * */
    this.afterFiller_;
  }

  // @ts-ignore: error TS7006: Parameter 'el' implicitly has an 'any' type.
  static decorate(el) {
    // @ts-ignore: error TS2339: Property 'decorate' does not exist on type
    // 'typeof List'.
    if (List.decorate) {
      // @ts-ignore: error TS2339: Property 'decorate' does not exist on type
      // 'typeof List'.
      List.decorate(el);
    }

    el.__proto__ = TableList.prototype;
    el.className = 'list';
  }

  /**
   * Resizes columns. Called when column width changed.
   */
  resize() {
    if (this.needsFullRedraw_()) {
      this.redraw();
      return;
    }
    if (this.updateScrollbars_()) {
      // @ts-ignore: error TS2339: Property 'redraw' does not exist on type
      // 'List'.
      List.prototype.redraw.call(this);
    }  // Redraw items only.
    this.resizeCells_();
  }

  /**
   * Updates width of cells.
   */
  resizeCells_() {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    const cm = this.table_.columnModel;
    for (let row = this.firstElementChild; row; row = row.nextElementSibling) {
      if (row.tagName != 'LI') {
        continue;
      }

      for (let i = 0; i < cm.size; i++) {
        // @ts-ignore: error TS2339: Property 'style' does not exist on type
        // 'Element'.
        row.children[i].style.width = cm.getWidth(i) + 'px';
      }
      // @ts-ignore: error TS2339: Property 'style' does not exist on type
      // 'Element'.
      row.style.width = cm.totalWidth + 'px';
    }
    // @ts-ignore: error TS2339: Property 'afterFiller_' does not exist on type
    // 'TableList'.
    this.afterFiller_.style.width = cm.totalWidth + 'px';
  }

  /**
   * Redraws the viewport.
   */
  redraw() {
    // @ts-ignore: error TS2339: Property 'batchCount_' does not exist on type
    // 'TableList'.
    if (this.batchCount_ != 0) {
      return;
    }
    this.updateScrollbars_();

    // @ts-ignore: error TS2339: Property 'redraw' does not exist on type
    // 'List'.
    List.prototype.redraw.call(this);
    this.resizeCells_();
  }

  /**
   * Returns the height of after filler in the list.
   * @param {number} lastIndex The index of item past the last in viewport.
   * @return {number} The height of after filler.
   * @override
   */
  // @ts-ignore: error TS4122: This member cannot have a JSDoc comment with an
  // '@override' tag because it is not declared in the base class 'List'.
  getAfterFillerHeight(lastIndex) {
    // If the list is empty set height to 1 to show horizontal
    // scroll bar.
    return lastIndex == 0 ?
        1 :
        // @ts-ignore: error TS2339: Property 'getAfterFillerHeight' does not
        // exist on type 'List'.
        List.prototype.getAfterFillerHeight.call(this, lastIndex);
  }

  /**
   * Shows or hides vertical and horizontal scroll bars in the list.
   * @return {boolean} True if horizontal scroll bar changed.
   */
  updateScrollbars_() {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    const cm = this.table_.columnModel;
    const style = this.style;
    if (!cm || cm.size == 0) {
      if (style.overflow != 'hidden') {
        style.overflow = 'hidden';
        return true;
      } else {
        return false;
      }
    }

    let height = this.offsetHeight;
    let changed = false;
    const offsetWidth = this.offsetWidth;
    if (cm.totalWidth > offsetWidth) {
      if (style.overflowX != 'scroll') {
        style.overflowX = 'scroll';
      }
      // Once we sure there will be horizontal
      // scrollbar calculate with this height.
      height = this.clientHeight;
    }
    if (this.areAllItemsVisible_(height)) {
      if (cm.totalWidth <= offsetWidth && style.overflowX != 'hidden') {
        style.overflowX = 'hidden';
      }
      changed = this.showVerticalScrollBar_(false);
    } else {
      changed = this.showVerticalScrollBar_(true);
      const x = cm.totalWidth <= this.clientWidth ? 'hidden' : 'scroll';
      if (style.overflowX != x) {
        style.overflowX = x;
      }
    }
    return changed;
  }

  /**
   * Shows or hides vertical scroll bar.
   * @param {boolean} show True to show.
   * @return {boolean} True if visibility changed.
   */
  showVerticalScrollBar_(show) {
    const style = this.style;
    if (show && style.overflowY == 'scroll') {
      return false;
    }
    if (!show && style.overflowY == 'hidden') {
      return false;
    }
    style.overflowY = show ? 'scroll' : 'hidden';
    return true;
  }

  /**
   * @param {number} visibleHeight Height in pixels.
   * @return {boolean} True if all rows could be accomodiated in
   *                   visibleHeight pixels.
   */
  areAllItemsVisible_(visibleHeight) {
    if (!this.dataModel || this.dataModel.length == 0) {
      return true;
    }
    // @ts-ignore: error TS2339: Property 'getItemTop' does not exist on type
    // 'TableList'.
    return this.getItemTop(this.dataModel.length) <= visibleHeight;
  }

  /**
   * Creates a new list item.
   * @param {*} dataItem The value to use for the item.
   * @return {!ListItem} The newly created list item.
   */
  createItem(dataItem) {
    return /** @type {!ListItem} */ (
        // @ts-ignore: error TS2345: Argument of type 'Table | null' is not
        // assignable to parameter of type 'Table'.
        this.table_.getRenderFunction().call(null, dataItem, this.table_));
  }

  /**
   * Determines whether a full redraw is required.
   * @return {boolean}
   */
  needsFullRedraw_() {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    const cm = this.table_.columnModel;
    const row = this.firstElementChild;
    // If the number of columns in the model has changed, a full redraw is
    // needed.
    // @ts-ignore: error TS18047: 'row' is possibly 'null'.
    if (row.children.length != cm.size) {
      return true;
    }
    // If the column visibility has changed, a full redraw is required.
    for (let i = 0; i < cm.size; ++i) {
      // @ts-ignore: error TS2339: Property 'hidden' does not exist on type
      // 'Element'.
      if (cm.isVisible(i) == row.children[i].hidden) {
        return true;
      }
    }
    return false;
  }
}

  /**
   * The table associated with the list.
   * @type {Element}
   */
// @ts-ignore: error TS2565: Property 'table' is used before being assigned.
TableList.prototype.table;
Object.defineProperty(
    TableList.prototype, 'table', getPropertyDescriptor('table'));
