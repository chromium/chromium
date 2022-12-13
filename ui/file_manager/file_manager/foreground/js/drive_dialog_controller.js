// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';

import {DriveDialogControllerInterface} from '../../externs/drive_dialog_controller.js';
import {FileManagerUI} from './ui/file_manager_ui.js';
import {FilesConfirmDialog} from './ui/files_confirm_dialog.js';

/**
 * Controls last modified column in the file table.
 * @implements {DriveDialogControllerInterface}
 */
export class DriveDialogController {
  /**
   * @param {!FileManagerUI} ui
   */
  constructor(ui) {
    /** @private @const {!FilesConfirmDialog} */
    this.dialog_ = new FilesConfirmDialog(ui.element);

    /** @private {boolean} */
    this.open_ = false;

    // Listen to mount events in order to hide the dialog when Drive shuts down.
    chrome.fileManagerPrivate.onMountCompleted.addListener(
        this.onMountCompleted_.bind(this));
  }

  /**
   * @return {boolean} Whether the dialog is open or not.
   */
  get open() {
    return this.open_;
  }

  /**
   * Handles dialog result.
   * @param {boolean} result
   * @private
   */
  onResult_(result) {
    this.open_ = false;
    chrome.fileManagerPrivate.notifyDriveDialogResult(
        result ? chrome.fileManagerPrivate.DriveDialogResult.ACCEPT :
                 chrome.fileManagerPrivate.DriveDialogResult.REJECT);
  }

  /**
   * Sets up the given dialog for a given Drive dialog type, and shows it.
   * @param {!chrome.fileManagerPrivate.DriveConfirmDialogEvent} event
   */
  showDialog(event) {
    this.open_ = true;
    let message = '';
    switch (event.type) {
      case chrome.fileManagerPrivate.DriveConfirmDialogType.ENABLE_DOCS_OFFLINE:
        message = loadTimeData.getString('OFFLINE_ENABLE_MESSAGE');
        this.dialog_.setOkLabel(
            loadTimeData.getString('OFFLINE_ENABLE_ACCEPT'));
        this.dialog_.setCancelLabel(
            loadTimeData.getString('OFFLINE_ENABLE_REJECT'));
        break;
    }
    this.dialog_.show(
        message, () => this.onResult_(true), () => this.onResult_(false));
  }

  onMountCompleted_(event) {
    if (event.eventType ===
            chrome.fileManagerPrivate.MountCompletedEventType.UNMOUNT &&
        event.volumeMetadata.volumeType ===
            chrome.fileManagerPrivate.VolumeType.DRIVE) {
      this.open && this.dialog_.hide();
    }
  }
}
