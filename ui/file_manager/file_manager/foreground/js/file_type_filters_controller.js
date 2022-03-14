// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {metrics} from '../../common/js/metrics.js';
import {str, util} from '../../common/js/util.js';
import {DirectoryChangeEvent} from '../../externs/directory_change_event.js';
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
   * @param {chrome.fileManagerPrivate.RecentFileType} fileType File type filter
   *     which needs to be recorded.
   * @private
   */
  recordFileTypeFilterUMA_(fileType) {
    /**
     * Keep the order of this in sync with FileManagerRecentFilterType in
     * tools/metrics/histograms/enums.xml.
     * The array indices will be recorded in UMA as enum values. The index for
     * each filter type should never be renumbered nor reused in this array.
     */
    const FileTypeFiltersForUMA =
        /** @type {!Array<!chrome.fileManagerPrivate.RecentFileType>} */ ([
          chrome.fileManagerPrivate.RecentFileType.ALL,    // 0
          chrome.fileManagerPrivate.RecentFileType.AUDIO,  // 1
          chrome.fileManagerPrivate.RecentFileType.IMAGE,  // 2
          chrome.fileManagerPrivate.RecentFileType.VIDEO,  // 3
        ]);
    Object.freeze(FileTypeFiltersForUMA);
    metrics.recordEnum('Recent.FilterByType', fileType, FileTypeFiltersForUMA);
  }

  /**
   * Creates filter button's UI element.
   * @param {string} label Label of the filter button.
   * @param {chrome.fileManagerPrivate.RecentFileType} fileType File type filter
   *     for the filter button.
   * @private
   */
  createFilterButton_(label, fileType) {
    const button = util.createChild(
        this.container_, 'file-type-filter-button', 'cr-button');
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
    const directoryChangeEvent = /** @type {!DirectoryChangeEvent} */ (event);
    const isEnteringRecentEntry =
        util.isSameEntry(directoryChangeEvent.newDirEntry, this.recentEntry_);
    const isLeavingRecentEntry = !isEnteringRecentEntry &&
        util.isSameEntry(
            directoryChangeEvent.previousDirEntry, this.recentEntry_);
    // We show filter buttons only in Recents view at this moment.
    this.container_.hidden = !isEnteringRecentEntry;
    // Reset the filter back to "All" on leaving Recents view.
    if (isLeavingRecentEntry) {
      this.recentEntry_.recentFileType =
          chrome.fileManagerPrivate.RecentFileType.ALL;
      this.updateButtonActiveStates_();
    }
  }

  /**
   * Updates the UI when one of the filter buttons is clicked.
   * @param {Event} event Event.
   * @private
   */
  onFilterButtonClicked_(event) {
    const target = /** @type {HTMLButtonElement} */ (event.target);
    const isButtonActive = target.classList.contains('active');
    const buttonFilter =
        /** @type {!chrome.fileManagerPrivate.RecentFileType} */
        (target.getAttribute('file-type-filter'));
    // Do nothing if "All" button is active and being clicked again.
    if (isButtonActive &&
        buttonFilter === chrome.fileManagerPrivate.RecentFileType.ALL) {
      return;
    }
    // Clicking an active button will make it inactive and make "All"
    // button active.
    const newFilter = isButtonActive ?
        chrome.fileManagerPrivate.RecentFileType.ALL :
        buttonFilter;
    this.recentEntry_.recentFileType = newFilter;
    this.updateButtonActiveStates_();
    if (isButtonActive) {
      this.allFilterButton_.focus();
    }
    // Refresh current directory with the updated Recent setting.
    // We don't need to invalidate the cached metadata for this rescan.
    this.directoryModel_.rescan(false);
    this.recordFileTypeFilterUMA_(newFilter);
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
