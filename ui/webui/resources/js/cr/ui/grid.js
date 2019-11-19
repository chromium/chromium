// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// require: list_selection_model.js
// require: list_selection_controller.js
// require: list.js

/**
 * @fileoverview This implements a grid control. Grid contains a bunch of
 * similar elements placed in multiple columns. It's pretty similar to the list,
 * except the multiple columns layout.
 */

cr.define('cr.ui', function() {
  /** @const */ const ListSelectionController = cr.ui.ListSelectionController;
  /** @const */ const List = cr.ui.List;
  /** @const */ const ListItem = cr.ui.ListItem;

  /**
   * Creates a new grid item element.
   * @param {*} dataItem The data item.
   * @constructor
   * @extends {cr.ui.ListItem}
   */
  function GridItem(dataItem) {
    const el = document.createElement('li');
    el.dataItem = dataItem;
    el.__proto__ = GridItem.prototype;
    return el;
  }

  GridItem.prototype = {
    __proto__: ListItem.prototype,

    /**
     * Called when an element is decorated as a grid item.
     */
    decorate: function() {
      ListItem.prototype.decorate.apply(this, arguments);
      this.textContent = this.dataItem;
    }
  };

  /**
   * Creates a new grid element.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {cr.ui.List}
   */
  const Grid = cr.ui.define('grid');

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
     * @type {function(new:cr.ui.GridItem, *)}
     * @override
     */
    itemConstructor_: GridItem,

    /**
     * Whether or not the rows on list have various heights.
     * Shows a warning at the setter because cr.ui.Grid does not support this.
     * @type {boolean}
     */
    get fixedHeight() {
      return true;
    },
    set fixedHeight(fixedHeight) {
      if (!fixedHeight) {
        console.warn('cr.ui.Grid does not support fixedHeight = false');
      }
    },

    /**
     * @return {number} The number of columns determined by width of the grid
     *     and width of the items.
     * @private
     */
    getColumnCount_: function() {
      // Size comes here with margin already collapsed.
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
          (this.clientWidthWithoutScrollbar_ - this.horizontalPadding_) /
          width);
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
    },

    /**
     * Measure and cache client width and height with and without scrollbar.
     * Must be updated when offsetWidth and/or offsetHeight changed.
     */
    updateMetrics_: function() {
      // Check changings that may affect number of columns.
      const offsetWidth = this.offsetWidth;
      const offsetHeight = this.offsetHeight;
      const style = window.getComputedStyle(this);
      const overflowY = style.overflowY;
      const horizontalPadding =
          parseFloat(style.paddingLeft) + parseFloat(style.paddingRight);

      if (this.lastOffsetWidth_ == offsetWidth &&
          this.lastOverflowY == overflowY &&
          this.horizontalPadding_ == horizontalPadding) {
        this.lastOffsetHeight_ = offsetHeight;
        return;
      }

      this.lastOffsetWidth_ = offsetWidth;
      this.lastOffsetHeight_ = offsetHeight;
      this.lastOverflowY = overflowY;
      this.horizontalPadding_ = horizontalPadding;
      this.columns_ = 0;

      if (overflowY == 'auto' && offsetWidth > 0) {
        // Column number may depend on whether scrollbar is present or not.
        const originalClientWidth = this.clientWidth;
        // At first make sure there is no scrollbar and calculate clientWidth
        // (triggers reflow).
        this.style.overflowY = 'hidden';
        this.clientWidthWithoutScrollbar_ = this.clientWidth;
        this.clientHeight_ = this.clientHeight;
        if (this.clientWidth != originalClientWidth) {
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
      if (value >= 0 && value != this.columns_) {
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
    getItemTop: function(index) {
      return Math.floor(index / this.columns) * this.getDefaultItemHeight_();
    },

    /**
     * @param {number} index The index of the item.
     * @return {number} The row of the item. May vary in the case
     *     of multiple columns.
     * @override
     */
    getItemRow: function(index) {
      return Math.floor(index / this.columns);
    },

    /**
     * @param {number} row The row.
     * @return {number} The index of the first item in the row.
     * @override
     */
    getFirstItemInRow: function(row) {
      return row * this.columns;
    },

    /**
     * Creates the selection controller to use internally.
     * @param {cr.ui.ListSelectionModel} sm The underlying selection model.
     * @return {!cr.ui.ListSelectionController} The newly created selection
     *     controller.
     * @override
     */
    createSelectionController: function(sm) {
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
    getItemsInViewPort: function(scrollTop, clientHeight) {
      const itemHeight = this.getDefaultItemHeight_();
      const firstIndex =
          this.autoExpands ? 0 : this.getIndexForListOffset_(scrollTop);
      const columns = this.columns;
      let count = this.autoExpands ?
          this.dataModel.length :
          Math.max(
              columns * (Math.ceil(clientHeight / itemHeight) + 1),
              this.countItemsInRange_(firstIndex, scrollTop + clientHeight));
      count = columns * Math.ceil(count / columns);
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
    mergeItems: function(firstIndex, lastIndex) {
      List.prototype.mergeItems.call(this, firstIndex, lastIndex);

      const afterFiller = this.afterFiller_;
      const columns = this.columns;

      for (let item = this.beforeFiller_.nextSibling; item != afterFiller;) {
        const next = item.nextSibling;
        if (isSpacer(item)) {
          // Spacer found on a place it mustn't be.
          this.removeChild(item);
          item = next;
          continue;
        }
        const index = item.listIndex;
        const nextIndex = index + 1;

        // Invisible pinned item could be outside of the
        // [firstIndex, lastIndex). Ignore it.
        if (index >= firstIndex && nextIndex < lastIndex &&
            nextIndex % columns == 0) {
          if (isSpacer(next)) {
            // Leave the spacer on its place.
            item = next.nextSibling;
          } else {
            // Insert spacer.
            const spacer = this.ownerDocument.createElement('div');
            spacer.className = 'spacer';
            this.insertBefore(spacer, next);
            item = next;
          }
        } else {
          item = next;
        }
      }

      function isSpacer(child) {
        return child.classList.contains('spacer') &&
            child != afterFiller;  // Must not be removed.
      }
    },

    /**
     * Returns the height of after filler in the list.
     * @param {number} lastIndex The index of item past the last in viewport.
     * @return {number} The height of after filler.
     * @override
     */
    getAfterFillerHeight: function(lastIndex) {
      const columns = this.columns;
      const itemHeight = this.getDefaultItemHeight_();
      // We calculate the row of last item, and the row of last shown item.
      // The difference is the number of rows not shown.
      const afterRows = Math.floor((this.dataModel.length - 1) / columns) -
          Math.floor((lastIndex - 1) / columns);
      return afterRows * itemHeight;
    },

    /**
     * Returns true if the child is a list item.
     * @param {Node} child Child of the list.
     * @return {boolean} True if a list item.
     */
    isItem: function(child) {
      // Non-items are before-, afterFiller and spacers added in mergeItems.
      return child.nodeType == Node.ELEMENT_NODE &&
          !child.classList.contains('spacer');
    },

    redraw: function() {
      this.updateMetrics_();
      const itemCount = this.dataModel ? this.dataModel.length : 0;
      if (this.lastItemCount_ != itemCount) {
        this.lastItemCount_ = itemCount;
        // Force recalculation.
        this.columns_ = 0;
      }

      List.prototype.redraw.call(this);
    }
  };

  /**
   * Creates a selection controller that is to be used with grids.
   * @param {cr.ui.ListSelectionModel} selectionModel The selection model to
   *     interact with.
   * @param {cr.ui.Grid} grid The grid to interact with.
   * @constructor
   * @extends {cr.ui.ListSelectionController}
   */
  function GridSelectionController(selectionModel, grid) {
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
    isAccessibilityEnabled: function() {
      return window.cvox && window.cvox.Api &&
          window.cvox.Api.isChromeVoxActive &&
          window.cvox.Api.isChromeVoxActive();
    },

    /**
     * Returns the index below (y axis) the given element.
     * @param {number} index The index to get the index below.
     * @return {number} The index below or -1 if not found.
     * @override
     */
    getIndexBelow: function(index) {
      if (this.isAccessibilityEnabled()) {
        return this.getIndexAfter(index);
      }
      const last = this.getLastIndex();
      if (index == last) {
        return -1;
      }
      index += this.grid_.columns;
      return Math.min(index, last);
    },

    /**
     * Returns the index above (y axis) the given element.
     * @param {number} index The index to get the index above.
     * @return {number} The index below or -1 if not found.
     * @override
     */
    getIndexAbove: function(index) {
      if (this.isAccessibilityEnabled()) {
        return this.getIndexBefore(index);
      }
      if (index == 0) {
        return -1;
      }
      index -= this.grid_.columns;
      return Math.max(index, 0);
    },

    /**
     * Returns the index before (x axis) the given element.
     * @param {number} index The index to get the index before.
     * @return {number} The index before or -1 if not found.
     * @override
     */
    getIndexBefore: function(index) {
      return index - 1;
    },

    /**
     * Returns the index after (x axis) the given element.
     * @param {number} index The index to get the index after.
     * @return {number} The index after or -1 if not found.
     * @override
     */
    getIndexAfter: function(index) {
      if (index == this.getLastIndex()) {
        return -1;
      }
      return index + 1;
    }
  };

  return {
    Grid: Grid,
    GridItem: GridItem,
    GridSelectionController: GridSelectionController
  };
});
