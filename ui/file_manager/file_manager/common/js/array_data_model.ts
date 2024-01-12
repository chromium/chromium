// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This is a data model representin
 */

import {assert} from 'chrome://resources/js/assert.js';

import {type CustomEventMap, FilesEventTarget} from './files_event_target.js';

export type SpliceEvent = CustomEvent<{
  removed: any[],
  added: any[],
  index?: number,
}>;

export type ChangeEvent = CustomEvent<{
  index: number,
}>;

export type PermutationEvent = CustomEvent<{
  permutation: number[],
  newLength: number,
}>;

interface ArrayDataModelEventMap extends CustomEventMap {
  'splice': SpliceEvent;
  'change': ChangeEvent;
  'permuted': PermutationEvent;
}

type CompareFunction<T> = (a: T, b: T) => number;

interface SortStatus {
  field: string|null;
  direction: string|null;
}

/**
 * Default compare function.
 */
function defaultValuesCompareFunction(a: number, b: number) {
  // We could insert i18n comparisons here.
  if (a < b) {
    return -1;
  }
  if (a > b) {
    return 1;
  }
  return 0;
}

/**
 * A data model that wraps a simple array and supports sorting by storing
 * initial indexes of elements for each position in sorted array.
 */
export class ArrayDataModel<T = any> extends
    FilesEventTarget<ArrayDataModelEventMap> {
  protected indexes_: number[] = [];
  protected compareFunctions_: Record<string, CompareFunction<T>> = {};
  private sortStatus_: SortStatus = {field: null, direction: null};

  /**
   * @param array The underlying array.
   */
  constructor(protected array_: T[]) {
    super();

    for (let i = 0; i < this.array_.length; i++) {
      this.indexes_.push(i);
    }
  }

  /**
   * The length of the data model.
   */
  get length() {
    return this.array_.length;
  }

  /**
   * Returns the item at the given index.
   * This implementation returns the item at the given index in the sorted
   * array.
   * @param index The index of the element to get.
   * @return The element at the given index.
   */
  item(index: number): T|undefined {
    if (index >= 0 && index < this.length) {
      return this.array_[this.indexes_[index]!];
    }
    return undefined;
  }

  /**
   * Returns compare function set for given field.
   * @param field The field to get compare function for.
   * @return Compare function set for given field.
   */
  compareFunction(field: string): CompareFunction<T>|undefined {
    return this.compareFunctions_[field];
  }

  /**
   * Sets compare function for given field.
   * @param field The field to set compare function.
   * @param compareFunction Compare function to set for given field.
   */
  setCompareFunction(field: string, compareFunction: CompareFunction<T>) {
    if (!this.compareFunctions_) {
      this.compareFunctions_ = {};
    }
    this.compareFunctions_[field] = compareFunction;
  }

  /**
   * Returns true if the field has a compare function.
   * @param field The field to check.
   * @return True if the field is sortable.
   */
  isSortable(field: string): boolean {
    return this.compareFunctions_ && field in this.compareFunctions_;
  }

  /**
   * Returns current sort status.
   * @return Current sort status.
   */
  get sortStatus(): {field: string|null, direction: string|null} {
    if (this.sortStatus_) {
      return this.createSortStatus(
          this.sortStatus_.field, this.sortStatus_.direction);
    } else {
      return this.createSortStatus(null, null);
    }
  }

  /**
   * Returns the first matching item.
   * @param item The item to find.
   * @param fromIndex If provided, then the searching start at the fromIndex.
   * @return The index of the first found element or -1 if not found.
   */
  indexOf(item: T, fromIndex?: number): number {
    for (let i = fromIndex || 0; i < this.indexes_.length; i++) {
      if (item === this.item(i)) {
        return i;
      }
    }
    return -1;
  }

  /**
   * Returns an array of elements in a selected range.
   * @param from The starting index of the selected range.
   * @param to The ending index of selected range.
   * @return An array of elements in the selected range.
   */
  slice(from?: number, to?: number): T[] {
    const arr = this.array_;
    return this.indexes_.slice(from, to).map((index) => {
      return arr[index]!;
    });
  }

  /**
   * This removes and adds items to the model.
   * This dispatches a splice event.
   * This implementation runs sort after splice and creates permutation for
   * the whole change.
   * @param index The index of the item to update.
   * @param deleteCount The number of items to remove.
   * @param itemsToAdd The items to add.
   * @return An array with the removed items.
   */
  splice(index: number, deleteCount: number, ...itemsToAdd: T[]): T[] {
    const addCount = itemsToAdd.length;
    const newIndexes = [];
    const deletePermutation = [];
    const deletedItems: T[] = [];
    const newArray = [];
    index = Math.min(index, this.indexes_.length);
    deleteCount = Math.min(deleteCount, this.indexes_.length - index);
    // Copy items before the insertion point.
    let i;
    for (i = 0; i < index; i++) {
      newIndexes.push(newArray.length);
      deletePermutation.push(i);
      newArray.push(this.array_[this.indexes_[i]!]);
    }
    // Delete items.
    for (; i < index + deleteCount; i++) {
      deletePermutation.push(-1);
      deletedItems.push(this.array_[this.indexes_[i]!]!);
    }
    // Insert new items instead deleted ones.
    for (let j = 0; j < addCount; j++) {
      newIndexes.push(newArray.length);
      newArray.push(arguments[j + 2]);
    }
    // Copy items after the insertion point.
    for (; i < this.indexes_.length; i++) {
      newIndexes.push(newArray.length);
      deletePermutation.push(i - deleteCount + addCount);
      newArray.push(this.array_[this.indexes_[i]!]);
    }

    this.indexes_ = newIndexes;

    this.array_ = newArray;

    // TODO(arv): Maybe unify splice and change events?
    const spliceEventDetail: SpliceEvent['detail'] = {
      removed: deletedItems,
      added: itemsToAdd,
    };

    const status = this.sortStatus;
    // if sortStatus.field is null, this restores original order.
    const sortPermutation =
        this.doSort_(this.sortStatus.field, this.sortStatus.direction);
    if (sortPermutation) {
      const splicePermutation = deletePermutation.map((element) => {
        return element !== -1 ? sortPermutation[element]! : -1;
      });
      this.dispatchPermutedEvent_(splicePermutation);
      spliceEventDetail.index = sortPermutation[index];
    } else {
      this.dispatchPermutedEvent_(deletePermutation);
      spliceEventDetail.index = index;
    }

    this.dispatchEvent(new CustomEvent('splice', {detail: spliceEventDetail}));

    // Still need to finish the sorting above (including events), so
    // list will not go to inconsistent state.
    if (status.field) {
      this.delayedSort_(status.field, status.direction);
    }

    return deletedItems;
  }

  /**
   * Appends items to the end of the model.
   *
   * This dispatches a splice event.
   *
   * @param itemsToAppend The items to append.
   * @return The new length of the model.
   */
  push(...itemsToAppend: T[]): number {
    this.splice(this.length, 0, ...itemsToAppend);
    return this.length;
  }

  /**
   * Updates the existing item with the new item.
   *
   * The existing item and the new item are regarded as the same item and the
   * permutation tracks these indexes.
   *
   * @param oldItem Old item that is contained in the model. If the item is not
   *     found in the model, the method call is just ignored.
   * @param newItem New item.
   */
  replaceItem(oldItem: T, newItem: T) {
    const index = this.indexOf(oldItem);
    if (index < 0) {
      return;
    }
    this.array_[this.indexes_[index]!] = newItem;
    this.updateIndex(index);
  }

  /**
   * Use this to update a given item in the array. This does not remove and
   * reinsert a new item.
   * This dispatches a change event.
   * This runs sort after updating.
   * @param index The index of the item to update.
   */
  updateIndex(index: number) {
    this.updateIndexes([index]);
  }

  /**
   * Notifies of update of the items in the array. This does not remove and
   * reinsert new items.
   * This dispatches one or more change events.
   * This runs sort after updating.
   * @param indexes The index list of items to update.
   */
  updateIndexes(indexes: number[]) {
    indexes.forEach(index => {
      assert(index >= 0 && index < this.length, 'Invalid index');
    });

    for (const index of indexes) {
      const e = new CustomEvent('change', {detail: {index}});
      this.dispatchEvent(e);
    }

    if (!this.sortStatus.field) {
      return;
    }

    const status = this.sortStatus;
    const sortPermutation =
        this.doSort_(this.sortStatus.field, this.sortStatus.direction);
    if (sortPermutation) {
      this.dispatchPermutedEvent_(sortPermutation);
    }
    // Still need to finish the sorting above (including events), so
    // list will not go to inconsistent state.
    this.delayedSort_(status.field, status.direction);
  }

  /**
   * Creates sort status with given field and direction.
   * @param field Sort field.
   * @param direction Sort direction.
   * @return Created sort status.
   */
  createSortStatus(field: string|null, direction: string|null): SortStatus {
    return {field: field, direction: direction};
  }

  /**
   * Sorts data model according to given field and direction and dispatches
   * sorted event with delay. If no need to delay, use sort() instead.
   * @param field Sort field.
   * @param direction Sort direction.
   */
  private delayedSort_(field: string|null, direction: string|null) {
    setTimeout(() => {
      // If the sort status has been changed, sorting has already done
      // on the change event.
      if (field === this.sortStatus.field &&
          direction === this.sortStatus.direction) {
        this.sort(field, direction);
      }
    }, 0);
  }

  /**
   * Sorts data model according to given field and direction and dispatches
   * sorted event.
   * @param field Sort field.
   * @param direction Sort direction.
   */
  sort(field: string|null, direction: string|null) {
    const sortPermutation = this.doSort_(field, direction);
    if (sortPermutation) {
      this.dispatchPermutedEvent_(sortPermutation);
    }
    this.dispatchSortEvent_();
  }

  /**
   * Sorts data model according to given field and direction.
   * @param field Sort field.
   * @param direction Sort direction.
   */
  private doSort_(field: string|null, direction: string|null) {
    const compareFunction = this.sortFunction_(field, direction);
    const positions: number[] = [];
    for (let i = 0; i < this.length; i++) {
      positions[this.indexes_[i]!] = i;
    }
    const sorted = this.indexes_.every((element, index, array) => {
      return index === 0 || compareFunction(element, array[index - 1]!) >= 0;
    });
    if (!sorted) {
      this.indexes_.sort(compareFunction);
    }
    this.sortStatus_ = this.createSortStatus(field, direction);
    const sortPermutation: number[] = [];
    let changed = false;
    for (let i = 0; i < this.length; i++) {
      if (positions[this.indexes_[i]!] !== i) {
        changed = true;
      }
      sortPermutation[positions[this.indexes_[i]!]!] = i;
    }
    if (changed) {
      return sortPermutation;
    }
    return null;
  }

  private dispatchSortEvent_() {
    const e = new Event('sorted');
    this.dispatchEvent(e);
  }

  protected dispatchPermutedEvent_(permutation: number[]) {
    const e = new CustomEvent(
        'permuted', {detail: {permutation, newLength: this.length}});
    this.dispatchEvent(e);
  }

  /**
   * Creates compare function for the field.
   * Returns the function set as sortFunction for given field or default compare
   * function
   * @param field Sort field.
   * @return Compare function.
   */
  private createCompareFunction_(field: string): CompareFunction<T> {
    const compareFunction =
        this.compareFunctions_ ? this.compareFunctions_[field] : null;
    if (compareFunction) {
      return compareFunction;
    } else {
      return function(a: any, b: any) {
        return defaultValuesCompareFunction(a[field], b[field]);
      };
    }
  }

  /**
   * Creates compare function for given field and direction.
   * @param field Sort field.
   * @param direction Sort direction.
   */
  private sortFunction_(field: string|null, direction: string|null) {
    let compareFunction: CompareFunction<T>|null = null;
    if (field !== null) {
      compareFunction = this.createCompareFunction_(field);
    }
    const dirMultiplier = direction === 'desc' ? -1 : 1;

    return (index1: number, index2: number) => {
      const item1 = this.array_[index1]!;
      const item2 = this.array_[index2]!;

      let compareResult = 0;
      if (typeof (compareFunction) === 'function') {
        compareResult = compareFunction(item1, item2);
      }
      if (compareResult !== 0) {
        return dirMultiplier * compareResult;
      }
      return dirMultiplier * defaultValuesCompareFunction(index1, index2);
    };
  }
}
