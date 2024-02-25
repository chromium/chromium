// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';

import {createChild} from '../../common/js/dom_utils.js';
import {isSameEntry} from '../../common/js/entry_utils.js';
import type {FakeEntry} from '../../common/js/files_app_entry_types.js';
import {recordEnum} from '../../common/js/metrics.js';
import {str, strf} from '../../common/js/translations.js';
import type {State} from '../../state/state.js';
import {getStore} from '../../state/store.js';

import type {DirectoryChangeEvent, DirectoryModel} from './directory_model.js';
import type {A11yAnnounce} from './ui/a11y_announce.js';

/**
 * This class controls wires file-type filter UI and the filter settings in
 * Recents view.
 */
export class FileTypeFiltersController {
  private readonly filterTypeToTranslationKeyMap_ =
      new Map<chrome.fileManagerPrivate.FileCategory, string>([
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
  private readonly allFilterButton_: CrButtonElement;
  private readonly audioFilterButton_: CrButtonElement;
  private readonly documentFilterButton_: CrButtonElement;
  private readonly imageFilterButton_: CrButtonElement;
  private readonly videoFilterButton_: CrButtonElement;

  private inRecent_: boolean = false;

  constructor(
      private readonly container_: HTMLElement,
      private readonly directoryModel_: DirectoryModel,
      private readonly recentEntry_: FakeEntry,
      private readonly a11y_: A11yAnnounce) {
    this.allFilterButton_ =
        this.createFilterButton_(chrome.fileManagerPrivate.FileCategory.ALL);
    this.audioFilterButton_ =
        this.createFilterButton_(chrome.fileManagerPrivate.FileCategory.AUDIO);
    this.documentFilterButton_ = this.createFilterButton_(
        chrome.fileManagerPrivate.FileCategory.DOCUMENT);
    this.imageFilterButton_ =
        this.createFilterButton_(chrome.fileManagerPrivate.FileCategory.IMAGE);
    this.videoFilterButton_ =
        this.createFilterButton_(chrome.fileManagerPrivate.FileCategory.VIDEO);

    this.directoryModel_.addEventListener(
        'directory-changed', this.onCurrentDirectoryChanged_.bind(this));

    this.updateButtonActiveStates_();

    getStore().subscribe(this);
  }


  /** @param state latest state from the store. */
  onStateChanged(state: State) {
    if (this.inRecent_) {
      const search = state.search;
      this.container_.hidden = !!(search?.query);
    }
  }


  /**
   * @param fileCategory File category
   *     filter which needs to be recorded.
   */
  private recordFileCategoryFilterUma_(
      fileCategory: chrome.fileManagerPrivate.FileCategory) {
    /**
     * Keep the order of this in sync with FileManagerRecentFilterType in
     * tools/metrics/histograms/enums.xml.
     * The array indices will be recorded in UMA as enum values. The index for
     * each filter type should never be renumbered nor reused in this array.
     */
    const FileTypeFiltersForUMA: chrome.fileManagerPrivate.FileCategory[] = ([
      chrome.fileManagerPrivate.FileCategory.ALL,       // 0
      chrome.fileManagerPrivate.FileCategory.AUDIO,     // 1
      chrome.fileManagerPrivate.FileCategory.IMAGE,     // 2
      chrome.fileManagerPrivate.FileCategory.VIDEO,     // 3
      chrome.fileManagerPrivate.FileCategory.DOCUMENT,  // 4
    ]);
    Object.freeze(FileTypeFiltersForUMA);
    recordEnum('Recent.FilterByType', fileCategory, FileTypeFiltersForUMA);
  }

  /**
   * Speak voice message in screen recording mode depends on the existing
   * filter and the new filter type.
   *
   */
  speakA11yMessage(
      currentFilter: chrome.fileManagerPrivate.FileCategory,
      newFilter: chrome.fileManagerPrivate.FileCategory) {
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
        str(this.filterTypeToTranslationKeyMap_.get(currentFilter)!));
    let onMessage = strf(
        'RECENT_VIEW_FILTER_ON',
        str(this.filterTypeToTranslationKeyMap_.get(newFilter)!));
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
   * @param fileCategory File category
   *     for the filter button.
   */
  private createFilterButton_(fileCategory:
                                  chrome.fileManagerPrivate.FileCategory) {
    const label = str(this.filterTypeToTranslationKeyMap_.get(fileCategory)!);
    const button =
        createChild(this.container_, 'file-type-filter-button', 'cr-button') as
        CrButtonElement;
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
   * @param event Event.
   */
  private onCurrentDirectoryChanged_(event: DirectoryChangeEvent) {
    const directoryChangeEvent = event;
    const isEnteringRecentEntry =
        isSameEntry(directoryChangeEvent.detail.newDirEntry, this.recentEntry_);
    const isLeavingRecentEntry = !isEnteringRecentEntry &&
        isSameEntry(
            directoryChangeEvent.detail.previousDirEntry, this.recentEntry_);
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
   * @param event Event.
   */
  private onFilterButtonClicked_(event: Event) {
    const target = event.target as CrButtonElement;
    const isButtonActive = target.classList.contains('active');
    const buttonFilter = target.getAttribute('file-type-filter') as
        chrome.fileManagerPrivate.FileCategory;
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
    this.recordFileCategoryFilterUma_(newFilter);
  }

  /**
   * Update the filter button active states based on current `fileCategory`.
   * Every time `fileCategory` is changed (including the initialization),
   * this method needs to be called to render the UI to reflect the file
   * type filter change.
   */
  private updateButtonActiveStates_() {
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
      const fileCategoryFilter = button.getAttribute('file-type-filter') as
          chrome.fileManagerPrivate.FileCategory;
      button.classList.toggle('active', currentFilter === fileCategoryFilter);
      button.setAttribute(
          'aria-pressed',
          currentFilter === fileCategoryFilter ? 'true' : 'false');
    });
  }
}
