// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {define as crUiDefine} from '../../../common/js/ui.js';
import {decorate} from '../../../common/js/cr_ui.js';

import {ListSelectionModel} from './list_selection_model.js';
import {ListSelectionController} from './list_selection_controller.js';
import {List} from './list.js';
import {ListItem} from './list_item.js';

/**
 * @fileoverview This implements a grid control. Grid contains a bunch of
 * similar elements placed in multiple columns. It's pretty similar to the list,
 * except the multiple columns layout.
 */

/**
 * @param {*} dataItem The data item.
 * @returns {!GridItem}
 */
export function createGridItem(dataItem) {
  const el = document.createElement('li');
  // @ts-ignore: error TS2339: Property 'dataItem' does not exist on type
  // 'HTMLLIElement'.
  el.dataItem = dataItem;
  // @ts-ignore: error TS2740: Type 'HTMLLIElement' is missing the following
  // properties from type 'GridItem': dataItem, decorate, listIndex_, label, and
  // 3 more.
  return decorate(el, ListItem);
}

/** Creates a new grid item element. */
export class GridItem extends ListItem {
  /** Unused, see the decorate() method instead. */
  constructor() {
    super();

    /** @type {*} */
    this.dataItem;
  }

  /**
   * Called when an element is decorated as a grid item.
   */
  // @ts-ignore: error TS4119: This member must have a JSDoc comment with an
  // '@override' tag because it overrides a member in the base class 'ListItem'.
  decorate() {
    // @ts-ignore: error TS2345: Argument of type 'IArguments' is not assignable
    // to parameter of type '[]'.
    ListItem.prototype.decorate.apply(this, arguments);
    this.textContent = this.dataItem;
  }
}

/**
 * Creates a new grid element.
 * @param {Object=} opt_propertyBag Optional properties.
 * @constructor
 * @extends {List}
 */
// @ts-ignore: error TS8022: JSDoc '@extends' is not attached to a class.
export const Grid = crUiDefine('grid');

