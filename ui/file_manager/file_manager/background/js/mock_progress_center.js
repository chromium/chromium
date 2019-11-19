// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

/**
 * Mock implementation of {ProgressCenter} for tests.
 * @implements {ProgressCenter}
 * @final
 */
class MockProgressCenter {
  constructor() {
    /**
     * Items stored in the progress center.
     * @const {!Object<ProgressCenterItem>}
     */
    this.items = {};
  }

  /**
   * Stores an item to the progress center.
   * @param {ProgressCenterItem} item Progress center item to be stored.
   */
  updateItem(item) {
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
}
