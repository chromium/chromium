// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {str, util} from '../../common/js/util.js';
import {FakeEntry} from '../../externs/files_app_entry_interfaces.js';

import {DirectoryModel} from './directory_model.js';

/**
 * This class controls wires file-type filter UI and the filter settings in
 * Recents view.
 */
export class FileTypeFiltersController {
  /**
   * @param {!HTMLElement} fileTypeFilterContainer
   * @param {!DirectoryModel} directoryModel
   * @param {!FakeEntry} recentEntry
   */
  constructor(fileTypeFilterContainer, directoryModel, recentEntry) {
    /**
     * @private {!HTMLElement}
     * @const
     */
    this.container_ = fileTypeFilterContainer;

    /**
     * @private {!DirectoryModel}
     * @const
     */
    this.directoryModel_ = directoryModel;

    /**
     * @private {!FakeEntry}
     * @const
     */
    this.recentEntry_ = recentEntry;

    /**
     * @private {!HTMLElement}
     * @const
     */
    this.allFilterButton_ = this.createFilterButton_(
        str('MEDIA_VIEW_ALL_ROOT_LABEL'),
        chrome.fileManagerPrivate.RecentFileType.ALL);

    /**
     * @private {!HTMLElement}
     * @const
     */
    this.audioFilterButton_ = this.createFilterButton_(
        str('MEDIA_VIEW_AUDIO_ROOT_LABEL'),
        chrome.fileManagerPrivate.RecentFileType.AUDIO);

    /**
     * @private {!HTMLElement}
     * @const
     */
    this.imageFilterButton_ = this.createFilterButton_(
        str('MEDIA_VIEW_IMAGES_ROOT_LABEL'),
        chrome.fileManagerPrivate.RecentFileType.IMAGE);

    /**
     * @private {!HTMLElement}
     * @const
     */
    this.videoFilterButton_ = this.createFilterButton_(
        str('MEDIA_VIEW_VIDEOS_ROOT_LABEL'),
        chrome.fileManagerPrivate.RecentFileType.VIDEO);

    this.directoryModel_.addEventListener(
        'directory-changed', this.onCurrentDirectoryChanged_.bind(this));

    this.updateButtonActiveStates_();
  }

  /**
   * Creates filter button's UI element.
   * @param {string} label Label of the filter button.
   * @param {chrome.fileManagerPrivate.RecentFileType} fileType File type filter
   *     for the filter button.
   * @private
   */
  createFilterButton_(label, fileType) {
    const button =
        util.createChild(this.container_, 'file-type-filter-button', 'button');
    button.textContent = label;
    // Store the "RecentFileType" on the button element so we know the mapping
    // between the DOM element and its corresponding "RecentFileType", which
    // will make it easier to trigger UI change based on "RecentFileType" or
    // vice versa.
    button.setAttribute('file-type-filter', fileType);
    button.onclick = this.onFilterButtonClicked_.bind(this);
    return button;
  }

  /**
   * Updates the UI when the current directory changes.
   * @param {!Event} event Event.
   * @private
   */
  onCurrentDirectoryChanged_(event) {
    // We show filter buttons only in Recents view at this moment.
    this.container_.hidden = !(event.newDirEntry == this.recentEntry_);
  }

  /**
   * Updates the UI when one of the filter buttons is clicked.
   * @param {Event} event Event.
   * @private
   */
  onFilterButtonClicked_(event) {
    const {target} = event;
    // At least one filter button should be selected, so we don't allow user to
    // unselect a filter button by clicking it, e.g. do nothing if an active
    // filter button is clicked.
    if (target.classList.contains('active')) {
      return;
    }
    const newFilter = /** @type {!chrome.fileManagerPrivate.RecentFileType} */
        (target.getAttribute('file-type-filter'));
    this.recentEntry_.recentFileType = newFilter;
    this.updateButtonActiveStates_();
    // Refresh current directory with the updated Recent setting.
    // We don't need to invalidate the cached metadata for this rescan.
    this.directoryModel_.rescan(false);
  }

  /**
   * Update the filter button active states based on current `recentFileType`.
   * Every time `recentFileType` is changed (including the initialization),
   * this method needs to be called to render the UI to reflect the file
   * type filter change.
   * @private
   */
  updateButtonActiveStates_() {
    const currentFilter = this.recentEntry_.recentFileType;
    const buttons = [
      this.allFilterButton_, this.audioFilterButton_, this.imageFilterButton_,
      this.videoFilterButton_
    ];
    buttons.forEach(button => {
      const fileTypeFilter =
          /** @type {!chrome.fileManagerPrivate.RecentFileType} */ (
              button.getAttribute('file-type-filter'));
      button.classList.toggle('active', currentFilter === fileTypeFilter);
    });
  }
}
