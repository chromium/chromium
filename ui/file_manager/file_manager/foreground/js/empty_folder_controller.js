// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';

import {str, util} from '../../common/js/util.js';
import {FakeEntry} from '../../externs/files_app_entry_interfaces.js';

import {DirectoryModel} from './directory_model.js';
import {FileListModel} from './file_list_model.js';

/**
 * Empty folder controller which controls the empty folder element inside
 * the file list container.
 */
export class EmptyFolderController {
  /**
   * @param {!HTMLElement} emptyFolder Empty folder element.
   * @param {!DirectoryModel} directoryModel Directory model.
   * @param {!FakeEntry} recentEntry Entry represents Recent view.
   */
  constructor(emptyFolder, directoryModel, recentEntry) {
    /**
     * @private {!HTMLElement}
     */
    this.emptyFolder_ = emptyFolder;

    /**
     * @private {!DirectoryModel}
     */
    this.directoryModel_ = directoryModel;

    /**
     * @private {!FakeEntry}
     * @const
     */
    this.recentEntry_ = recentEntry;

    /**
     * @private {!HTMLElement}
     */
    this.label_ = util.queryRequiredElement('.label', emptyFolder);

    /**
     * @private {boolean}
     */
    this.isScanning_ = false;

    this.directoryModel_.addEventListener(
        'scan-started', this.onScanStarted_.bind(this));
    this.directoryModel_.addEventListener(
        'scan-failed', this.onScanFinished_.bind(this));
    this.directoryModel_.addEventListener(
        'scan-cancelled', this.onScanFinished_.bind(this));
    this.directoryModel_.addEventListener(
        'scan-completed', this.onScanFinished_.bind(this));
    this.directoryModel_.addEventListener(
        'rescan-completed', this.onScanFinished_.bind(this));
  }

  /**
   * Handles scan start.
   * @private
   */
  onScanStarted_() {
    this.isScanning_ = true;
    this.updateUI_();
  }

  /**
   * Handles scan finish.
   * @private
   */
  onScanFinished_() {
    this.isScanning_ = false;
    this.updateUI_();
  }

  /**
   * Updates visibility of empty folder UI.
   * @private
   */
  updateUI_() {
    const isRecent =
        util.isRecentRootType(this.directoryModel_.getCurrentRootType());
    const fileListModel = assert(this.directoryModel_.getFileList());
    if (!isRecent || this.isScanning_ || fileListModel.length > 0) {
      this.emptyFolder_.hidden = true;
      this.label_.innerText = '';
      return;
    }

    this.emptyFolder_.hidden = false;
    switch (this.recentEntry_.recentFileType) {
      case chrome.fileManagerPrivate.RecentFileType.AUDIO:
        this.label_.innerText = str('RECENT_EMPTY_AUDIO_FOLDER');
        break;
      case chrome.fileManagerPrivate.RecentFileType.DOCUMENT:
        this.label_.innerText = str('RECENT_EMPTY_DOCUMENTS_FOLDER');
        break;
      case chrome.fileManagerPrivate.RecentFileType.IMAGE:
        this.label_.innerText = str('RECENT_EMPTY_IMAGES_FOLDER');
        break;
      case chrome.fileManagerPrivate.RecentFileType.VIDEO:
        this.label_.innerText = str('RECENT_EMPTY_VIDEOS_FOLDER');
        break;
      default:
        this.label_.innerText = str('RECENT_EMPTY_FOLDER');
    }
  }
}
