// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

import {queryRequiredElement} from '../../common/js/dom_utils.js';
import {str, util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {FakeEntry} from '../../externs/files_app_entry_interfaces.js';

import {DirectoryModel} from './directory_model.js';

/**
 * The empty state image for the Recents folder.
 * @type {string}
 * @const
 */
const RECENTS_EMPTY_FOLDER =
    'foreground/images/files/ui/empty_folder.svg#empty_folder';

/**
 * The empty state image for the Trash folder.
 * @type {string}
 * @const
 */
const TRASH_EMPTY_FOLDER =
    'foreground/images/files/ui/empty_trash_folder.svg#empty_trash_folder';

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
    this.label_ = queryRequiredElement('.label', emptyFolder);

    /**
     * @private {!HTMLElement}
     */
    this.image_ = queryRequiredElement('.image', emptyFolder);

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
    const currentRootType = this.directoryModel_.getCurrentRootType();

    const isRecent = util.isRecentRootType(currentRootType);
    const isTrash = currentRootType === VolumeManagerCommon.RootType.TRASH;
    const fileListModel = assert(this.directoryModel_.getFileList());

    this.label_.innerText = '';
    if ((!isRecent && !isTrash) || this.isScanning_ ||
        fileListModel.length > 0) {
      this.emptyFolder_.hidden = true;
      return;
    }

    const svgUseElement = this.image_.querySelector('.image > svg > use');
    svgUseElement.setAttributeNS(
        'http://www.w3.org/1999/xlink', 'xlink:href',
        (isTrash) ? TRASH_EMPTY_FOLDER : RECENTS_EMPTY_FOLDER);
    this.emptyFolder_.hidden = false;
    if (isTrash) {
      const titleSpan = document.createElement('span');
      titleSpan.id = 'empty-folder-title';
      titleSpan.innerText = str('EMPTY_TRASH_FOLDER_TITLE');
      const descSpan = document.createElement('span');
      descSpan.innerText = str('EMPTY_TRASH_FOLDER_DESC');
      const breakElement = document.createElement('br');
      this.label_.appendChild(titleSpan);
      this.label_.appendChild(breakElement);
      this.label_.appendChild(descSpan);
      return;
    }

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
