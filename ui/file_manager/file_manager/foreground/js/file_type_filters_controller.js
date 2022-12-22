// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {createChild} from '../../common/js/dom_utils.js';
import {metrics} from '../../common/js/metrics.js';
import {str, strf, util} from '../../common/js/util.js';
import {DirectoryChangeEvent} from '../../externs/directory_change_event.js';
import {FakeEntry} from '../../externs/files_app_entry_interfaces.js';

import {DirectoryModel} from './directory_model.js';
import {A11yAnnounce} from './ui/a11y_announce.js';

/**
 * This class controls wires file-type filter UI and the filter settings in
 * Recents view.
 */
export class FileTypeFiltersController {
  /**
   * @param {!HTMLElement} fileTypeFilterContainer
   * @param {!DirectoryModel} directoryModel
   * @param {!FakeEntry} recentEntry
   * @param {!A11yAnnounce} a11y
   */
  constructor(fileTypeFilterContainer, directoryModel, recentEntry, a11y) {
    /**
     * @private {Map<chrome.fileManagerPrivate.RecentFileType, string>}
     * @const
     */
    this.filterTypeToTranslationKeyMap_ = new Map([
      [
        chrome.fileManagerPrivate.RecentFileType.ALL,
        'MEDIA_VIEW_ALL_ROOT_LABEL',
      ],
      [
        chrome.fileManagerPrivate.RecentFileType.AUDIO,
        'MEDIA_VIEW_AUDIO_ROOT_LABEL',
      ],
      [
        chrome.fileManagerPrivate.RecentFileType.IMAGE,
        'MEDIA_VIEW_IMAGES_ROOT_LABEL',
      ],
      [
        chrome.fileManagerPrivate.RecentFileType.VIDEO,
        'MEDIA_VIEW_VIDEOS_ROOT_LABEL',
      ],
      [
        chrome.fileManagerPrivate.RecentFileType.DOCUMENT,
        'MEDIA_VIEW_DOCUMENTS_ROOT_LABEL',
      ],
    ]);

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
     * @private {!A11yAnnounce}
     * @const
     */
    this.a11y_ = a11y;

    /**
     * @private {!HTMLElement}
     * @const
     */
    this.allFilterButton_ =
        this.createFilterButton_(chrome.fileManagerPrivate.RecentFileType.ALL);

    /**
     * @private {!HTMLElement}
     * @const
     */
    this.audioFilterButton_ = this.createFilterButton_(
        chrome.fileManagerPrivate.RecentFileType.AUDIO);

    /**
     * @private {!HTMLElement|null}
     * @const
     */
    this.documentFilterButton_ = this.createFilterButton_(
        chrome.fileManagerPrivate.RecentFileType.DOCUMENT);

    /**
     * @private {!HTMLElement}
     * @const
     */
    this.imageFilterButton_ = this.createFilterButton_(
        chrome.fileManagerPrivate.RecentFileType.IMAGE);

    /**
     * @private {!HTMLElement}
     * @const
     */
    this.videoFilterButton_ = this.createFilterButton_(
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
          chrome.fileManagerPrivate.RecentFileType.ALL,       // 0
          chrome.fileManagerPrivate.RecentFileType.AUDIO,     // 1
          chrome.fileManagerPrivate.RecentFileType.IMAGE,     // 2
          chrome.fileManagerPrivate.RecentFileType.VIDEO,     // 3
          chrome.fileManagerPrivate.RecentFileType.DOCUMENT,  // 4
        ]);
    Object.freeze(FileTypeFiltersForUMA);
    metrics.recordEnum('Recent.FilterByType', fileType, FileTypeFiltersForUMA);
  }

  /**
   * Speak voice message in screen recording mode depends on the existing
   * filter and the new filter type.
   *
   * @param {!chrome.fileManagerPrivate.RecentFileType} currentFilter
   * @param {!chrome.fileManagerPrivate.RecentFileType} newFilter
   */
  speakA11yMessage(currentFilter, newFilter) {
    /**
     * When changing button active/inactive states, the common voice message is
     * "AAA filter is off. BBB filter is on.", i.e. the "off" message first
     * then the "on" message. However there are some exceptions:
     *  * If the active filter changes from "All" to others, no need to say
     * the off message.
     *  * If the active filter changes from others to "All", the on message will
     * be a filter reset message.
     */
    const isFromAllToOthers =
        currentFilter === chrome.fileManagerPrivate.RecentFileType.ALL;
    const isFromOthersToAll =
        newFilter === chrome.fileManagerPrivate.RecentFileType.ALL;
    let offMessage = strf(
        'RECENT_VIEW_FILTER_OFF',
        str(this.filterTypeToTranslationKeyMap_.get(currentFilter)));
    let onMessage = strf(
        'RECENT_VIEW_FILTER_ON',
        str(this.filterTypeToTranslationKeyMap_.get(newFilter)));
    if (isFromAllToOthers) {
      offMessage = '';
    }
    if (isFromOthersToAll) {
      onMessage = str('RECENT_VIEW_FILTER_RESET');
    }
    this.a11y_.speakA11yMessage(
        offMessage ? `${offMessage} ${onMessage}` : onMessage);
  }

  /**
   * Creates filter button's UI element.
   *
   * @param {!chrome.fileManagerPrivate.RecentFileType} fileType File type
   *     for the filter button.
   * @private
   */
  createFilterButton_(fileType) {
    const label = str(this.filterTypeToTranslationKeyMap_.get(fileType));
    const button =
        createChild(this.container_, 'file-type-filter-button', 'cr-button');
    button.textContent = label;
    button.setAttribute('aria-label', label);
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
    const currentFilter = this.recentEntry_.recentFileType ||
        chrome.fileManagerPrivate.RecentFileType.ALL;
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
    // Clear and scan the current directory with the updated Recent setting.
    this.directoryModel_.clearCurrentDirAndScan();
    this.speakA11yMessage(currentFilter, newFilter);
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
      this.allFilterButton_,
      this.audioFilterButton_,
      this.imageFilterButton_,
      this.videoFilterButton_,
    ];
    if (this.documentFilterButton_) {
      buttons.push(this.documentFilterButton_);
    }
    buttons.forEach(button => {
      const fileTypeFilter =
          /** @type {!chrome.fileManagerPrivate.RecentFileType} */ (
              button.getAttribute('file-type-filter'));
      button.classList.toggle('active', currentFilter === fileTypeFilter);
      button.setAttribute(
          'aria-pressed', currentFilter === fileTypeFilter ? 'true' : 'false');
    });
  }
}
