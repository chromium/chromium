// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ProgressCenterItem} from '../common/js/progress_center_common.js';

/**
 * Interface implemented in foreground page that the background page uses to
 * send progress event updates to the foreground page, and to receive cancel
 * and dismiss events from the foreground page.
 * @interface
 */
export class ProgressCenterPanelInterface {
  constructor() {
    /**
     * Callback to be called with the ID of the progress item when the cancel
     * button is clicked.
     * @public @type {?function(string):void}
     */
    // @ts-ignore: error TS2339: Property 'cancelCallback' does not exist on
    // type 'ProgressCenterPanelInterface'.
    this.cancelCallback;

    /**
     * Callback to be called with the ID of the error item when user pressed
     * dismiss button of it.
     * @public @type {?function(string):void}
     */
    // @ts-ignore: error TS2551: Property 'dismissErrorItemCallback' does not
    // exist on type 'ProgressCenterPanelInterface'. Did you mean
    // 'dismissErrorItem'?
    this.dismissErrorItemCallback;
  }

  /**
   * Updates an item to the progress center panel.
   * @param {!ProgressCenterItem} item Item including new contents.
   */
  // @ts-ignore: error TS6133: 'item' is declared but its value is never read.
  updateItem(item) {}

  /**
   * Requests all item groups to dismiss an error item.
   * @param {string} id Item id.
   */
  // @ts-ignore: error TS6133: 'id' is declared but its value is never read.
  dismissErrorItem(id) {}

  /**
   * @param {number} _pendingTimeMs
   * @param {number} _timeoutToRemoveMs
   */
  setTimingForTests(_pendingTimeMs, _timeoutToRemoveMs) {}
}