Grid.prototype = {
  __proto__: List.prototype,

  /**
   * The number of columns in the grid. Either set by the user, or lazy
   * calculated as the maximum number of items fitting in the grid width.
   * @type {number}
   * @private
   */
  columns_: 0,

  /**
   * Function used to create grid items.
   * @type {function(*): GridItem}
   * @override
   */
  itemConstructor_: createGridItem,

  /**
   * Whether or not the rows on list have various heights.
   * Shows a warning at the setter because Grid does not support this.
   * @type {boolean}
   */
  get fixedHeight() {
    return true;
  },
  set fixedHeight(fixedHeight) {
    if (!fixedHeight) {
      console.warn('Grid does not support fixedHeight = false');
    }
  },

  /**
   * @return {number} The number of columns determined by width of the grid
   *     and width of the items.
   * @private
   */
  getColumnCount_() {
    // Size comes here with margin already collapsed.
    // @ts-ignore: error TS2339: Property 'getDefaultItemSize_' does not exist
    // on type '{ __proto__: List; columns_: number; itemConstructor_: (arg0:
    // any) => GridItem; fixedHeight: boolean; getColumnCount_(): number;
    // updateMetrics_(): void; columns: number; ... 8 more ...; redraw(): void;
    // }'.
    const size = this.getDefaultItemSize_();

    if (!size) {
      return 0;
    }

    // We should uncollapse margin, since margin isn't collapsed for
    // inline-block elements according to css spec which are thumbnail items.

    const width = size.width + Math.min(size.marginLeft, size.marginRight);
    const height = size.height + Math.min(size.marginTop, size.marginBottom);

    if (!width || !height) {
      return 0;
    }

    // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type '{
    // __proto__: List; columns_: number; itemConstructor_: (arg0: any) =>
    // GridItem; fixedHeight: boolean; getColumnCount_(): number;
    // updateMetrics_(): void; columns: number; ... 8 more ...; redraw(): void;
    // }'.
    const itemCount = this.dataModel ? this.dataModel.length : 0;
    if (!itemCount) {
      return 0;
    }

    const columns = Math.floor(
        // @ts-ignore: error TS2339: Property 'horizontalPadding_' does not
        // exist on type '{ __proto__: List; columns_: number; itemConstructor_:
        // (arg0: any) => GridItem; fixedHeight: boolean; getColumnCount_():
        // number; updateMetrics_(): void; columns: number; ... 8 more ...;
        // redraw(): void; }'.
        (this.clientWidthWithoutScrollbar_ - this.horizontalPadding_) / width);
    if (!columns) {
      return 0;
    }

    const rows = Math.ceil(itemCount / columns);
    // @ts-ignore: error TS2339: Property 'clientHeight_' does not exist on type
    // '{ __proto__: List; columns_: number; itemConstructor_: (arg0: any) =>
    // GridItem; fixedHeight: boolean; getColumnCount_(): number;
    // updateMetrics_(): void; columns: number; ... 8 more ...; redraw(): void;
    // }'.
    if (rows * height <= this.clientHeight_) {
      // Content fits within the client area (no scrollbar required).
      return columns;
    }

    // If the content doesn't fit within the client area, the number of
    // columns should be calculated with consideration for scrollbar's width.
    return Math.floor(
        // @ts-ignore: error TS2339: Property 'horizontalPadding_' does not
        // exist on type '{ __proto__: List; columns_: number; itemConstructor_:
        // (arg0: any) => GridItem; fixedHeight: boolean; getColumnCount_():
        // number; updateMetrics_(): void; columns: number; ... 8 more ...;
        // redraw(): void; }'.
        (this.clientWidthWithScrollbar_ - this.horizontalPadding_) / width);
  },

  /**
   * Measure and cache client width and height with and without scrollbar.
   * Must be updated when offsetWidth and/or offsetHeight changed.
   */
  updateMetrics_() {
    // Check changings that may affect number of columns.
    // @ts-ignore: error TS2339: Property 'offsetWidth' does not exist on type
    // '{ __proto__: List; columns_: number; itemConstructor_: (arg0: any) =>
    // GridItem; fixedHeight: boolean; getColumnCount_(): number;
    // updateMetrics_(): void; columns: number; ... 8 more ...; redraw(): void;
    // }'.
    const offsetWidth = this.offsetWidth;
    // @ts-ignore: error TS2339: Property 'offsetHeight' does not exist on type
    // '{ __proto__: List; columns_: number; itemConstructor_: (arg0: any) =>
    // GridItem; fixedHeight: boolean; getColumnCount_(): number;
    // updateMetrics_(): void; columns: number; ... 8 more ...; redraw(): void;
    // }'.
    const offsetHeight = this.offsetHeight;
    // @ts-ignore: error TS2345: Argument of type '{ __proto__: List; columns_:
    // number; itemConstructor_: (arg0: any) => GridItem; fixedHeight: boolean;
    // getColumnCount_(): number; updateMetrics_(): void; columns: number; ... 8
    // more ...; redraw(): void; }' is not assignable to parameter of type
    // 'Element'.
    const style = window.getComputedStyle(this);
    const overflowY = style.overflowY;
    const horizontalPadding =
        parseFloat(style.paddingLeft) + parseFloat(style.paddingRight);

    // @ts-ignore: error TS2339: Property 'lastOffsetWidth_' does not exist on
    // type '{ __proto__: List; columns_: number; itemConstructor_: (arg0: any)
    // => GridItem; fixedHeight: boolean; getColumnCount_(): number;
    // updateMetrics_(): void; columns: number; ... 8 more ...; redraw(): void;
    // }'.
    if (this.lastOffsetWidth_ === offsetWidth &&
        // @ts-ignore: error TS2339: Property 'lastOverflowY' does not exist on
        // type '{ __proto__: List; columns_: number; itemConstructor_: (arg0:
        // any) => GridItem; fixedHeight: boolean; getColumnCount_(): number;
        // updateMetrics_(): void; columns: number; ... 8 more ...; redraw():
        // void; }'.
        this.lastOverflowY === overflowY &&
        // @ts-ignore: error TS2339: Property 'horizontalPadding_' does not
        // exist on type '{ __proto__: List; columns_: number; itemConstructor_:
        // (arg0: any) => GridItem; fixedHeight: boolean; getColumnCount_():
        // number; updateMetrics_(): void; columns: number; ... 8 more ...;
        // redraw(): void; }'.
        this.horizontalPadding_ === horizontalPadding) {
      // @ts-ignore: error TS2339: Property 'lastOffsetHeight_' does not exist
      // on type '{ __proto__: List; columns_: number; itemConstructor_: (arg0:
      // any) => GridItem; fixedHeight: boolean; getColumnCount_(): number;
      // updateMetrics_(): void; columns: number; ... 8 more ...; redraw():
      // void; }'.
      this.lastOffsetHeight_ = offsetHeight;
      return;
    }

    // @ts-ignore: error TS2339: Property 'lastOffsetWidth_' does not exist on
    // type '{ __proto__: List; columns_: number; itemConstructor_: (arg0: any)
    // => GridItem; fixedHeight: boolean; getColumnCount_(): number;
    // updateMetrics_(): void; columns: number; ... 8 more ...; redraw(): void;
    // }'.
    this.lastOffsetWidth_ = offsetWidth;
    // @ts-ignore: error TS2339: Property 'lastOffsetHeight_' does not exist on
    // type '{ __proto__: List; columns_: number; itemConstructor_: (arg0: any)
    // => GridItem; fixedHeight: boolean; getColumnCount_(): number;
    // updateMetrics_(): void; columns: number; ... 8 more ...; redraw(): void;
    // }'.
    this.lastOffsetHeight_ = offsetHeight;
    // @ts-ignore: error TS2339: Property 'lastOverflowY' does not exist on type
    // '{ __proto__: List; columns_: number; itemConstructor_: (arg0: any) =>
    // GridItem; fixedHeight: boolean; getColumnCount_(): number;
    // updateMetrics_(): void; columns: number; ... 8 more ...; redraw(): void;
    // }'.
    this.lastOverflowY = overflowY;
    // @ts-ignore: error TS2339: Property 'horizontalPadding_' does not exist on
    // type '{ __proto__: List; columns_: number; itemConstructor_: (arg0: any)
    // => GridItem; fixedHeight: boolean; getColumnCount_(): number;
    // updateMetrics_(): void; columns: number; ... 8 more ...; redraw(): void;
    // }'.
    this.horizontalPadding_ = horizontalPadding;
    this.columns_ = 0;

    if (overflowY === 'auto' && offsetWidth > 0) {
      // Column number may depend on whether scrollbar is present or not.
      // @ts-ignore: error TS2339: Property 'clientWidth' does not exist on type
      // '{ __proto__: List; columns_: number; itemConstructor_: (arg0: any) =>
      // GridItem; fixedHeight: boolean; getColumnCount_(): number;
      // updateMetrics_(): void; columns: number; ... 8 more ...; redraw():
      // void; }'.
      const originalClientWidth = this.clientWidth;
      // At first make sure there is no scrollbar and calculate clientWidth
      // (triggers reflow).
      // @ts-ignore: error TS2339: Property 'style' does not exist on type '{
      // __proto__: List; columns_: number; itemConstructor_: (arg0: any) =>
      // GridItem; fixedHeight: boolean; getColumnCount_(): number;
      // updateMetrics_(): void; columns: number; ... 8 more ...; redraw():
      // void; }'.
      this.style.overflowY = 'hidden';
      // @ts-ignore: error TS2339: Property 'clientWidth' does not exist on type
      // '{ __proto__: List; columns_: number; itemConstructor_: (arg0: any) =>
      // GridItem; fixedHeight: boolean; getColumnCount_(): number;
      // updateMetrics_(): void; columns: number; ... 8 more ...; redraw():
      // void; }'.
      this.clientWidthWithoutScrollbar_ = this.clientWidth;
      // @ts-ignore: error TS2339: Property 'clientHeight' does not exist on
      // type '{ __proto__: List; columns_: number; itemConstructor_: (arg0:
      // any) => GridItem; fixedHeight: boolean; getColumnCount_(): number;
      // updateMetrics_(): void; columns: number; ... 8 more ...; redraw():
      // void; }'.
      this.clientHeight_ = this.clientHeight;
      // @ts-ignore: error TS2339: Property 'clientWidth' does not exist on type
      // '{ __proto__: List; columns_: number; itemConstructor_: (arg0: any) =>
      // GridItem; fixedHeight: boolean; getColumnCount_(): number;
      // updateMetrics_(): void; columns: number; ... 8 more ...; redraw():
      // void; }'.
      if (this.clientWidth !== originalClientWidth) {
        // If clientWidth changed then previously scrollbar was shown.
        // @ts-ignore: error TS2339: Property 'clientWidthWithScrollbar_' does
        // not exist on type '{ __proto__: List; columns_: number;
        // itemConstructor_: (arg0: any) => GridItem; fixedHeight: boolean;
        // getColumnCount_(): number; updateMetrics_(): void; columns: number;
        // ... 8 more ...; redraw(): void; }'.
        this.clientWidthWithScrollbar_ = originalClientWidth;
      } else {
        // Show scrollbar and recalculate clientWidth (triggers reflow).
        // @ts-ignore: error TS2339: Property 'style' does not exist on type '{
        // __proto__: List; columns_: number; itemConstructor_: (arg0: any) =>
        // GridItem; fixedHeight: boolean; getColumnCount_(): number;
        // updateMetrics_(): void; columns: number; ... 8 more ...; redraw():
        // void; }'.
        this.style.overflowY = 'scroll';
        // @ts-ignore: error TS2339: Property 'clientWidth' does not exist on
        // type '{ __proto__: List; columns_: number; itemConstructor_: (arg0:
        // any) => GridItem; fixedHeight: boolean; getColumnCount_(): number;
        // updateMetrics_(): void; columns: number; ... 8 more ...; redraw():
        // void; }'.
        this.clientWidthWithScrollbar_ = this.clientWidth;
      }
      // @ts-ignore: error TS2339: Property 'style' does not exist on type '{
      // __proto__: List; columns_: number; itemConstructor_: (arg0: any) =>
      // GridItem; fixedHeight: boolean; getColumnCount_(): number;
      // updateMetrics_(): void; columns: number; ... 8 more ...; redraw():
      // void; }'.
      this.style.overflowY = '';
    } else {
      // @ts-ignore: error TS2339: Property 'clientWidthWithScrollbar_' does not
      // exist on type '{ __proto__: List; columns_: number; itemConstructor_:
      // (arg0: any) => GridItem; fixedHeight: boolean; getColumnCount_():
      // number; updateMetrics_(): void; columns: number; ... 8 more ...;
      // redraw(): void; }'.
      this.clientWidthWithoutScrollbar_ = this.clientWidthWithScrollbar_ =
          // @ts-ignore: error TS2339: Property 'clientWidth' does not exist on
          // type '{ __proto__: List; columns_: number; itemConstructor_: (arg0:
          // any) => GridItem; fixedHeight: boolean; getColumnCount_(): number;
          // updateMetrics_(): void; columns: number; ... 8 more ...; redraw():
          // void; }'.
          this.clientWidth;
      // @ts-ignore: error TS2339: Property 'clientHeight' does not exist on
      // type '{ __proto__: List; columns_: number; itemConstructor_: (arg0:
      // any) => GridItem; fixedHeight: boolean; getColumnCount_(): number;
      // updateMetrics_(): void; columns: number; ... 8 more ...; redraw():
      // void; }'.
      this.clientHeight_ = this.clientHeight;
    }
  },

  /**
   * The number of columns in the grid. If not set, determined automatically
   * as the maximum number of items fitting in the grid width.
   * @type {number}
   */
  get columns() {
    if (!this.columns_) {
      this.columns_ = this.getColumnCount_();
    }
    return this.columns_ || 1;
  },
  set columns(value) {
    if (value >= 0 && value !== this.columns_) {
      this.columns_ = value;
      this.redraw();
    }
  },

  /**
   * @param {number} index The index of the item.
   * @return {number} The top position of the item inside the list, not taking
   *     into account lead item. May vary in the case of multiple columns.
   * @override
   */
  getItemTop(index) {
    // @ts-ignore: error TS2339: Property 'getDefaultItemHeight_' does not exist
    // on type '{ __proto__: List; columns_: number; itemConstructor_: (arg0:
    // any) => GridItem; fixedHeight: boolean; getColumnCount_(): number;
    // updateMetrics_(): void; columns: number; ... 8 more ...; redraw(): void;
    // }'.
    return Math.floor(index / this.columns) * this.getDefaultItemHeight_();
  },

  /**
   * @param {number} index The index of the item.
   * @return {number} The row of the item. May vary in the case
   *     of multiple columns.
   * @override
   */
  getItemRow(index) {
    return Math.floor(index / this.columns);
  },

  /**
   * @param {number} row The row.
   * @return {number} The index of the first item in the row.
   * @override
   */
  getFirstItemInRow(row) {
    return row * this.columns;
  },

  /**
   * Creates the selection controller to use internally.
   * @param {ListSelectionModel} sm The underlying selection model.
   * @return {!ListSelectionController} The newly created selection
   *     controller.
   * @override
   */
  createSelectionController(sm) {
    // @ts-ignore: error TS2740: Type 'GridSelectionController' is missing the
    // following properties from type 'ListSelectionController': selectionModel,
    // getNextIndex, getPreviousIndex, getFirstIndex, and 4 more.
    return new GridSelectionController(sm, this);
  },

  /**
   * Calculates the number of items fitting in the given viewport.
   * @param {number} scrollTop The scroll top position.
   * @param {number} clientHeight The height of viewport.
   * @return {{first: number, length: number, last: number}} The index of
   *     first item in view port, The number of items, The item past the last.
   * @override
   */
  getItemsInViewPort(scrollTop, clientHeight) {
    // @ts-ignore: error TS2339: Property 'getDefaultItemHeight_' does not exist
    // on type '{ __proto__: List; columns_: number; itemConstructor_: (arg0:
    // any) => GridItem; fixedHeight: boolean; getColumnCount_(): number;
    // updateMetrics_(): void; columns: number; ... 8 more ...; redraw(): void;
    // }'.
    const itemHeight = this.getDefaultItemHeight_();
    const firstIndex =
        // @ts-ignore: error TS2339: Property 'getIndexForListOffset_' does not
        // exist on type '{ __proto__: List; columns_: number; itemConstructor_:
        // (arg0: any) => GridItem; fixedHeight: boolean; getColumnCount_():
        // number; updateMetrics_(): void; columns: number; ... 8 more ...;
        // redraw(): void; }'.
        this.autoExpands ? 0 : this.getIndexForListOffset_(scrollTop);
    const columns = this.columns;
    // @ts-ignore: error TS2339: Property 'autoExpands' does not exist on type
    // '{ __proto__: List; columns_: number; itemConstructor_: (arg0: any) =>
    // GridItem; fixedHeight: boolean; getColumnCount_(): number;
    // updateMetrics_(): void; columns: number; ... 8 more ...; redraw(): void;
    // }'.
    let count = this.autoExpands ?
        // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type
        // '{ __proto__: List; columns_: number; itemConstructor_: (arg0: any)
        // => GridItem; fixedHeight: boolean; getColumnCount_(): number;
        // updateMetrics_(): void; columns: number; ... 8 more ...; redraw():
        // void; }'.
        this.dataModel.length :
        Math.max(
            columns * (Math.ceil(clientHeight / itemHeight) + 1),
            // @ts-ignore: error TS2339: Property 'countItemsInRange_' does not
            // exist on type '{ __proto__: List; columns_: number;
            // itemConstructor_: (arg0: any) => GridItem; fixedHeight: boolean;
            // getColumnCount_(): number; updateMetrics_(): void; columns:
            // number; ... 8 more ...; redraw(): void; }'.
            this.countItemsInRange_(firstIndex, scrollTop + clientHeight));
    count = columns * Math.ceil(count / columns);
    // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type '{
    // __proto__: List; columns_: number; itemConstructor_: (arg0: any) =>
    // GridItem; fixedHeight: boolean; getColumnCount_(): number;
    // updateMetrics_(): void; columns: number; ... 8 more ...; redraw(): void;
    // }'.
    count = Math.min(count, this.dataModel.length - firstIndex);
    return {first: firstIndex, length: count, last: firstIndex + count - 1};
  },

  /**
   * Merges list items. Calls the base class implementation and then
   * puts spacers on the right places.
   * @param {number} firstIndex The index of first item, inclusively.
   * @param {number} lastIndex The index of last item, exclusively.
   * @override
   */
  mergeItems(firstIndex, lastIndex) {
    // @ts-ignore: error TS2339: Property 'mergeItems' does not exist on type
    // 'List'.
    List.prototype.mergeItems.call(this, firstIndex, lastIndex);

    // @ts-ignore: error TS2339: Property 'afterFiller_' does not exist on type
    // '{ __proto__: List; columns_: number; itemConstructor_: (arg0: any) =>
    // GridItem; fixedHeight: boolean; getColumnCount_(): number;
    // updateMetrics_(): void; columns: number; ... 8 more ...; redraw(): void;
    // }'.
    const afterFiller = this.afterFiller_;
    const columns = this.columns;

    // @ts-ignore: error TS2339: Property 'beforeFiller_' does not exist on type
    // '{ __proto__: List; columns_: number; itemConstructor_: (arg0: any) =>
    // GridItem; fixedHeight: boolean; getColumnCount_(): number;
    // updateMetrics_(): void; columns: number; ... 8 more ...; redraw(): void;
    // }'.
    for (let item = this.beforeFiller_.nextSibling; item !== afterFiller;) {
      const next = item.nextSibling;
      if (isSpacer(item)) {
        // Spacer found on a place it mustn't be.
        // @ts-ignore: error TS2339: Property 'removeChild' does not exist on
        // type '{ __proto__: List; columns_: number; itemConstructor_: (arg0:
        // any) => GridItem; fixedHeight: boolean; getColumnCount_(): number;
        // updateMetrics_(): void; columns: number; ... 8 more ...; redraw():
        // void; }'.
        this.removeChild(item);
        item = next;
        continue;
      }
      const index = item.listIndex;
      const nextIndex = index + 1;

      // Invisible pinned item could be outside of the
      // [firstIndex, lastIndex). Ignore it.
      if (index >= firstIndex && nextIndex < lastIndex &&
          nextIndex % columns === 0) {
        if (isSpacer(next)) {
          // Leave the spacer on its place.
          item = next.nextSibling;
        } else {
          // Insert spacer.
          // @ts-ignore: error TS2339: Property 'ownerDocument' does not exist
          // on type '{ __proto__: List; columns_: number; itemConstructor_:
          // (arg0: any) => GridItem; fixedHeight: boolean; getColumnCount_():
          // number; updateMetrics_(): void; columns: number; ... 8 more ...;
          // redraw(): void; }'.
          const spacer = this.ownerDocument.createElement('div');
          spacer.className = 'spacer';
          // @ts-ignore: error TS2339: Property 'insertBefore' does not exist on
          // type '{ __proto__: List; columns_: number; itemConstructor_: (arg0:
          // any) => GridItem; fixedHeight: boolean; getColumnCount_(): number;
          // updateMetrics_(): void; columns: number; ... 8 more ...; redraw():
          // void; }'.
          this.insertBefore(spacer, next);
          item = next;
        }
      } else {
        item = next;
      }
    }

    // @ts-ignore: error TS7006: Parameter 'child' implicitly has an 'any' type.
    function isSpacer(child) {
      return child.classList.contains('spacer') &&
          child !== afterFiller;  // Must not be removed.
    }
  },

  /**
   * Returns the height of after filler in the list.
   * @param {number} lastIndex The index of item past the last in viewport.
   * @return {number} The height of after filler.
   * @override
   */
  getAfterFillerHeight(lastIndex) {
    const columns = this.columns;
    // @ts-ignore: error TS2339: Property 'getDefaultItemHeight_' does not exist
    // on type '{ __proto__: List; columns_: number; itemConstructor_: (arg0:
    // any) => GridItem; fixedHeight: boolean; getColumnCount_(): number;
    // updateMetrics_(): void; columns: number; ... 8 more ...; redraw(): void;
    // }'.
    const itemHeight = this.getDefaultItemHeight_();
    // We calculate the row of last item, and the row of last shown item.
    // The difference is the number of rows not shown.
    // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type '{
    // __proto__: List; columns_: number; itemConstructor_: (arg0: any) =>
    // GridItem; fixedHeight: boolean; getColumnCount_(): number;
    // updateMetrics_(): void; columns: number; ... 8 more ...; redraw(): void;
    // }'.
    const afterRows = Math.floor((this.dataModel.length - 1) / columns) -
        Math.floor((lastIndex - 1) / columns);
    return afterRows * itemHeight;
  },

  /**
   * Returns true if the child is a list item.
   * @param {Node} child Child of the list.
   * @return {boolean} True if a list item.
   */
  isItem(child) {
    // Non-items are before-, afterFiller and spacers added in mergeItems.
    return child.nodeType === Node.ELEMENT_NODE &&
        // @ts-ignore: error TS2339: Property 'classList' does not exist on type
        // 'Node'.
        !child.classList.contains('spacer');
  },

  redraw() {
    this.updateMetrics_();
    // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type '{
    // __proto__: List; columns_: number; itemConstructor_: (arg0: any) =>
    // GridItem; fixedHeight: boolean; getColumnCount_(): number;
    // updateMetrics_(): void; columns: number; ... 8 more ...; redraw(): void;
    // }'.
    const itemCount = this.dataModel ? this.dataModel.length : 0;
    // @ts-ignore: error TS2339: Property 'lastItemCount_' does not exist on
    // type '{ __proto__: List; columns_: number; itemConstructor_: (arg0: any)
    // => GridItem; fixedHeight: boolean; getColumnCount_(): number;
    // updateMetrics_(): void; columns: number; ... 8 more ...; redraw(): void;
    // }'.
    if (this.lastItemCount_ !== itemCount) {
      // @ts-ignore: error TS2339: Property 'lastItemCount_' does not exist on
      // type '{ __proto__: List; columns_: number; itemConstructor_: (arg0:
      // any) => GridItem; fixedHeight: boolean; getColumnCount_(): number;
      // updateMetrics_(): void; columns: number; ... 8 more ...; redraw():
      // void; }'.
      this.lastItemCount_ = itemCount;
      // Force recalculation.
      this.columns_ = 0;
    }

    // @ts-ignore: error TS2339: Property 'redraw' does not exist on type
    // 'List'.
    List.prototype.redraw.call(this);
  },
};

