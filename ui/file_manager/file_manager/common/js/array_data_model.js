// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This is a data model representin
 */

import {assert} from 'chrome://resources/ash/common/assert.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

/**
 * A data model that wraps a simple array and supports sorting by storing
 * initial indexes of elements for each position in sorted array.
 */
export class ArrayDataModel extends EventTarget {
  /**
   * @param {!Array<*>} array The underlying array.
   */
  constructor(array) {
    super();
    this.array_ = array;
    this.indexes_ = [];
    this.compareFunctions_ = {};

    /** @type {null|{field: string|null, direction: string|null}} */
    this.sortStatus_;

    for (let i = 0; i < array.length; i++) {
      this.indexes_.push(i);
    }
  }

  /**
   * The length of the data model.
   * @type {number}
   */
  get length() {
    return this.array_.length;
  }

  /**
   * Returns the item at the given index.
   * This implementation returns the item at the given index in the sorted
   * array.
   * @param {number} index The index of the element to get.
   * @return {*} The element at the given index.
   */
  item(index) {
    if (index >= 0 && index < this.length) {
      // @ts-ignore: error TS2538: Type 'undefined' cannot be used as an index
      // type.
      return this.array_[this.indexes_[index]];
    }
    return undefined;
  }

  /**
   * Returns compare function set for given field.
   * @param {string} field The field to get compare function for.
   * @return {function(*, *): number} Compare function set for given field.
   */
  compareFunction(field) {
    // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
    // expression of type 'string' can't be used to index type '{}'.
    return this.compareFunctions_[field];
  }

  /**
   * Sets compare function for given field.
   * @param {string} field The field to set compare function.
   * @param {function(*, *): number} compareFunction Compare function to set
   *     for given field.
   */
  setCompareFunction(field, compareFunction) {
    if (!this.compareFunctions_) {
      this.compareFunctions_ = {};
    }
    // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
    // expression of type 'string' can't be used to index type '{}'.
    this.compareFunctions_[field] = compareFunction;
  }

  /**
   * Returns true if the field has a compare function.
   * @param {string} field The field to check.
   * @return {boolean} True if the field is sortable.
   */
  isSortable(field) {
    return this.compareFunctions_ && field in this.compareFunctions_;
  }

  /**
   * Returns current sort status.
   * @return {{field: string|null, direction: string|null}} Current sort status.
   */
  get sortStatus() {
    if (this.sortStatus_) {
      return this.createSortStatus(
          this.sortStatus_.field, this.sortStatus_.direction);
    } else {
      return this.createSortStatus(null, null);
    }
  }

  /**
   * Returns the first matching item.
   * @param {*} item The item to find.
   * @param {number=} opt_fromIndex If provided, then the searching start at
   *     the {@code opt_fromIndex}.
   * @return {number} The index of the first found element or -1 if not found.
   */
  indexOf(item, opt_fromIndex) {
    for (let i = opt_fromIndex || 0; i < this.indexes_.length; i++) {
      if (item === this.item(i)) {
        return i;
      }
    }
    return -1;
  }

  /**
   * Returns an array of elements in a selected range.
   * @param {number=} opt_from The starting index of the selected range.
   * @param {number=} opt_to The ending index of selected range.
   * @return {!Array<*>} An array of elements in the selected range.
   */
  slice(opt_from, opt_to) {
    const arr = this.array_;
    return this.indexes_.slice(opt_from, opt_to).map(function(index) {
      return arr[index];
    });
  }

