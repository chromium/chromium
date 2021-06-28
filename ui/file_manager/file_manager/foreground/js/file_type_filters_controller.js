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
    this.audioFilterButton_ =
        this.createFilterButton_(str('MEDIA_VIEW_AUDIO_ROOT_LABEL'));

    /**
     * @private {!HTMLElement}
     * @const
     */
    this.imageFilterButton_ =
        this.createFilterButton_(str('MEDIA_VIEW_IMAGES_ROOT_LABEL'));

    /**
     * @private {!HTMLElement}
     * @const
     */
    this.videoFilterButton_ =
        this.createFilterButton_(str('MEDIA_VIEW_VIDEOS_ROOT_LABEL'));

    this.directoryModel_.addEventListener(
        'directory-changed', this.onCurrentDirectoryChanged_.bind(this));
  }

  /**
   * Creates filter button's UI element.
   * @param {string} label Label of the filter button.
   * @private
   */
  createFilterButton_(label) {
    const button =
        util.createChild(this.container_, 'file-type-filter-button', 'button');
    button.textContent = label;
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
    // Reset the filter buttons' active state on leaving Recents view.
    if (event.previousDirEntry == this.recentEntry_ &&
        event.newDirEntry != this.recentEntry_) {
      this.audioFilterButton_.classList.toggle('active', false);
      this.imageFilterButton_.classList.toggle('active', false);
      this.videoFilterButton_.classList.toggle('active', false);
    }
  }

  /**
   * Updates the UI when one of the filter buttons is clicked.
   * @param {Event} event Event.
   * @private
   */
  onFilterButtonClicked_(event) {
    // Toggle active state of clicked filter. When one filter button is clicked,
    // other filter buttons should become inactive.
    this.audioFilterButton_.classList.toggle(
        'active', event.target == this.audioFilterButton_ ? undefined : false);
    this.imageFilterButton_.classList.toggle(
        'active', event.target == this.imageFilterButton_ ? undefined : false);
    this.videoFilterButton_.classList.toggle(
        'active', event.target == this.videoFilterButton_ ? undefined : false);
    this.refreshRecentView_();
  }

  /**
   * Refreshes the current directory based on the filter settings.
   * @private
   */
  refreshRecentView_() {
    // Update the Recent entry's setting based on the 'active' state of
    // filter buttons.
    let fileType = chrome.fileManagerPrivate.RecentFileType.ALL;
    if (this.audioFilterButton_.classList.contains('active')) {
      fileType = chrome.fileManagerPrivate.RecentFileType.AUDIO;
    } else if (this.imageFilterButton_.classList.contains('active')) {
      fileType = chrome.fileManagerPrivate.RecentFileType.IMAGE;
    } else if (this.videoFilterButton_.classList.contains('active')) {
      fileType = chrome.fileManagerPrivate.RecentFileType.VIDEO;
    }
    this.recentEntry_.recentFileType = fileType;
    // Refresh current directory with the updated Recent setting.
    // We don't need to invalidate the cached metadata for this rescan.
    this.directoryModel_.rescan(false);
  }
}
