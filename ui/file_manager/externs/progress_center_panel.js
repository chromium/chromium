// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Interface implemented in foreground page that the background page uses to
 * send progress event updates to the foreground page, and to receive cancel
 * and dismiss events from the foreground page.
 * @interface
 */
class ProgressCenterPanelInterface {
  constructor() {
    /**
     * Callback to be called with the ID of the progress item when the cancel
     * button is clicked.
     * @public {?function(string)}
     */
    this.cancelCallback;

    /**
     * Callback to be called with the ID of the error item when user pressed
     * dismiss button of it.
     * @public {?function(string)}
     */
    this.dismissErrorItemCallback;
  }

  /**
   * Updates an item to the progress center panel.
   * @param {!ProgressCenterItem} item Item including new contents.
   */
  updateItem(item) {}

  /**
   * Requests all item groups to dismiss an error item.
   * @param {string} id Item id.
   */
  dismissErrorItem(id) {}
}