  /**
   * This removes and adds items to the model.
   * This dispatches a splice event.
   * This implementation runs sort after splice and creates permutation for
   * the whole change.
   * @param {number} index The index of the item to update.
   * @param {number} deleteCount The number of items to remove.
   * @param {...*} var_args The items to add.
   * @return {!Array<*>} An array with the removed items.
   */
  // @ts-ignore: error TS6133: 'var_args' is declared but its value is never
  // read.
  splice(index, deleteCount, ...var_args) {
    const addCount = arguments.length - 2;
    const newIndexes = [];
    const deletePermutation = [];
    const deletedItems = [];
    const newArray = [];
    index = Math.min(index, this.indexes_.length);
    deleteCount = Math.min(deleteCount, this.indexes_.length - index);
    // Copy items before the insertion point.
    let i;
    for (i = 0; i < index; i++) {
      newIndexes.push(newArray.length);
      deletePermutation.push(i);
      // @ts-ignore: error TS2538: Type 'undefined' cannot be used as an index
      // type.
      newArray.push(this.array_[this.indexes_[i]]);
    }
    // Delete items.
    for (; i < index + deleteCount; i++) {
      deletePermutation.push(-1);
      // @ts-ignore: error TS2538: Type 'undefined' cannot be used as an index
      // type.
      deletedItems.push(this.array_[this.indexes_[i]]);
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
      // @ts-ignore: error TS2538: Type 'undefined' cannot be used as an index
      // type.
      newArray.push(this.array_[this.indexes_[i]]);
    }

    this.indexes_ = newIndexes;

    this.array_ = newArray;

    // TODO(arv): Maybe unify splice and change events?
    const
        spliceEventDetail = /**
                                @type {import('../../definitions/array_data_model_events.js').ArrayDataModelSpliceEvent['detail']}
                                  */
        ({
          removed: deletedItems,
          added: Array.prototype.slice.call(arguments, 2),
        });

    const status = this.sortStatus;
    // if sortStatus.field is null, this restores original order.
    const sortPermutation =
        // @ts-ignore: error TS2339: Property 'direction' does not exist on type
        // 'Object'.
        this.doSort_(this.sortStatus.field, this.sortStatus.direction);
    if (sortPermutation) {
      const splicePermutation = deletePermutation.map(function(element) {
        return element !== -1 ? sortPermutation[element] : -1;
      });
      this.dispatchPermutedEvent_(splicePermutation);
      spliceEventDetail.index = sortPermutation[index];
    } else {
      this.dispatchPermutedEvent_(deletePermutation);
      spliceEventDetail.index = index;
    }

    this.dispatchEvent(new CustomEvent('splice', {detail: spliceEventDetail}));

    // If real sorting is needed, we should first call prepareSort (data may
    // change), and then sort again.
    // Still need to finish the sorting above (including events), so
    // list will not go to inconsistent state.
    // @ts-ignore: error TS2339: Property 'field' does not exist on type
    // 'Object'.
    if (status.field) {
      // @ts-ignore: error TS2339: Property 'direction' does not exist on type
      // 'Object'.
      this.delayedSort_(status.field, status.direction);
    }

    return deletedItems;
  }

  /**
   * Appends items to the end of the model.
   *
   * This dispatches a splice event.
   *
   * @param {...*} var_args The items to append.
   * @return {number} The new length of the model.
   */
  // @ts-ignore: error TS6133: 'var_args' is declared but its value is never
  // read.
  push(...var_args) {
    const args = Array.prototype.slice.call(arguments);
    args.unshift(this.length, 0);
    // @ts-ignore: error TS2345: Argument of type 'any[]' is not assignable to
    // parameter of type '[index: number, deleteCount: number, ...var_args:
    // any[]]'.
    this.splice.apply(this, args);
    return this.length;
  }

  /**
   * Updates the existing item with the new item.
   *
   * The existing item and the new item are regarded as the same item and the
   * permutation tracks these indexes.
   *
   * @param {*} oldItem Old item that is contained in the model. If the item
   *     is not found in the model, the method call is just ignored.
   * @param {*} newItem New item.
   */
  replaceItem(oldItem, newItem) {
    const index = this.indexOf(oldItem);
    if (index < 0) {
      return;
    }
    // @ts-ignore: error TS2538: Type 'undefined' cannot be used as an index
    // type.
    this.array_[this.indexes_[index]] = newItem;
    this.updateIndex(index);
  }

  /**
   * Use this to update a given item in the array. This does not remove and
   * reinsert a new item.
   * This dispatches a change event.
   * This runs sort after updating.
   * @param {number} index The index of the item to update.
   */
  updateIndex(index) {
    this.updateIndexes([index]);
  }

