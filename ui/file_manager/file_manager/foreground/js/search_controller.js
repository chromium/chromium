// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Controller for searching.
 */
class SearchController {
  /**
   * @param {!SearchBox} searchBox Search box UI element.
   * @param {!LocationLine} locationLine Location line UI element.
   * @param {!DirectoryModel} directoryModel Directory model.
   * @param {!TaskController} taskController Task controller to execute the
   *     selected item.
   * @param {!FileManagerUI} a11y FileManagerUI to be able to announce a11y
   *     messages.
   */
  constructor(
      searchBox, locationLine, directoryModel, volumeManager, taskController,
      a11y) {
    /** @const @private {!SearchBox} */
    this.searchBox_ = searchBox;

    /** @const @private {!LocationLine} */
    this.locationLine_ = locationLine;

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

    searchBox.addEventListener(
        SearchBox.EventType.TEXT_CHANGE, this.onTextChange_.bind(this));
    searchBox.addEventListener(
        SearchBox.EventType.ITEM_SELECT, this.onItemSelect_.bind(this));
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
    this.searchBox_.clear();
    // Only update visibility if |clear| is called from "directory-changed"
    // event.
    if (opt_event) {
      // My Files currently doesn't implement search so let's hide it.
      const isMyFiles =
          (opt_event.newDirEntry &&
           opt_event.newDirEntry.rootType ===
               VolumeManagerCommon.RootType.MY_FILES);
      this.searchBox_.setHidden(isMyFiles);
    }
  }

  /**
   * Handles text change event.
   * @private
   */
  onTextChange_() {
    const searchString = this.searchBox_.inputElement.value.trimLeft();

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
      this.directoryModel_.search('', () => {}, () => {});
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
    const searchString = this.searchBox_.inputElement.value.trimLeft();
    this.lastAutocompleteQuery_ = searchString;
    if (this.autocompleteSuggestionsBusy_) {
      return;
    }

    // Clear search if the query empty.
    if (!searchString) {
      const msg = str('SEARCH_A11Y_CLEAR_SEARCH');
      this.a11y_.speakA11yMessage(msg);
      this.searchBox_.autocompleteList.suggestions = [];
      return;
    }

    // Add header item.
    const headerItem = /** @type {SearchItem} */ (
        {isHeaderItem: true, searchQuery: searchString});
    if (!this.searchBox_.autocompleteList.dataModel ||
        this.searchBox_.autocompleteList.dataModel.length == 0) {
      this.searchBox_.autocompleteList.suggestions = [headerItem];
    } else {
      // Updates only the head item to prevent a flickering on typing.
      this.searchBox_.autocompleteList.dataModel.splice(0, 1, headerItem);
    }

    // The autocomplete list should be resized and repositioned here as the
    // search box is resized when it's focused.
    this.searchBox_.autocompleteList.syncWidthAndPositionToInput();
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
          this.searchBox_.autocompleteList.suggestions =
              [headerItem].concat(suggestions);
        });
  }

  /**
   * Opens the currently selected suggestion item.
   * @private
   */
  onItemSelect_() {
    const selectedItem = this.searchBox_.autocompleteList.selectedItem;

    // Clear the current auto complete list.
    this.lastAutocompleteQuery_ = '';
    this.searchBox_.autocompleteList.suggestions = [];

    // If the entry is the search item or no entry is selected, just change to
    // the search result.
    if (!selectedItem || selectedItem.isHeaderItem) {
      const query = selectedItem ? selectedItem.searchQuery :
                                   this.searchBox_.inputElement.value;
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
               VolumeManagerCommon.RootType.DRIVE_OTHER)) {
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

      // If the current location is somewhere in Drive, all files in Drive can
      // be listed as search results regardless of current location.
      // In this case, showing current location is confusing, so use the Drive
      // root "My Drive" as the current location.
      if (this.isOnDrive_) {
        const locationInfo = this.currentLocationInfo_;
        const rootEntry = locationInfo.volumeInfo.displayRoot;
        if (rootEntry) {
          this.locationLine_.show(rootEntry);
        }
      }
    };

    const onClearSearch = function() {
      this.locationLine_.show(this.directoryModel_.getCurrentDirEntry());
    };

    this.directoryModel_.search(
        searchString, onSearchRescan.bind(this), onClearSearch.bind(this));
  }
}
