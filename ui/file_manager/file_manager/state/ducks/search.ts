// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Slice} from '../../lib/base_store.js';
import {type SearchData, SearchLocation, type SearchOptions, SearchRecency, type State} from '../../state/state.js';

/**
 * @fileoverview Search slice of the store.
 */

const slice = new Slice<State, State['search']>('search');
export {slice as searchSlice};

/**
 * Returns if the given search data represents empty (cleared) search.
 */
export function isSearchEmpty(search: SearchData): boolean {
  return Object.values(search).every(f => f === undefined);
}

/**
 * Helper function that does a deep comparison between two SearchOptions.
 */
function optionsChanged(
    stored: SearchOptions|undefined, fresh: SearchOptions|undefined): boolean {
  if (fresh === undefined) {
    // If fresh options are undefined, that means keep the stored options. No
    // matter what the stored options are, we are saying they have not changed.
    return false;
  }
  if (stored === undefined) {
    return true;
  }
  return fresh.location !== stored.location ||
      fresh.recency !== stored.recency ||
      fresh.fileCategory !== stored.fileCategory;
}

const setSearchParameters = slice.addReducer('set', searchReducer);

function searchReducer(state: State, payload: SearchData): State {
  const blankSearch = {
    query: undefined,
    status: undefined,
    options: undefined,
  };
  // Special case: if none of the fields are set, the action clears the search
  // state in the store.
  if (isSearchEmpty(payload)) {
    // Only change the state if the stored value has some defined values.
    if (state.search && !isSearchEmpty(state.search)) {
      return {
        ...state,
        search: blankSearch,
      };
    }
    return state;
  }

  const currentSearch = state.search || blankSearch;
  // Create a clone of current search. We must not modify the original object,
  // as store customers are free to cache it and check for changes. If we modify
  // the original object the check for changes incorrectly return false.
  const search: SearchData = {...currentSearch};
  let changed = false;
  if (payload.query !== undefined && payload.query !== currentSearch.query) {
    search.query = payload.query;
    changed = true;
  }
  if (payload.status !== undefined && payload.status !== currentSearch.status) {
    search.status = payload.status;
    changed = true;
  }
  if (optionsChanged(currentSearch.options, payload.options)) {
    search.options = {...payload.options} as SearchOptions;
    changed = true;
  }
  return changed ? {...state, search} : state;
}

/**
 * Generates a search action based on the supplied data.
 * Query, status and options can be adjusted independently of each other.
 */
export const updateSearch = (data: SearchData) => setSearchParameters({
  query: data.query,
  status: data.status,
  options: data.options,
});

/**
 * Create action to clear all search settings.
 */
export const clearSearch = () => setSearchParameters({
  query: undefined,
  status: undefined,
  options: undefined,
});

/**
 * Search options to be used if the user did not specify their own.
 */
export function getDefaultSearchOptions(): SearchOptions {
  return {
    location: SearchLocation.THIS_FOLDER,
    recency: SearchRecency.ANYTIME,
    fileCategory: chrome.fileManagerPrivate.FileCategory.ALL,
  };
}
