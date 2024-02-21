// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {crInjectTypeAndInit} from '../../../common/js/cr_ui.js';

import {List} from './list.js';
import {ListItem} from './list_item.js';
import {ListSelectionController} from './list_selection_controller.js';
import type {ListSelectionModel} from './list_selection_model.js';

/**
 * @fileoverview This implements a grid control. Grid contains a bunch of
 * similar elements placed in multiple columns. It's pretty similar to the list,
 * except the multiple columns layout.
 */

export function createGridItem(dataItem: any): GridItem {
  const el = document.createElement('li') as GridItem;
  el.dataItem = dataItem;
  crInjectTypeAndInit(el, GridItem);
  return el;
}

/** Creates a new grid item element. */
export class GridItem extends ListItem {
  dataItem: any;

  /**
   * Called when an element is decorated as a grid item.
   */
  override initialize() {
    super.initialize();
    this.textContent = this.dataItem;
  }
}

/**
 * Creates a new grid element.
 */
export abstract class Grid extends List {
  /**
   * The number of columns in the grid. Either set by the user, or lazy
   * calculated as the maximum number of items fitting in the grid width.
   */
  private columns_: number = 0;

  /**
   * Function used to create grid items.
   */
  protected override itemConstructor_:
      (..._: any[]) => GridItem = createGridItem;

  private lastItemCount_: number = 0;
  private lastOffsetWidth_: number = 0;
  private lastOverflowY: string = '';
  private horizontalPadding_: number = 0;
  private clientWidthWithoutScrollbar_: number = 0;
  private clientHeight_: number = 0;
  private clientWidthWithScrollbar_: number = 0;

  /**
   * Initializes the element.
   */
  override initialize() {
    this.columns_ = 0;
    super.initialize();
  }

  /**
   * Whether or not the rows on list have various heights.
   * Shows a warning at the setter because Grid does not support this.
   */
  override get fixedHeight(): boolean {
    return true;
  }

  override set fixedHeight(fixedHeight) {
    if (!fixedHeight) {
      console.warn('Grid does not support fixedHeight = false');
    }
  }

  /**
   * The number of columns determined by width of the grid and width of the
   * items.
   */
  private getColumnCount_(): number {
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

    const itemCount = this.dataModel ? this.dataModel.length : 0;
    if (!itemCount) {
      return 0;
    }

    const columns = Math.floor(
        (this.clientWidthWithoutScrollbar_ - this.horizontalPadding_) / width);
    if (!columns) {
      return 0;
    }

    const rows = Math.ceil(itemCount / columns);
    if (rows * height <= this.clientHeight_) {
      // Content fits within the client area (no scrollbar required).
      return columns;
    }

    // If the content doesn't fit within the client area, the number of
    // columns should be calculated with consideration for scrollbar's width.
    return Math.floor(
        (this.clientWidthWithScrollbar_ - this.horizontalPadding_) / width);
  }

  /**
   * Measure and cache client width and height with and without scrollbar.
   * Must be updated when offsetWidth and/or offsetHeight changed.
   */
  private updateMetrics_() {
    // Check changings that may affect number of columns.
    const offsetWidth = this.offsetWidth;
    const style = window.getComputedStyle(this);
    const overflowY = style.overflowY;
    const horizontalPadding =
        parseFloat(style.paddingLeft) + parseFloat(style.paddingRight);

    if (this.lastOffsetWidth_ === offsetWidth &&
        this.lastOverflowY === overflowY &&
        this.horizontalPadding_ === horizontalPadding) {
      return;
    }

    this.lastOffsetWidth_ = offsetWidth;
    this.lastOverflowY = overflowY;
    this.horizontalPadding_ = horizontalPadding;
    this.columns_ = 0;

    if (overflowY === 'auto' && offsetWidth > 0) {
      // Column number may depend on whether scrollbar is present or not.
      const originalClientWidth = this.clientWidth;
      // At first make sure there is no scrollbar and calculate clientWidth
      // (triggers reflow).
      this.style.overflowY = 'hidden';
      this.clientWidthWithoutScrollbar_ = this.clientWidth;
      this.clientHeight_ = this.clientHeight;
      if (this.clientWidth !== originalClientWidth) {
        // If clientWidth changed then previously scrollbar was shown.
        this.clientWidthWithScrollbar_ = originalClientWidth;
      } else {
        // Show scrollbar and recalculate clientWidth (triggers reflow).
        this.style.overflowY = 'scroll';
        this.clientWidthWithScrollbar_ = this.clientWidth;
      }
      this.style.overflowY = '';
    } else {
      this.clientWidthWithoutScrollbar_ = this.clientWidthWithScrollbar_ =
          this.clientWidth;
      this.clientHeight_ = this.clientHeight;
    }
  }