  /**
   * Notifies of update of the items in the array. This does not remove and
   * reinsert new items.
   * This dispatches one or more change events.
   * This runs sort after updating.
   * @param {Array<number>} indexes The index list of items to update.
   */
  updateIndexes(indexes) {
    indexes.forEach(function(index) {
      // @ts-ignore: error TS2683: 'this' implicitly has type 'any' because it
      // does not have a type annotation.
      assert(index >= 0 && index < this.length, 'Invalid index');
    }, this);

    for (let i = 0; i < indexes.length; i++) {
      const e = new CustomEvent('change', {detail: {index: indexes[i]}});
      this.dispatchEvent(e);
    }

    // @ts-ignore: error TS2339: Property 'field' does not exist on type
    // 'Object'.
    if (this.sortStatus.field) {
      const status = this.sortStatus;
      const sortPermutation =
          // @ts-ignore: error TS2339: Property 'direction' does not exist on
          // type 'Object'.
          this.doSort_(this.sortStatus.field, this.sortStatus.direction);
      if (sortPermutation) {
        this.dispatchPermutedEvent_(sortPermutation);
      }
      // We should first call prepareSort (data may change), and then sort.
      // Still need to finish the sorting above (including events), so
      // list will not go to inconsistent state.
      // @ts-ignore: error TS2339: Property 'direction' does not exist on type
      // 'Object'.
      this.delayedSort_(status.field, status.direction);
    }
  }

  /**
   * Creates sort status with given field and direction.
   * @param {?string} field Sort field.
   * @param {?string} direction Sort direction.
   * @return {{field: string|null, direction: string|null}} Created sort status.
   */
  createSortStatus(field, direction) {
    return {field: field, direction: direction};
  }

  /**
   * Called before a sort happens so that you may fetch additional data
   * required for the sort.
   *
   * @param {string} field Sort field.
   * @param {function():void} callback The function to invoke when preparation
   *     is complete.
   */
  // @ts-ignore: error TS6133: 'field' is declared but its value is never read.
  prepareSort(field, callback) {
    callback();
  }

  /**
   * Sorts data model according to given field and direction and dispatches
   * sorted event with delay. If no need to delay, use sort() instead.
   * @param {string} field Sort field.
   * @param {string} direction Sort direction.
   * @private
   */
  delayedSort_(field, direction) {
    const self = this;
    setTimeout(function() {
      // If the sort status has been changed, sorting has already done
      // on the change event.
      // @ts-ignore: error TS2339: Property 'field' does not exist on type
      // 'Object'.
      if (field === self.sortStatus.field &&
          // @ts-ignore: error TS2339: Property 'direction' does not exist on
          // type 'Object'.
          direction === self.sortStatus.direction) {
        self.sort(field, direction);
      }
    }, 0);
  }

  /**
   * Sorts data model according to given field and direction and dispatches
   * sorted event.
   * @param {string} field Sort field.
   * @param {string} direction Sort direction.
   */
  sort(field, direction) {
    const self = this;

    this.prepareSort(field, function() {
      const sortPermutation = self.doSort_(field, direction);
      if (sortPermutation) {
        self.dispatchPermutedEvent_(sortPermutation);
      }
      self.dispatchSortEvent_();
    });
  }

