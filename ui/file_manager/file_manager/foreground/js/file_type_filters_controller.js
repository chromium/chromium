// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {createChild} from '../../common/js/dom_utils.js';
import {metrics} from '../../common/js/metrics.js';
import {str, strf, util} from '../../common/js/util.js';
import {DirectoryChangeEvent} from '../../externs/directory_change_event.js';
import {FakeEntry} from '../../externs/files_app_entry_interfaces.js';
import {State} from '../../externs/ts/state.js';
import {getStore} from '../../state/store.js';

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
     * @private {Map<chrome.fileManagerPrivate.FileCategory, string>}
     * @const
     */
    this.filterTypeToTranslationKeyMap_ = new Map([
      [
        chrome.fileManagerPrivate.FileCategory.ALL,
        'MEDIA_VIEW_ALL_ROOT_LABEL',
      ],
      [
        chrome.fileManagerPrivate.FileCategory.AUDIO,
        'MEDIA_VIEW_AUDIO_ROOT_LABEL',
      ],
      [
        chrome.fileManagerPrivate.FileCategory.IMAGE,
        'MEDIA_VIEW_IMAGES_ROOT_LABEL',
      ],
      [
        chrome.fileManagerPrivate.FileCategory.VIDEO,
        'MEDIA_VIEW_VIDEOS_ROOT_LABEL',
      ],
      [
        chrome.fileManagerPrivate.FileCategory.DOCUMENT,
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
        this.createFilterButton_(chrome.fileManagerPrivate.FileCategory.ALL);

    /**
     * @private {!HTMLElement}
     * @const
     */
    this.audioFilterButton_ =
        this.createFilterButton_(chrome.fileManagerPrivate.FileCategory.AUDIO);

    /**
     * @private {!HTMLElement|null}
     * @const
     */
    this.documentFilterButton_ = this.createFilterButton_(
        chrome.fileManagerPrivate.FileCategory.DOCUMENT);

    /**
     * @private {!HTMLElement}
     * @const
     */
    this.imageFilterButton_ =
        this.createFilterButton_(chrome.fileManagerPrivate.FileCategory.IMAGE);

    /**
     * @private {!HTMLElement}
     * @const
     */
    this.videoFilterButton_ =
        this.createFilterButton_(chrome.fileManagerPrivate.FileCategory.VIDEO);

    this.directoryModel_.addEventListener(
        'directory-changed', this.onCurrentDirectoryChanged_.bind(this));

    this.updateButtonActiveStates_();

    /**
     * @private {boolean}
     */
    this.inRecent_ = false;

    if (util.isSearchV2Enabled()) {
      getStore().subscribe(this);
    }
  }


  /** @param {!State} state latest state from the store. */
  onStateChanged(state) {
    if (util.isSearchV2Enabled()) {
      if (this.inRecent_) {
        const search = state.search;
        this.container_.hidden = !!(search?.query);
      }
    }
  }


  /**
   * @param {chrome.fileManagerPrivate.FileCategory} fileCategory File category
   *     filter which needs to be recorded.
   * @private
   */
  recordFileCategoryFilterUMA_(fileCategory) {
    /**
     * Keep the order of this in sync with FileManagerRecentFilterType in
     * tools/metrics/histograms/enums.xml.
     * The array indices will be recorded in UMA as enum values. The index for
     * each filter type should never be renumbered nor reused in this array.
     */
    const FileTypeFiltersForUMA =
        /** @type {!Array<!chrome.fileManagerPrivate.FileCategory>} */ ([
          chrome.fileManagerPrivate.FileCategory.ALL,       // 0
          chrome.fileManagerPrivate.FileCategory.AUDIO,     // 1
          chrome.fileManagerPrivate.FileCategory.IMAGE,     // 2
          chrome.fileManagerPrivate.FileCategory.VIDEO,     // 3
          chrome.fileManagerPrivate.FileCategory.DOCUMENT,  // 4
        ]);
    Object.freeze(FileTypeFiltersForUMA);
    metrics.recordEnum(
        'Recent.FilterByType', fileCategory, FileTypeFiltersForUMA);
  }

  /**
   * Speak voice message in screen recording mode depends on the existing
   * filter and the new filter type.
   *
   * @param {!chrome.fileManagerPrivate.FileCategory} currentFilter
   * @param {!chrome.fileManagerPrivate.FileCategory} newFilter
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
        currentFilter === chrome.fileManagerPrivate.FileCategory.ALL;
    const isFromOthersToAll =
        newFilter === chrome.fileManagerPrivate.FileCategory.ALL;
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
   * @param {!chrome.fileManagerPrivate.FileCategory} fileCategory File category
   *     for the filter button.
   * @private
   */
  createFilterButton_(fileCategory) {
    const label = str(this.filterTypeToTranslationKeyMap_.get(fileCategory));
    const button =
        createChild(this.container_, 'file-type-filter-button', 'cr-button');
    button.textContent = label;
    button.setAttribute('aria-label', label);
    // Store the "FileCategory" on the button element so we know the mapping
    // between the DOM element and its corresponding "FileCategory", which
    // will make it easier to trigger UI change based on "FileCategory" or
    // vice versa.
    button.setAttribute('file-type-filter', fileCategory);
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
      this.recentEntry_.fileCategory =
          chrome.fileManagerPrivate.FileCategory.ALL;
      this.updateButtonActiveStates_();
    }
    this.inRecent_ = isEnteringRecentEntry;
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
        /** @type {!chrome.fileManagerPrivate.FileCategory} */
        (target.getAttribute('file-type-filter'));
    // Do nothing if "All" button is active and being clicked again.
    if (isButtonActive &&
        buttonFilter === chrome.fileManagerPrivate.FileCategory.ALL) {
      return;
    }
    const currentFilter = this.recentEntry_.fileCategory ||
        chrome.fileManagerPrivate.FileCategory.ALL;
    // Clicking an active button will make it inactive and make "All"
    // button active.
    const newFilter = isButtonActive ?
        chrome.fileManagerPrivate.FileCategory.ALL :
        buttonFilter;
    this.recentEntry_.fileCategory = newFilter;
    this.updateButtonActiveStates_();
    if (isButtonActive) {
      this.allFilterButton_.focus();
    }
    // Clear and scan the current directory with the updated Recent setting.
    this.directoryModel_.clearCurrentDirAndScan();
    this.speakA11yMessage(currentFilter, newFilter);
    this.recordFileCategoryFilterUMA_(newFilter);
  }

  /**
   * Update the filter button active states based on current `fileCategory`.
   * Every time `fileCategory` is changed (including the initialization),
   * this method needs to be called to render the UI to reflect the file
   * type filter change.
   * @private
   */
  updateButtonActiveStates_() {
    const currentFilter = this.recentEntry_.fileCategory;
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
      const fileCategoryFilter =
          /** @type {!chrome.fileManagerPrivate.FileCategory} */ (
              button.getAttribute('file-type-filter'));
      button.classList.toggle('active', currentFilter === fileCategoryFilter);
      button.setAttribute(
          'aria-pressed',
          currentFilter === fileCategoryFilter ? 'true' : 'false');
    });
  }
}
