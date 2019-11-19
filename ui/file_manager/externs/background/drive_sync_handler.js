// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Handler of the background page for the Drive sync events. Implementations
 * of this interface must @extends {cr.EventTarget}.
 *
 * @interface
 * @extends {EventTarget}
 */
class DriveSyncHandler extends EventTarget {
  /**
   * Returns the completed event name.
   * @return {string}
   */
  getCompletedEventName() {}

  /**
   * Returns whether the Drive sync is currently suppressed or not.
   * @return {boolean}
   */
  isSyncSuppressed() {}

  /**
   * Shows a notification that Drive sync is disabled on cellular networks.
   */
  showDisabledMobileSyncNotification() {}

  /**
   * @return {boolean} Whether the handler is syncing items or not.
   */
  get syncing() {}
}
