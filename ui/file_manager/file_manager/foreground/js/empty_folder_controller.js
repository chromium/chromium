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
 * The image shown when search returned no results.
 * @type {string}
 * @const
 */
const SEARCH_EMPTY_RESULTS =
    'foreground/images/files/ui/empty_search_results.svg#empty_search_results';

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
   * Shows the given message. It may consist of just the `title`, or
   * `title` and `description`.
   * @param {string} title
   * @param {string=} description
   * @private
   */
  showMessage_(title, description = '') {
    if (!description) {
      this.label_.appendChild(document.createTextNode(title));
      return;
    }

    const titleSpan = document.createElement('span');
    titleSpan.id = 'empty-folder-title';
    titleSpan.innerText = title;
    const descSpan = document.createElement('span');
    descSpan.innerText = description;
    this.label_.appendChild(titleSpan);
    this.label_.appendChild(document.createElement('br'));
    this.label_.appendChild(descSpan);
  }

  /**
   * Updates visibility of empty folder UI.
   * @private
   */
  updateUI_() {
    const currentRootType = this.directoryModel_.getCurrentRootType();

    let svgRef = null;
    if (util.isRecentRootType(currentRootType)) {
      svgRef = RECENTS_EMPTY_FOLDER;
    } else if (currentRootType === VolumeManagerCommon.RootType.TRASH) {
      svgRef = TRASH_EMPTY_FOLDER;
    } else if (this.directoryModel_.isSearching()) {
      if (util.isSearchV2Enabled()) {
        svgRef = SEARCH_EMPTY_RESULTS;
      }
    }

    const fileListModel = assert(this.directoryModel_.getFileList());

    this.label_.innerText = '';
    if (svgRef === null || this.isScanning_ || fileListModel.length > 0) {
      this.emptyFolder_.hidden = true;
      return;
    }

    const svgUseElement = this.image_.querySelector('.image > svg > use');
    svgUseElement.setAttributeNS(
        'http://www.w3.org/1999/xlink', 'xlink:href', svgRef);
    this.emptyFolder_.hidden = false;

    if (svgRef === TRASH_EMPTY_FOLDER) {
      this.showMessage_(
          str('EMPTY_TRASH_FOLDER_TITLE'), str('EMPTY_TRASH_FOLDER_DESC'));
      return;
    }

    if (svgRef === SEARCH_EMPTY_RESULTS) {
      this.showMessage_(
          str('SEARCH_NO_MATCHING_RESULTS_TITLE'),
          str('SEARCH_NO_MATCHING_RESULTS_DESC'));
      return;
    }

    switch (this.recentEntry_.fileCategory) {
      case chrome.fileManagerPrivate.FileCategory.AUDIO:
        this.showMessage_(str('RECENT_EMPTY_AUDIO_FOLDER'));
        break;
      case chrome.fileManagerPrivate.FileCategory.DOCUMENT:
        this.showMessage_(str('RECENT_EMPTY_DOCUMENTS_FOLDER'));
        break;
      case chrome.fileManagerPrivate.FileCategory.IMAGE:
        this.showMessage_(str('RECENT_EMPTY_IMAGES_FOLDER'));
        break;
      case chrome.fileManagerPrivate.FileCategory.VIDEO:
        this.showMessage_(str('RECENT_EMPTY_VIDEOS_FOLDER'));
        break;
      default:
        this.showMessage_(str('RECENT_EMPTY_FOLDER'));
    }
  }
}