/**
 * Creates a selection controller that is to be used with grids.
 * @param {ListSelectionModel} selectionModel The selection model to
 *     interact with.
 * @param {Grid} grid The grid to interact with.
 * @constructor
 * @extends {ListSelectionController}
 */
// @ts-ignore: error TS8022: JSDoc '@extends' is not attached to a class.
export function GridSelectionController(selectionModel, grid) {
  this.selectionModel_ = selectionModel;
  this.grid_ = grid;
}

GridSelectionController.prototype = {
  __proto__: ListSelectionController.prototype,

  /**
   * Check if accessibility is enabled: if ChromeVox is running
   * (which provides spoken feedback for accessibility), make up/down
   * behave the same as left/right. That's because the 2-dimensional
   * structure of the grid isn't exposed, so it makes more sense to a
   * user who is relying on spoken feedback to flatten it.
   * @return {boolean} True if accessibility is enabled.
   */
  isAccessibilityEnabled() {
    // @ts-ignore: error TS2339: Property 'cvox' does not exist on type 'Window
    // & typeof globalThis'.
    return window.cvox && window.cvox.Api &&
        // @ts-ignore: error TS2339: Property 'cvox' does not exist on type
        // 'Window & typeof globalThis'.
        window.cvox.Api.isChromeVoxActive &&
        // @ts-ignore: error TS2339: Property 'cvox' does not exist on type
        // 'Window & typeof globalThis'.
        window.cvox.Api.isChromeVoxActive();
  },

  /**
   * Returns the index below (y axis) the given element.
   * @param {number} index The index to get the index below.
   * @return {number} The index below or -1 if not found.
   * @override
   */
  getIndexBelow(index) {
    if (this.isAccessibilityEnabled()) {
      return this.getIndexAfter(index);
    }
    // @ts-ignore: error TS2339: Property 'getLastIndex' does not exist on type
    // 'GridSelectionController'.
    const last = this.getLastIndex();
    if (index === last) {
      return -1;
    }
    // @ts-ignore: error TS2339: Property 'columns' does not exist on type
    // 'Grid'.
    index += this.grid_.columns;
    return Math.min(index, last);
  },

  /**
   * Returns the index above (y axis) the given element.
   * @param {number} index The index to get the index above.
   * @return {number} The index below or -1 if not found.
   * @override
   */
  getIndexAbove(index) {
    if (this.isAccessibilityEnabled()) {
      return this.getIndexBefore(index);
    }
    if (index === 0) {
      return -1;
    }
    // @ts-ignore: error TS2339: Property 'columns' does not exist on type
    // 'Grid'.
    index -= this.grid_.columns;
    return Math.max(index, 0);
  },

  /**
   * Returns the index before (x axis) the given element.
   * @param {number} index The index to get the index before.
   * @return {number} The index before or -1 if not found.
   * @override
   */
  getIndexBefore(index) {
    return index - 1;
  },

  /**
   * Returns the index after (x axis) the given element.
   * @param {number} index The index to get the index after.
   * @return {number} The index after or -1 if not found.
   * @override
   */
  getIndexAfter(index) {
    // @ts-ignore: error TS2339: Property 'getLastIndex' does not exist on type
    // 'GridSelectionController'.
    if (index === this.getLastIndex()) {
      return -1;
    }
    return index + 1;
  },
};
