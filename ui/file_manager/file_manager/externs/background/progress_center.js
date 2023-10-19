// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ProgressCenterItem} from '../../common/js/progress_center_common.js';
import {ProgressCenterPanelInterface} from '../progress_center_panel.js';

/**
 * Progress center at the background page.
 * @interface
 */
export class ProgressCenter {
  /**
   * Turns off sending updates when a file operation reaches 'completed' state.
   * Used for testing UI that can be ephemeral otherwise.
   */
  neverNotifyCompleted() {}
  /**
   * Updates the item in the progress center.
   * If the item has a new ID, the item is added to the item list.
   * @param {ProgressCenterItem} item Updated item.
   */
  // @ts-ignore: error TS6133: 'item' is declared but its value is never read.
  updateItem(item) {}

  /**
   * Requests to cancel the progress item.
   * @param {string} id Progress ID to be requested to cancel.
   */
  // @ts-ignore: error TS6133: 'id' is declared but its value is never read.
  requestCancel(id) {}

  /**
   * Adds a panel UI to the notification center.
   * @param {ProgressCenterPanelInterface} panel Panel UI.
   */
  // @ts-ignore: error TS6133: 'panel' is declared but its value is never read.
  addPanel(panel) {}

  /**
   * Removes a panel UI from the notification center.
   * @param {ProgressCenterPanelInterface} panel Panel UI.
   */
  // @ts-ignore: error TS6133: 'panel' is declared but its value is never read.
  removePanel(panel) {}

  /**
   * Obtains item by ID.
   * @param {string} id ID of progress item.
   * @return {ProgressCenterItem|undefined} Progress center item having the
   *     specified ID. Null if the item is not found.
   */
  // @ts-ignore: error TS6133: 'id' is declared but its value is never read.
  getItemById(id) {
    return undefined;
  }
}
