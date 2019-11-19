// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This extends cr.ui.List for use in the table.
 */

cr.define('cr.ui.table', function() {
  /** @const */ const List = cr.ui.List;

  /**
   * Creates a new table list element.
   */
  class TableList extends cr.ui.List {
    /**
     * @param {Object=} opt_propertyBag Optional properties.
     */
    constructor(opt_propertyBag) {
      super(opt_propertyBag);

      /** @private {cr.ui.Table} */
      this.table_ = null;

      /**
       * Actually defined and managed by cr.ui.List.
       * @private {HTMLElement}
       * */
      this.afterFiller_;
    }

    static decorate(el) {
      cr.ui.List.decorate(el);
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
        List.prototype.redraw.call(this);
      }  // Redraw items only.
      this.resizeCells_();
    }

    /**
     * Updates width of cells.
     */
    resizeCells_() {
      const cm = this.table_.columnModel;
      for (let row = this.firstElementChild; row;
           row = row.nextElementSibling) {
        if (row.tagName != 'LI') {
          continue;
        }

        for (let i = 0; i < cm.size; i++) {
          row.children[i].style.width = cm.getWidth(i) + 'px';
        }
        row.style.width = cm.totalWidth + 'px';
      }
      this.afterFiller_.style.width = cm.totalWidth + 'px';
    }

    /**
     * Redraws the viewport.
     */
    redraw() {
      if (this.batchCount_ != 0) {
        return;
      }
      this.updateScrollbars_();

      List.prototype.redraw.call(this);
      this.resizeCells_();
    }

    /**
     * Returns the height of after filler in the list.
     * @param {number} lastIndex The index of item past the last in viewport.
     * @return {number} The height of after filler.
     * @override
     */
    getAfterFillerHeight(lastIndex) {
      // If the list is empty set height to 1 to show horizontal
      // scroll bar.
      return lastIndex == 0 ?
          1 :
          cr.ui.List.prototype.getAfterFillerHeight.call(this, lastIndex);
    }

    /**
     * Shows or hides vertical and horizontal scroll bars in the list.
     * @return {boolean} True if horizontal scroll bar changed.
     */
    updateScrollbars_() {
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
      return this.getItemTop(this.dataModel.length) <= visibleHeight;
    }

    /**
     * Creates a new list item.
     * @param {*} dataItem The value to use for the item.
     * @return {!cr.ui.ListItem} The newly created list item.
     */
    createItem(dataItem) {
      return /** @type {!cr.ui.ListItem} */ (
          this.table_.getRenderFunction().call(null, dataItem, this.table_));
    }

    /**
     * Determines whether a full redraw is required.
     * @return {boolean}
     */
    needsFullRedraw_() {
      const cm = this.table_.columnModel;
      const row = this.firstElementChild;
      // If the number of columns in the model has changed, a full redraw is
      // needed.
      if (row.children.length != cm.size) {
        return true;
      }
      // If the column visibility has changed, a full redraw is required.
      for (let i = 0; i < cm.size; ++i) {
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
  cr.defineProperty(TableList, 'table');

  return {
    TableList: TableList,
  };
});
