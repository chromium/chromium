// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Progress center at the background page.
 * @interface
 */
class ProgressCenter {
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
  updateItem(item) {}

  /**
   * Requests to cancel the progress item.
   * @param {string} id Progress ID to be requested to cancel.
   */
  requestCancel(id) {}

  /**
   * Adds a panel UI to the notification center.
   * @param {ProgressCenterPanelInterface} panel Panel UI.
   */
  addPanel(panel) {}

  /**
   * Removes a panel UI from the notification center.
   * @param {ProgressCenterPanelInterface} panel Panel UI.
   */
  removePanel(panel) {}

  /**
   * Obtains item by ID.
   * @param {string} id ID of progress item.
   * @return {?ProgressCenterItem} Progress center item having the specified
   *     ID. Null if the item is not found.
   */
  getItemById(id) {}
}
