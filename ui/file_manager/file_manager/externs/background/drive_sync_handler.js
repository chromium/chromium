// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DriveDialogControllerInterface} from '../drive_dialog_controller.js';

/**
 * Handler of the background page for the Drive sync events. Implementations
 * of this interface must @extends {cr.EventTarget}.
 *
 * @interface
 */
export class DriveSyncHandler extends EventTarget {
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

  /**
   * @param {Object} model
   */
  set metadataModel(model) {}

  /**
   * Adds a dialog to be controlled by DriveSyncHandler.
   * @param {string} appId App ID of window containing the dialog.
   * @param {DriveDialogControllerInterface} dialog Dialog to be controlled.
   */
  addDialog(appId, dialog) {}

  /**
   * Removes a dialog from being controlled by DriveSyncHandler.
   * @param {string} appId App ID of window containing the dialog.
   */
  removeDialog(appId) {}
}
