// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import {ProgressCenterItem, ProgressItemState} from '../../common/js/progress_center_common.js';
import {ProgressCenter} from '../../externs/background/progress_center.js';


/**
 * Mock implementation of {ProgressCenter} for tests.
 * @implements {ProgressCenter}
 * @final
 */
export class MockProgressCenter {
  constructor() {
    /**
     * Items stored in the progress center.
     * @const @type {!Record<string, ProgressCenterItem>}
     */
    this.items = {};
  }

  /**
   * Stores an item to the progress center.
   * @param {ProgressCenterItem} item Progress center item to be stored.
   */
  updateItem(item) {
    // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
    // expression of type 'string' can't be used to index type '{}'.
    this.items[item.id] = item;
  }

  /**
   * Obtains an item stored in the progress center.
   * @param {string} id ID spcifying the progress item.
   */
  getItemById(id) {
    return this.items[id];
  }

  requestCancel() {}
  addPanel() {}
  removePanel() {}
  neverNotifyCompleted() {}

  /**
   * Returns the number of unique keys in |this.items|.
   * @return {number}
   */
  getItemCount() {
    const array = Object.keys(
        /** @type {!Object} */ (this.items));
    return array.length;
  }

  /**
   * Returns the items that have a given state.
   * @param {ProgressItemState} state State to filter by.
   * @returns {!Array<ProgressCenterItem>}
   */
  getItemsByState(state) {
    return Object.values(this.items).filter(item => item.state == state);
  }
}