  /**
   * The number of columns in the grid. If not set, determined automatically
   * as the maximum number of items fitting in the grid width.
   */
  get columns(): number {
    if (!this.columns_) {
      this.columns_ = this.getColumnCount_();
    }
    return this.columns_ || 1;
  }

  set columns(value: number) {
    if (value >= 0 && value !== this.columns_) {
      this.columns_ = value;
      this.redraw();
    }
  }

  /**
   * The top position of the item inside the list, not taking into account lead
   * item. May vary in the case of multiple columns.
   * @param index The index of the item.
   */
  override getItemTop(index: number): number {
    return Math.floor(index / this.columns) * this.getDefaultItemHeight_();
  }

  /**
   * @param index The index of the item.
   * @return The row of the item. May vary in the case of multiple columns.
   */
  override getItemRow(index: number): number {
    return Math.floor(index / this.columns);
  }

  /**
   * @param row The row.
   * @return The index of the first item in the row.
   */
  override getFirstItemInRow(row: number): number {
    return row * this.columns;
  }

  /**
   * Creates the selection controller to use internally.
   * @param sm The underlying selection model.
   * @return The newly created selection controller.
   */
  override createSelectionController(sm: ListSelectionModel):
      ListSelectionController {
    return new GridSelectionController(sm, this);
  }

  /**
   * Calculates the number of items fitting in the given viewport.
   * @param scrollTop The scroll top position.
   * @param clientHeight The height of viewport.
   * @return The index of first item in view port, The number of items, The item
   *     past the last.
   */
  override getItemsInViewPort(scrollTop: number, clientHeight: number):
      {first: number, length: number, last: number} {
    const itemHeight = this.getDefaultItemHeight_();
    const firstIndex =
        this.autoExpands ? 0 : this.getIndexForListOffset_(scrollTop);
    const columns = this.columns;
    assert(this.dataModel);
    let count = this.autoExpands ?
        this.dataModel.length :
        Math.max(
            columns * (Math.ceil(clientHeight / itemHeight) + 1),
            this.countItemsInRange_(firstIndex, scrollTop + clientHeight));
    count = columns * Math.ceil(count / columns);
    count = Math.min(count, this.dataModel.length - firstIndex);
    return {first: firstIndex, length: count, last: firstIndex + count - 1};
  }

  /**
   * Merges list items. Calls the base class implementation and then puts
   * spacers on the right places.
   * @param firstIndex The index of first item, inclusively.
   * @param lastIndex The index of last item, exclusively.
   */
  override mergeItems(firstIndex: number, lastIndex: number) {
    super.mergeItems(firstIndex, lastIndex);

    const afterFiller = this.afterFiller_;
    const columns = this.columns;

    assert(this.beforeFiller_);

    function isSpacer(child: GridItem|null) {
      return child && child.classList.contains('spacer') &&
          child !== afterFiller;  // Must not be removed.
    }

    for (let item: GridItem|null = (this.beforeFiller_.nextSibling as GridItem);
         item !== afterFiller;) {
      const next = item.nextSibling!;
      if (isSpacer(item)) {
        // Spacer found on a place it mustn't be.
        this.removeChild(item!);
        item = next as GridItem;
        continue;
      }
      const index = item!.listIndex;
      const nextIndex = index + 1;

      // Invisible pinned item could be outside of the
      // [firstIndex, lastIndex). Ignore it.
      if (index >= firstIndex && nextIndex < lastIndex &&
          nextIndex % columns === 0) {
        if (isSpacer(next as GridItem)) {
          // Leave the spacer on its place.
          item = next.nextSibling as GridItem;
        } else {
          // Insert spacer.
          const spacer = this.ownerDocument.createElement('div');
          spacer.className = 'spacer';
          this.insertBefore(spacer, next);
          item = next as GridItem;
        }
      } else {
        item = next as GridItem;
      }
    }
  }

