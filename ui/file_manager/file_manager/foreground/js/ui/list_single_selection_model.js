// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dispatchPropertyChange} from 'chrome://resources/ash/common/cr_deprecated.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

/**
 * Creates a new selection model that is to be used with lists. This only
 * allows a single index to be selected.
 */
export class ListSingleSelectionModel extends EventTarget {
  /**
   * @param {number=} opt_length The number items in the selection.
   */
  constructor(opt_length) {
    super();
    this.length_ = opt_length || 0;
    this.selectedIndex = -1;

    // True if any item could be lead or anchor. False if only selected ones.
    this.independentLeadItem_ = false;

    /** @private {number} */
    this.leadIndex_ = -1;

    /** @private {?number} */
    this.selectedIndex_;

    /** @private {?number} */
    this.selectedIndexBefore_;

    /** @private {?number} */
    this.changeCount_;
  }

  /**
   * The number of items in the model.
   * @type {number}
   */
  get length() {
    return this.length_;
  }

  /**
   * @type {!Array} The selected indexes.
   */
  get selectedIndexes() {
    const i = this.selectedIndex;
    return i !== -1 ? [this.selectedIndex] : [];
  }

  set selectedIndexes(indexes) {
    this.selectedIndex = indexes.length ? indexes[0] : -1;
  }

  /**
   * Convenience getter which returns the first selected index.
   * Setter also changes lead and anchor indexes if value is nonegative.
   * @type {number}
   */
  get selectedIndex() {
    return this.selectedIndex_;
  }

  set selectedIndex(selectedIndex) {
    const oldSelectedIndex = this.selectedIndex;
    const i = Math.max(-1, Math.min(this.length_ - 1, selectedIndex));

    if (i !== oldSelectedIndex) {
      this.beginChange();
      this.selectedIndex_ = i;
      this.leadIndex = i >= 0 ? i : this.leadIndex;
      this.endChange();
    }
  }

  /**
   * Selects a range of indexes, starting with {@code start} and ends with
   * {@code end}.
   * @param {number} start The first index to select.
   * @param {number} end The last index to select.
   */
  selectRange(start, end) {
    // Only select first index.
    this.selectedIndex = Math.min(start, end);
  }

  /**
   * Selects all indexes.
   */
  selectAll() {
    // Select all is not allowed on a single selection model
  }

  /**
   * Clears the selection
   */
  clear() {
    this.beginChange();
    this.length_ = 0;
    this.selectedIndex = this.anchorIndex = this.leadIndex = -1;
    this.endChange();
  }

  /**
   * Unselects all selected items.
   */
  unselectAll() {
    this.selectedIndex = -1;
  }

  /**
   * Sets the selected state for an index.
   * @param {number} index The index to set the selected state for.
   * @param {boolean} b Whether to select the index or not.
   */
  setIndexSelected(index, b) {
    // Only allow selection
    const oldSelected = index === this.selectedIndex_;
    if (oldSelected === b) {
      return;
    }

    if (b) {
      this.selectedIndex = index;
    } else if (index === this.selectedIndex_) {
      this.selectedIndex = -1;
    }
  }

  /**
   * Whether a given index is selected or not.
   * @param {number} index The index to check.
   * @return {boolean} Whether an index is selected.
   */
  getIndexSelected(index) {
    return index === this.selectedIndex_;
  }

  /**
   * This is used to begin batching changes. Call {@code endChange} when you
   * are done making changes.
   */
  beginChange() {
    if (!this.changeCount_) {
      this.changeCount_ = 0;
      this.selectedIndexBefore_ = this.selectedIndex_;
    }
    this.changeCount_++;
  }

  /**
   * Call this after changes are done and it will dispatch a change event if
   * any changes were actually done.
   */
  endChange() {
    this.changeCount_--;
    if (!this.changeCount_) {
      if (this.selectedIndexBefore_ !== this.selectedIndex_) {
        const beforeChange = this.createChangeEvent('beforeChange');
        if (this.dispatchEvent(beforeChange)) {
          this.dispatchEvent(this.createChangeEvent('change'));
        } else {
          this.selectedIndex_ = this.selectedIndexBefore_;
        }
      }
    }
  }

  /**
   * Creates event with specified name and fills its {changes} property.
   * @param {string} eventName Event name.
   */
  createChangeEvent(eventName) {
    const e = new Event(eventName);
    const indexes = [this.selectedIndexBefore_, this.selectedIndex_];
    e.changes =
        indexes
            .filter(function(index) {
              return index !== -1;
            })
            .map(function(index) {
              return {index: index, selected: index === this.selectedIndex_};
            }, this);

    return e;
  }

  /**
   * The leadIndex is used with multiple selection and it is the index that
   * the user is moving using the arrow keys.
   * @type {number}
   */
  get leadIndex() {
    return this.leadIndex_;
  }

  set leadIndex(leadIndex) {
    const li = this.adjustIndex_(leadIndex);
    if (li !== this.leadIndex_) {
      const oldLeadIndex = this.leadIndex_;
      this.leadIndex_ = li;
      dispatchPropertyChange(this, 'leadIndex', li, oldLeadIndex);
      dispatchPropertyChange(this, 'anchorIndex', li, oldLeadIndex);
    }
  }

  adjustIndex_(index) {
    index = Math.max(-1, Math.min(this.length_ - 1, index));
    if (!this.independentLeadItem_) {
      index = this.selectedIndex;
    }
    return index;
  }

  /**
   * The anchorIndex is used with multiple selection.
   * @type {number}
   */
  get anchorIndex() {
    return this.leadIndex;
  }

  set anchorIndex(anchorIndex) {
    this.leadIndex = anchorIndex;
  }

  /**
   * Whether the selection model supports multiple selected items.
   * @type {boolean}
   */
  get multiple() {
    return false;
  }

  /**
   * Adjusts the selection after reordering of items in the table.
   * @param {!Array<number>} permutation The reordering permutation.
   */
  adjustToReordering(permutation) {
    if (this.leadIndex !== -1) {
      this.leadIndex = permutation[this.leadIndex];
    }

    const oldSelectedIndex = this.selectedIndex;
    if (oldSelectedIndex !== -1) {
      this.selectedIndex = permutation[oldSelectedIndex];
    }
  }

  /**
   * Adjusts selection model length.
   * @param {number} length New selection model length.
   */
  adjustLength(length) {
    this.length_ = length;
  }
}
