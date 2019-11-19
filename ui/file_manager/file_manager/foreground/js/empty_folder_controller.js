// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Empty folder controller.
 */
class EmptyFolderController {
  /**
   * @param {!EmptyFolder} emptyFolder Empty folder ui.
   * @param {!DirectoryModel} directoryModel Directory model.
   * @param {!FilesAlertDialog} alertDialog Alert dialog.
   */
  constructor(emptyFolder, directoryModel, alertDialog) {
    /**
     * @private {!EmptyFolder}
     */
    this.emptyFolder_ = emptyFolder;

    /**
     * @private {!DirectoryModel}
     */
    this.directoryModel_ = directoryModel;

    /**
     * @private {!FilesAlertDialog}
     */
    this.alertDialog_ = alertDialog;

    /**
     * @private {!FileListModel}
     */
    this.dataModel_ = assert(this.directoryModel_.getFileList());

    /**
     * @private {boolean}
     */
    this.isScanning_ = false;

    this.directoryModel_.addEventListener(
        'scan-started', this.onScanStarted_.bind(this));
    this.directoryModel_.addEventListener(
        'scan-failed', this.onScanFailed_.bind(this));
    this.directoryModel_.addEventListener(
        'scan-cancelled', this.onScanFinished_.bind(this));
    this.directoryModel_.addEventListener(
        'scan-completed', this.onScanFinished_.bind(this));
    this.directoryModel_.addEventListener(
        'rescan-completed', this.onScanFinished_.bind(this));

    this.dataModel_.addEventListener('splice', this.onSplice_.bind(this));
  }

  /**
   * Handles splice event.
   * @private
   */
  onSplice_() {
    this.update_();
  }

  /**
   * Handles scan start.
   * @private
   */
  onScanStarted_() {
    this.isScanning_ = true;
    this.update_();
  }

  /**
   * Handles scan fail.
   * @param {Event} event Event may contain error field containing DOMError for
   *   alert.
   * @private
   */
  onScanFailed_(event) {
    this.isScanning_ = false;
    // Show alert for crostini connection error.
    if (event.error.name == DirectoryModel.CROSTINI_CONNECT_ERR) {
      this.alertDialog_.showWithTitle(
          str('ERROR_LINUX_FILES_CONNECTION'), event.error.message);
    }
    this.update_();
  }

  /**
   * Handles scan finish.
   * @private
   */
  onScanFinished_() {
    this.isScanning_ = false;
    this.update_();
  }

  /**
   * Updates visibility of empty folder UI.
   * @private
   */
  update_() {
    if (!this.isScanning_ && this.dataModel_.length === 0) {
      const query = this.directoryModel_.getLastSearchQuery();
      let html = '';
      if (query) {
        html = strf('SEARCH_NO_MATCHING_FILES_HTML', util.htmlEscape(query));
      } else {
        html = str('EMPTY_FOLDER');
      }

      this.emptyFolder_.setMessage(html);
      this.emptyFolder_.show();
    } else {
      this.emptyFolder_.hide();
    }
  }
}
