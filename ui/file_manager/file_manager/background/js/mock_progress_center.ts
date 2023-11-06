// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ProgressCenterItem, ProgressItemState} from '../../common/js/progress_center_common.js';
import {ProgressCenter} from '../../externs/background/progress_center.js';

/**
 * Mock implementation of {ProgressCenter} for tests.
 * @final
 */
export class MockProgressCenter implements ProgressCenter {
  /**
   * Items stored in the progress center.
   */
  readonly items: Record<string, ProgressCenterItem> = {};

  /**
   * Stores an item to the progress center.
   * @param item Progress center item to be stored.
   */
  updateItem(item: ProgressCenterItem) {
    this.items[item.id] = item;
  }

  /**
   * Obtains an item stored in the progress center.
   * @param id ID spcifying the progress item.
   */
  getItemById(id: string): ProgressCenterItem|undefined {
    return this.items[id];
  }

  requestCancel() {}
  addPanel() {}
  removePanel() {}
  neverNotifyCompleted() {}

  /**
   * Returns the number of unique keys in |this.items|.
   */
  getItemCount(): number {
    const array = Object.keys(this.items);
    return array.length;
  }

  /**
   * Returns the items that have a given state.
   * @param state State to filter by.
   */
  getItemsByState(state: ProgressItemState): ProgressCenterItem[] {
    return Object.values(this.items).filter(item => item.state == state);
  }
}