  /**
   * Sorts data model according to given field and direction.
   * @param {string} field Sort field.
   * @param {string} direction Sort direction.
   * @private
   */
  doSort_(field, direction) {
    const compareFunction = this.sortFunction_(field, direction);
    // @ts-ignore: error TS7034: Variable 'positions' implicitly has type
    // 'any[]' in some locations where its type cannot be determined.
    const positions = [];
    for (let i = 0; i < this.length; i++) {
      // @ts-ignore: error TS2538: Type 'undefined' cannot be used as an index
      // type.
      positions[this.indexes_[i]] = i;
    }
    const sorted = this.indexes_.every(function(element, index, array) {
      return index === 0 || compareFunction(element, array[index - 1]) >= 0;
    });
    if (!sorted) {
      this.indexes_.sort(compareFunction);
    }
    this.sortStatus_ = this.createSortStatus(field, direction);
    const sortPermutation = [];
    let changed = false;
    for (let i = 0; i < this.length; i++) {
      // @ts-ignore: error TS2538: Type 'undefined' cannot be used as an index
      // type.
      if (positions[this.indexes_[i]] !== i) {
        changed = true;
      }
      // @ts-ignore: error TS2538: Type 'undefined' cannot be used as an index
      // type.
      sortPermutation[positions[this.indexes_[i]]] = i;
    }
    if (changed) {
      return sortPermutation;
    }
    return null;
  }

  dispatchSortEvent_() {
    const e = new Event('sorted');
    this.dispatchEvent(e);
  }

  // @ts-ignore: error TS7006: Parameter 'permutation' implicitly has an 'any'
  // type.
  dispatchPermutedEvent_(permutation) {
    const e = new Event('permuted');
    // @ts-ignore: error TS2339: Property 'permutation' does not exist on type
    // 'Event'.
    e.permutation = permutation;
    // @ts-ignore: error TS2339: Property 'newLength' does not exist on type
    // 'Event'.
    e.newLength = this.length;
    this.dispatchEvent(e);
  }

  /**
   * Creates compare function for the field.
   * Returns the function set as sortFunction for given field
   * or default compare function
   * @param {string} field Sort field.
   * @return {function(*, *): number} Compare function.
   * @private
   */
  createCompareFunction_(field) {
    const compareFunction =
        // @ts-ignore: error TS7053: Element implicitly has an 'any' type
        // because expression of type 'string' can't be used to index type '{}'.
        this.compareFunctions_ ? this.compareFunctions_[field] : null;
    const defaultValuesCompareFunction = this.defaultValuesCompareFunction;
    if (compareFunction) {
      return compareFunction;
    } else {
      return function(a, b) {
        return defaultValuesCompareFunction.call(null, a[field], b[field]);
      };
    }
  }

  /**
   * Creates compare function for given field and direction.
   * @param {string} field Sort field.
   * @param {string} direction Sort direction.
   * @private
   */
  sortFunction_(field, direction) {
    // @ts-ignore: error TS7034: Variable 'compareFunction' implicitly has type
    // 'any' in some locations where its type cannot be determined.
    let compareFunction = null;
    if (field !== null) {
      compareFunction = this.createCompareFunction_(field);
    }
    const dirMultiplier = direction === 'desc' ? -1 : 1;

    // @ts-ignore: error TS7006: Parameter 'index2' implicitly has an 'any'
    // type.
    return function(index1, index2) {
      // @ts-ignore: error TS2683: 'this' implicitly has type 'any' because it
      // does not have a type annotation.
      const item1 = this.array_[index1];
      // @ts-ignore: error TS2683: 'this' implicitly has type 'any' because it
      // does not have a type annotation.
      const item2 = this.array_[index2];

      let compareResult = 0;
      // @ts-ignore: error TS7005: Variable 'compareFunction' implicitly has an
      // 'any' type.
      if (typeof (compareFunction) === 'function') {
        // @ts-ignore: error TS7005: Variable 'compareFunction' implicitly has
        // an 'any' type.
        compareResult = compareFunction.call(null, item1, item2);
      }
      if (compareResult !== 0) {
        return dirMultiplier * compareResult;
      }
      // @ts-ignore: error TS2683: 'this' implicitly has type 'any' because it
      // does not have a type annotation.
      return dirMultiplier * this.defaultValuesCompareFunction(index1, index2);
    }.bind(this);
  }

  /**
   * Default compare function.
   */
  // @ts-ignore: error TS7006: Parameter 'b' implicitly has an 'any' type.
  defaultValuesCompareFunction(a, b) {
    // We could insert i18n comparisons here.
    if (a < b) {
      return -1;
    }
    if (a > b) {
      return 1;
    }
    return 0;
  }
}
