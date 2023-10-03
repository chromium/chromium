// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {str, strf} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {SearchContainer} from '../../containers/search_container.js';
import {SearchOptions, State} from '../../externs/ts/state.js';
import {getDefaultSearchOptions} from '../../state/ducks/search.js';
import {getStore} from '../../state/store.js';

import {DirectoryModel} from './directory_model.js';
import {FileManagerUI} from './ui/file_manager_ui.js';

/**
 * Controller for searching.
 */
export class SearchController {
  /**
   * @param {!SearchContainer} searchContainer The controller of search
   *     elements.
   * @param {!DirectoryModel} directoryModel Directory model.
   * @param {!FileManagerUI} a11y FileManagerUI to be able to announce a11y
   *     messages.
   */
  constructor(searchContainer, directoryModel, a11y) {
    /** @const @private {!SearchContainer} */
    this.searchContainer_ = searchContainer;

    /** @const @private {!DirectoryModel} */
    this.directoryModel_ = directoryModel;

    /** @const @private {!FileManagerUI} */
    this.a11y_ = a11y;

    directoryModel.addEventListener('directory-changed', this.clear.bind(this));

    this.cachedSearchState_ = {};
    this.store_ = getStore();
    this.store_.subscribe(this);
  }

  /**
   * Reacts to state change in the store. This method checks if the search
   * component of the search state changed. If so, it triggers a new search.
   * @param {State} state
   */
  onStateChanged(state) {
    const searchState = state.search;
    if (!searchState) {
      return;
    }
    if (searchState.query === this.cachedSearchState_.query &&
        searchState.options === this.cachedSearchState_.options) {
      return;
    }
    this.cachedSearchState_ = searchState;
    this.onSearchChange_(
        searchState.query || '',
        searchState.options || getDefaultSearchOptions());
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
   * @param {!SearchOptions} options Options to be used when searching.
   */
  setSearchQuery(searchQuery, options) {
    this.searchContainer_.setQuery(searchQuery);
    this.onSearchChange_(searchQuery, options);
  }

  /**
   * Handles text change event.
   * @param {string} query
   * @param {!SearchOptions} options Search options, such as type,
   *    location, etc.
   * @private
   */
  onSearchChange_(query, options) {
    if (!query) {
      const msg = str('SEARCH_A11Y_CLEAR_SEARCH');
      this.a11y_.speakA11yMessage(msg);
    }
    const onSearchRescan = function() {
      const fileList = this.directoryModel_.getFileList();
      const count = fileList.getFileCount() + fileList.getFolderCount();
      const msgId =
          count === 0 ? 'SEARCH_A11Y_NO_RESULT' : 'SEARCH_A11Y_RESULT';
      const msg = strf(msgId, query);
      this.a11y_.speakA11yMessage(msg);
    };

    this.directoryModel_.search(query, options, onSearchRescan.bind(this));
  }
}
