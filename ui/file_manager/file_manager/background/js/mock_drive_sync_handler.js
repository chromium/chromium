// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Mock of DriveSyncHandler.
 * @implements {DriveSyncHandler}
 */
class MockDriveSyncHandler extends cr.EventTarget {
  constructor() {
    super();

    /**
     * @type {boolean} Drive sync suppressed state.
     * @private
     */
    this.syncSuppressed_ = false;

    /**
     * @type {boolean} Drive sync disabled on mobile notification state.
     * @private
     */
    this.showingDisabledMobileSyncNotification_ = false;
  }

  /**
   * Returns the completed event name.
   * @return {string}
   */
  getCompletedEventName() {
    return MockDriveSyncHandler.DRIVE_SYNC_COMPLETED_EVENT;
  }

  /**
   * Returns whether the Drive sync is currently suppressed or not.
   * @return {boolean}
   */
  isSyncSuppressed() {
    return this.syncSuppressed_;
  }

  /**
   * Shows a notification that Drive sync is disabled on cellular networks.
   */
  showDisabledMobileSyncNotification() {
    this.showingDisabledMobileSyncNotification_ = true;
  }

  /**
   * @return {boolean} Whether the handler is syncing items or not.
   */
  get syncing() {
    return false;
  }
}

/**
 * Completed event name.
 * @type {string}
 * @private
 * @const
 */
MockDriveSyncHandler.DRIVE_SYNC_COMPLETED_EVENT = 'completed';
