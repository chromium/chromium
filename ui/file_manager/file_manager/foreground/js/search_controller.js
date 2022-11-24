// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {str, strf, util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {SEARCH_ITEM_CHANGED, SEARCH_QUERY_CHANGED, SearchContainer} from '../../containers/search_container.js';
import {EntryLocation} from '../../externs/entry_location.js';
import {VolumeManager} from '../../externs/volume_manager.js';

import {DirectoryModel} from './directory_model.js';
import {TaskController} from './task_controller.js';
import {FileManagerUI} from './ui/file_manager_ui.js';
import {SearchItem} from './ui/search_autocomplete_list.js';

/**
 * Controller for searching.
 */
export class SearchController {
  /**
   * @param {!SearchContainer} searchContainer The controller of search
   *     elements.
   * @param {!DirectoryModel} directoryModel Directory model.
   * @param {!VolumeManager} volumeManager Volume manager.
   * @param {!TaskController} taskController Task controller to execute the
   *     selected item.
   * @param {!FileManagerUI} a11y FileManagerUI to be able to announce a11y
   *     messages.
   */
  constructor(
      searchContainer, directoryModel, volumeManager, taskController, a11y) {
    /** @const @private {!SearchContainer} */
    this.searchContainer_ = searchContainer;

    /** @const @private {!DirectoryModel} */
    this.directoryModel_ = directoryModel;

    /** @const @private {!VolumeManager} */
    this.volumeManager_ = volumeManager;

    /** @const @private {!TaskController} */
    this.taskController_ = taskController;

    /** @private {string} */
    this.lastAutocompleteQuery_ = '';

    /** @private {boolean} */
    this.autocompleteSuggestionsBusy_ = false;

    /** @const @private {!FileManagerUI} */
    this.a11y_ = a11y;

    searchContainer.addEventListener(SEARCH_QUERY_CHANGED, (event) => {
      this.onTextChange_(event.detail.query);
    });
    searchContainer.addEventListener(
        SEARCH_ITEM_CHANGED, this.onItemSelect_.bind(this));
    directoryModel.addEventListener('directory-changed', this.clear.bind(this));
  }

  /**
   * Obtains current directory's locaiton info.
   * @type {EntryLocation}
   * @private
   */
  get currentLocationInfo_() {
    const entry = this.directoryModel_.getCurrentDirEntry();
    return entry && this.volumeManager_.getLocationInfo(entry);
  }

  /**
   * Whether the current directory is on drive or not.
   * @private
   */
  get isOnDrive_() {
    const currentLocationInfo = this.currentLocationInfo_;
    return currentLocationInfo && currentLocationInfo.isDriveBased;
  }

  /**
   * Clears the search state.
   * @param {Event=} opt_event when called from "directory-changed" event.
   */
  clear(opt_event) {
    this.directoryModel_.clearLastSearchQuery();
    // If the call to clear was caused by the startup code we do not clear
    // the search container. This is to prevent the search container from
    // hiding launch parameter search query. We detect the app start up
    // condition by checking that the previous directory entry was not set.
    if (opt_event && opt_event.previousDirEntry !== null) {
      this.searchContainer_.clear();
    }
    // Only update visibility if |clear| is called from "directory-changed"
    // event.
    if (opt_event) {
      // My Files currently doesn't implement search so let's hide it.
      const isMyFiles =
          (opt_event.newDirEntry &&
           opt_event.newDirEntry.rootType ===
               VolumeManagerCommon.RootType.MY_FILES);
      this.searchContainer_.setHidden(isMyFiles);
    }
  }

  /**
   * Sets search query on the search box and performs a search.
   * @param {string} searchQuery Search query string to be searched with.
   */
  setSearchQuery(searchQuery) {
    this.searchContainer_.setQuery(searchQuery);
    this.onTextChange_(searchQuery);
    if (this.isOnDrive_) {
      this.onItemSelect_();
    }
  }

  /**
   * Handles text change event.
   * @param {string} query
   * @private
   */
  onTextChange_(query) {
    const searchString = query;

    // On drive, incremental search is not invoked since we have an auto-
    // complete suggestion instead.
    if (!this.isOnDrive_) {
      this.search_(searchString);
      return;
    }

    // When the search text is changed, finishes the search and showes back
    // the last directory by passing an empty string to
    // {@code DirectoryModel.search()}.
    if (this.directoryModel_.isSearching() &&
        this.directoryModel_.getLastSearchQuery() != searchString) {
      this.directoryModel_.search('', () => {});
    }

    this.requestAutocompleteSuggestions_();
  }

