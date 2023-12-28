// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dispatchPropertyChange} from 'chrome://resources/ash/common/cr_deprecated.js';
import {assert} from 'chrome://resources/js/assert.js';

import {type CustomEventMap, FilesEventTarget} from '../../../common/js/files_event_target.js';

export type SelectionChangeEvent =
    CustomEvent<{changes: Array<{index: number, selected: boolean}>}>;

interface ListSelectionModelEventMap extends CustomEventMap {
  'change': SelectionChangeEvent;
}

/**
 * Creates a new selection model that is to be used with lists.
 *
 */
export class ListSelectionModel extends
    FilesEventTarget<ListSelectionModelEventMap> {
  private length_: number;

  // Using a object/record and rely on the ascending order returned by iterating
  // over its keys with `Object.keys()`.
  private selectedIndexes_: Record<number, number> = {};

  // True if any item could be lead or anchor. False if only selected ones.
  protected independentLeadItem: boolean = false;

  private leadIndex_: number = -1;
  private oldLeadIndex_: number|null = null;
  private anchorIndex_: number = -1;
  private oldAnchorIndex_: number|null = null;
  private changeCount_: number|null = null;
  private changedIndexes_: null|Record<number, boolean> = null;

  /**
   * @param length The number items in the selection.
   */
  constructor(length?: number) {
    super();
    this.length_ = length ?? 0;
  }

  /**
   * The number of items in the model.
   */
  get length() {
    return this.length_;
  }

  /**
   * The selected indexes.
   * Setter also changes lead and anchor indexes if value list is nonempty.
   */
  get selectedIndexes(): number[] {
    return Object.keys(this.selectedIndexes_).map(Number);
  }

  set selectedIndexes(selectedIndexes) {
    this.beginChange();
    assert(this.changedIndexes_);
    const unselected: Record<number, boolean> = {};
    for (const index in this.selectedIndexes_) {
      unselected[index] = true;
    }

    for (let i = 0; i < selectedIndexes.length; i++) {
      const index = selectedIndexes[i]!;
      if (index in this.selectedIndexes_) {
        delete unselected[index];
      } else {
        this.selectedIndexes_[index] = index;
        // Mark the index as changed. If previously marked, then unmark,
        // since it just got reverted to the original state.
        if (index in this.changedIndexes_) {
          delete this.changedIndexes_[index];
        } else {
          this.changedIndexes_[index] = true;
        }
      }
    }

    for (const i of Object.keys(unselected)) {
      const index = Number(i);
      delete this.selectedIndexes_[index];
      // Mark the index as changed. If previously marked, then unmark,
      // since it just got reverted to the original state.
      if (index in this.changedIndexes_!) {
        delete this.changedIndexes_![index];
      } else {
        this.changedIndexes_![index] = false;
      }
    }

    if (selectedIndexes.length) {
      this.leadIndex = this.anchorIndex = selectedIndexes[0]!;
    } else {
      this.leadIndex = this.anchorIndex = -1;
    }
    this.endChange();
  }

  /**
   * Convenience getter which returns the first selected index.
   * Setter also changes lead and anchor indexes if value is nonnegative.
   */
  get selectedIndex() {
    for (const i in this.selectedIndexes_) {
      return Number(i);
    }
    return -1;
  }

  set selectedIndex(selectedIndex) {
    this.selectedIndexes = selectedIndex !== -1 ? [selectedIndex] : [];
  }

  /**
   * Returns the nearest selected index or -1 if no item selected.
   * @param index The origin index.
   */
  private getNearestSelectedIndex_(index: number): number {
    if (index === -1) {
      // If no index is provided, pick the first selected index if there is
      // one.
      if (this.selectedIndexes.length) {
        return this.selectedIndexes[0]!;
      }
      return -1;
    }

    let result = Infinity;
    for (const j in this.selectedIndexes_) {
      const i = Number(j);
      if (Math.abs(i - index) < Math.abs(result - index)) {
        result = i;
      }
    }
    return result < this.length ? Number(result) : -1;
  }

  /**
   * Selects a range of indexes, starting with `start` and ends with `end`.
   * @param start The first index to select.
   * @param end The last index to select.
   */
  selectRange(start: number, end: number) {
    // Swap if starts comes after end.
    if (start > end) {
      const tmp = start;
      start = end;
      end = tmp;
    }

    this.beginChange();

    for (let index = start; index !== end; index++) {
      this.setIndexSelected(index, true);
    }
    this.setIndexSelected(end, true);

    this.endChange();
  }

  /**
   * Selects all indexes.
   */
  selectAll() {
    if (this.length === 0) {
      return;
    }

    this.selectRange(0, this.length - 1);
  }

  /**
   * Clears the selection
   */
  clear() {
    this.beginChange();
    this.length_ = 0;
    this.anchorIndex = this.leadIndex = -1;
    this.unselectAll();
    this.endChange();
  }

  /**
   * Unselects all selected items.
   */
  unselectAll() {
    this.beginChange();
    for (const i in this.selectedIndexes_) {
      this.setIndexSelected(+i, false);
    }
    this.endChange();
  }

  /**
   * Sets the selected state for an index.
   * @param index The index to set the selected state for.
   * @param b Whether to select the index or not.
   */
  setIndexSelected(index: number, b: boolean) {
    const oldSelected = index in this.selectedIndexes_;
    if (oldSelected === b) {
      return;
    }

    if (b) {
      this.selectedIndexes_[index] = index;
    } else {
      delete this.selectedIndexes_[index];
    }

    this.beginChange();

    this.changedIndexes_![index] = b;

    // End change dispatches an event which in turn may update the view.
    this.endChange();
  }

  /**
   * Whether a given index is selected or not.
   * @param index The index to check.
   * @return Whether an index is selected.
   */
  getIndexSelected(index: number): boolean {
    return index in this.selectedIndexes_;
  }

  /**
   * This is used to begin batching changes. Call {@code endChange} when you
   * are done making changes.
   */
  beginChange() {
    if (!this.changeCount_) {
      this.changeCount_ = 0;
      this.changedIndexes_ = {};
      this.oldLeadIndex_ = this.leadIndex_;
      this.oldAnchorIndex_ = this.anchorIndex_;
    }
    this.changeCount_++;
  }

  /**
   * Call this after changes are done and it will dispatch a change event if
   * any changes were actually done.
   */
  endChange() {
    this.changeCount_!--;
    if (!this.changeCount_) {
      // Calls delayed |dispatchPropertyChange|s, only when |leadIndex| or
      // |anchorIndex| has been actually changed in the batch.
      this.leadIndex_ = this.adjustIndex_(this.leadIndex_);
      if (this.leadIndex_ !== this.oldLeadIndex_) {
        dispatchPropertyChange(
            this, 'leadIndex', this.leadIndex_, this.oldLeadIndex_);
      }
      this.oldLeadIndex_ = null;

      this.anchorIndex_ = this.adjustIndex_(this.anchorIndex_);
      if (this.anchorIndex_ !== this.oldAnchorIndex_) {
        dispatchPropertyChange(
            this, 'anchorIndex', this.anchorIndex_, this.oldAnchorIndex_);
      }
      this.oldAnchorIndex_ = null;

      const indexes = Object.keys(this.changedIndexes_!);
      if (indexes.length) {
        const e = new CustomEvent('change', {
          detail: {
            changes: indexes.map((index: string) => {
              return {
                index: Number(index),
                selected: this.changedIndexes_![Number(index)]!,
              };
            }),
          },
        });
        this.dispatchEvent(e);
      }
      this.changedIndexes_ = {};
    }
  }

  /**
   * The leadIndex is used with multiple selection and it is the index that
   * the user is moving using the arrow keys.
   */
  get leadIndex() {
    return this.leadIndex_;
  }

  set leadIndex(leadIndex) {
    const oldValue = this.leadIndex_;
    const newValue = this.adjustIndex_(leadIndex);
    this.leadIndex_ = newValue;
    // Delays the call of dispatchPropertyChange if batch is running.
    if (!this.changeCount_ && newValue !== oldValue) {
      dispatchPropertyChange(this, 'leadIndex', newValue, oldValue);
    }
  }

  /**
   * The anchorIndex is used with multiple selection.
   */
  get anchorIndex() {
    return this.anchorIndex_;
  }

  set anchorIndex(anchorIndex) {
    const oldValue = this.anchorIndex_;
    const newValue = this.adjustIndex_(anchorIndex);
    this.anchorIndex_ = newValue;
    // Delays the call of dispatchPropertyChange if batch is running.
    if (!this.changeCount_ && newValue !== oldValue) {
      dispatchPropertyChange(this, 'anchorIndex', newValue, oldValue);
    }
  }

  /**
   * Helper method that adjustes a value before assigning it to leadIndex or
   * anchorIndex.
   * @param index New value for leadIndex or anchorIndex.
   * @return Corrected value.
   */
  private adjustIndex_(index: number): number {
    index = Math.max(-1, Math.min(this.length_ - 1, index));
    // On Mac and ChromeOS lead and anchor items are forced to be among
    // selected items. This rule is not enforces until end of batch update.
    if (!this.changeCount_ && !this.independentLeadItem &&
        !this.getIndexSelected(index)) {
      const index2 = this.getNearestSelectedIndex_(index);
      index = index2;
    }
    return index;
  }

  /**
   * Whether the selection model supports multiple selected items.
   */
  get multiple() {
    return true;
  }

  /**
   * Adjusts the selection after reordering of items in the table.
   * @param permutation The reordering permutation.
   */
  adjustToReordering(permutation: number[]) {
    this.beginChange();
    const oldLeadIndex = this.leadIndex;
    const oldAnchorIndex = this.anchorIndex;
    const oldSelectedItemsCount = this.selectedIndexes.length;

    this.selectedIndexes = this.selectedIndexes
                               .map((oldIndex) => {
                                 return permutation[oldIndex]!;
                               })
                               .filter((index) => {
                                 return index !== -1;
                               });

    // Will be adjusted in endChange.
    if (oldLeadIndex !== -1) {
      this.leadIndex = permutation[oldLeadIndex]!;
    }
    if (oldAnchorIndex !== -1) {
      this.anchorIndex = permutation[oldAnchorIndex]!;
    }

    if (oldSelectedItemsCount && !this.selectedIndexes.length && this.length_ &&
        oldLeadIndex !== -1) {
      // All selected items are deleted. We move selection to next item of
      // last selected item, following it to its new position.
      let newSelectedIndex = Math.min(oldLeadIndex, this.length_ - 1);
      for (let i = oldLeadIndex + 1; i < permutation.length; ++i) {
        if (permutation[i] !== -1) {
          newSelectedIndex = permutation[i]!;
          break;
        }
      }
      this.selectedIndexes = [newSelectedIndex];
    }

    this.endChange();
  }

  /**
   * Adjusts selection model length.
   * @param length New selection model length.
   */
  adjustLength(length: number) {
    this.length_ = length;
  }
}
