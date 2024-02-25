// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This extends List for use in the table.
 */

import {jsSetter} from '../../../../common/js/cr_ui.js';
import {List} from '../list.js';
import type {ListItem} from '../list_item.js';

import type {Table} from './table.js';


/**
 * Creates a new table list element.
 */
export abstract class TableList extends List {
  private table_: Table|null = null;

  override initialize() {
    super.initialize();
    this.className = 'list';
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
  private resizeCells_() {
    const cm = this.table_!.columnModel;
    for (let row = this.firstElementChild; row; row = row.nextElementSibling) {
      if (row.tagName !== 'LI') {
        continue;
      }

      for (let i = 0; i < cm.size; i++) {
        const child = row.children[i]! as HTMLElement;
        child.style.width = cm.getWidth(i) + 'px';
      }
      (row as HTMLElement).style.width = cm.totalWidth + 'px';
    }
    if (this.afterFiller_) {
      this.afterFiller_.style.width = cm.totalWidth + 'px';
    }
  }

  /**
   * Redraws the viewport.
   */
  override redraw() {
    if (this.batchCount_ !== 0) {
      return;
    }
    this.updateScrollbars_();

    List.prototype.redraw.call(this);
    this.resizeCells_();
  }

  /**
   * Returns the height of after filler in the list.
   * @param lastIndex The index of item past the last in viewport.
   * @return The height of after filler.
   */
  override getAfterFillerHeight(lastIndex: number): number {
    // If the list is empty set height to 1 to show horizontal
    // scroll bar.
    return lastIndex === 0 ?
        1 :
        List.prototype.getAfterFillerHeight.call(this, lastIndex);
  }

  /**
   * Shows or hides vertical and horizontal scroll bars in the list.
   * @return True if horizontal scroll bar changed.
   */
  private updateScrollbars_(): boolean {
    const cm = this.table_!.columnModel;
    const style = this.style;
    if (!cm || cm.size === 0) {
      if (style.overflow !== 'hidden') {
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
      if (style.overflowX !== 'scroll') {
        style.overflowX = 'scroll';
      }
      // Once we sure there will be horizontal
      // scrollbar calculate with this height.
      height = this.clientHeight;
    }
    if (this.areAllItemsVisible_(height)) {
      if (cm.totalWidth <= offsetWidth && style.overflowX !== 'hidden') {
        style.overflowX = 'hidden';
      }
      changed = this.showVerticalScrollBar_(false);
    } else {
      changed = this.showVerticalScrollBar_(true);
      const x = cm.totalWidth <= this.clientWidth ? 'hidden' : 'scroll';
      if (style.overflowX !== x) {
        style.overflowX = x;
      }
    }
    return changed;
  }

  /**
   * Shows or hides vertical scroll bar.
   * @param show True to show.
   * @return True if visibility changed.
   */
  private showVerticalScrollBar_(show: boolean): boolean {
    const style = this.style;
    if (show && style.overflowY === 'scroll') {
      return false;
    }
    if (!show && style.overflowY === 'hidden') {
      return false;
    }
    style.overflowY = show ? 'scroll' : 'hidden';
    return true;
  }

  /**
   * @param visibleHeight Height in pixels.
   * @return True if all rows could be accomodiated in
   *                   visibleHeight pixels.
   */
  private areAllItemsVisible_(visibleHeight: number): boolean {
    if (!this.dataModel || this.dataModel.length === 0) {
      return true;
    }
    return this.getItemTop(this.dataModel.length) <= visibleHeight;
  }

  /**
   * Creates a new list item.
   * @param dataItem The value to use for the item.
   * @return The newly created list item.
   */
  override createItem(dataItem: unknown): ListItem {
    return this.table_!.getRenderFunction().call(
               null, dataItem, this.table_!) as ListItem;
  }

  /**
   * Determines whether a full redraw is required.
   */
  private needsFullRedraw_(): boolean {
    const cm = this.table_!.columnModel;
    const row = this.firstElementChild as HTMLElement;
    // If the number of columns in the model has changed, a full redraw is
    // needed.
    if (row.children.length !== cm.size) {
      return true;
    }
    // If the column visibility has changed, a full redraw is required.
    for (let i = 0; i < cm.size; ++i) {
      const child = row.children[i]! as HTMLElement;
      if (cm.isVisible(i) === child.hidden) {
        return true;
      }
    }
    return false;
  }

  /**
   * The table associated with the list.
   */
  get table(): Table|null {
    return this.table_;
  }

  set table(value: Table) {
    jsSetter(this, 'table', value);
  }
}