  /**
   * Updates autocompletion items.
   * @private
   */
  requestAutocompleteSuggestions_() {
    // Remember the most recent query. If there is an other request in progress,
    // then it's result will be discarded and it will call a new request for
    // this query.
    const searchString = this.searchContainer_.getQuery();
    this.lastAutocompleteQuery_ = searchString;
    if (this.autocompleteSuggestionsBusy_) {
      return;
    }

    // Clear search if the query empty.
    if (!searchString) {
      const msg = str('SEARCH_A11Y_CLEAR_SEARCH');
      this.a11y_.speakA11yMessage(msg);
      this.searchContainer_.clearSuggestions();
      return;
    }

    // Add header item.
    const headerItem = /** @type {SearchItem} */ (
        {isHeaderItem: true, searchQuery: searchString});
    this.searchContainer_.setHeaderItem(headerItem);

    this.autocompleteSuggestionsBusy_ = true;

    chrome.fileManagerPrivate.searchDriveMetadata(
        {
          query: searchString,
          types: chrome.fileManagerPrivate.SearchType.ALL,
          maxResults: 4,
        },
        suggestions => {
          this.autocompleteSuggestionsBusy_ = false;

          // Discard results for previous requests and fire a new search
          // for the most recent query.
          if (searchString != this.lastAutocompleteQuery_) {
            this.requestAutocompleteSuggestions_();
            return;
          }

          // Keeps the items in the suggestion list.
          this.searchContainer_.setSuggestions(
              [headerItem].concat(suggestions));
        });
  }

  /**
   * Opens the currently selected suggestion item.
   * @private
   */
  onItemSelect_() {
    const selectedItem = this.searchContainer_.getSelectedItem();

    // Clear the current auto complete list.
    this.lastAutocompleteQuery_ = '';
    this.searchContainer_.clearSuggestions();

    // If the entry is the search item or no entry is selected, just change to
    // the search result.
    if (!selectedItem || selectedItem.isHeaderItem) {
      const query = selectedItem ? selectedItem.searchQuery :
                                   this.searchContainer_.getQuery();
      this.search_(query);
      return;
    }

    // Clear the search box if an item except for the search item is
    // selected. Eventually the following directory change clears the search
    // box, but if the selected item is located just under /drive/other, the
    // current directory will not changed. For handling the case, and for
    // improving response time, clear the text manually here.
    this.clear();

    // If the entry is a directory, just change the directory.
    const entry = selectedItem.entry;
    if (entry.isDirectory) {
      this.directoryModel_.changeDirectoryEntry(entry);
      return;
    }

    // Change the current directory to the directory that contains the
    // selected file. Note that this is necessary for an image or a video,
    // which should be opened in the gallery mode, as the gallery mode
    // requires the entry to be in the current directory model. For
    // consistency, the current directory is always changed regardless of
    // the file type.
    entry.getParent(parentEntry => {
      // Check if the parent entry points /drive/other or not.
      // If so it just opens the file.
      const locationInfo = this.volumeManager_.getLocationInfo(parentEntry);
      if (!locationInfo ||
          (locationInfo.isRootEntry &&
           locationInfo.rootType ===
               VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME)) {
        this.taskController_.executeEntryTask(entry);
        return;
      }
      // If the parent entry can be /drive/other.
      this.directoryModel_.changeDirectoryEntry(parentEntry, () => {
        this.directoryModel_.selectEntry(entry);
        this.taskController_.executeEntryTask(entry);
      });
    });
  }

  /**
   * Search files and update the list with the search result.
   * @param {string} searchString String to be searched with.
   * @private
   */
  search_(searchString) {
    if (!searchString) {
      const msg = str('SEARCH_A11Y_CLEAR_SEARCH');
      this.a11y_.speakA11yMessage(msg);
    }
    const onSearchRescan = function() {
      const fileList = this.directoryModel_.getFileList();
      const count = fileList.getFileCount() + fileList.getFolderCount();
      const msgId =
          count === 0 ? 'SEARCH_A11Y_NO_RESULT' : 'SEARCH_A11Y_RESULT';
      const msg = strf(msgId, searchString);
      this.a11y_.speakA11yMessage(msg);
    };

    this.directoryModel_.search(searchString, onSearchRescan.bind(this));
  }
}