  /**
   * Returns the height of after filler in the list.
   * @param lastIndex The index of item past the last in viewport.
   */
  override getAfterFillerHeight(lastIndex: number): number {
    const columns = this.columns;
    const itemHeight = this.getDefaultItemHeight_();
    assert(this.dataModel);
    // We calculate the row of last item, and the row of last shown item.
    const afterRows = Math.floor((this.dataModel.length - 1) / columns) -
        Math.floor((lastIndex - 1) / columns);
    return afterRows * itemHeight;
  }

  /**
   * Returns true if the child is a list item.
   * @param child Child of the list.
   * @return True if a list item.
   */
  override isItem(child: HTMLElement): boolean {
    // Non-items are before-, afterFiller and spacers added in mergeItems.
    return child.nodeType === Node.ELEMENT_NODE &&
        !child.classList.contains('spacer');
  }

  override redraw() {
    this.updateMetrics_();
    const itemCount = this.dataModel ? this.dataModel.length : 0;
    if (this.lastItemCount_ !== itemCount) {
      this.lastItemCount_ = itemCount;
      // Force recalculation.
      this.columns_ = 0;
    }

    super.redraw();
  }
}

/**
 * Creates a selection controller that is to be used with grids.
 */
export class GridSelectionController extends ListSelectionController {
  /**
   * Creates a selection controller that is to be used with grids.
   * @param selectionModel The selection model to interact with.
   * @param grid The grid to interact with.
   */
  constructor(selectionModel: ListSelectionModel, protected grid_: Grid) {
    super(selectionModel);
  }

  /**
   * Check if accessibility is enabled: if ChromeVox is running
   * (which provides spoken feedback for accessibility), make up/down
   * behave the same as left/right. That's because the 2-dimensional
   * structure of the grid isn't exposed, so it makes more sense to a
   * user who is relying on spoken feedback to flatten it.
   */
  isAccessibilityEnabled(): boolean {
    return window.cvox?.Api?.isChromeVoxActive() ?? false;
  }

  /**
   * Returns the index below (y axis) the given element.
   * @param index The index to get the index below.
   * @return The index below or -1 if not found.
   */
  override getIndexBelow(index: number): number {
    if (this.isAccessibilityEnabled()) {
      return this.getIndexAfter(index);
    }
    const last = this.getLastIndex();
    if (index === last) {
      return -1;
    }
    index += this.grid_.columns;
    return Math.min(index, last);
  }

  /**
   * Returns the index above (y axis) the given element.
   * @param  index The index to get the index above.
   * @return  The index below or -1 if not found.
   */
  override getIndexAbove(index: number): number {
    if (this.isAccessibilityEnabled()) {
      return this.getIndexBefore(index);
    }
    if (index === 0) {
      return -1;
    }
    index -= this.grid_.columns;
    return Math.max(index, 0);
  }

  /**
   * Returns the index before (x axis) the given element.
   * @param index The index to get the index before.
   * @return The index before or -1 if not found.
   */
  override getIndexBefore(index: number): number {
    return index - 1;
  }

  /**
   * Returns the index after (x axis) the given element.
   * @param index The index to get the index after.
   * @return The index after or -1 if not found.
   */
  override getIndexAfter(index: number): number {
    if (index === this.getLastIndex()) {
      return -1;
    }
    return index + 1;
  }
}
