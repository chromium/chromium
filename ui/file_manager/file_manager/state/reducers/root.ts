// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {State} from '../../externs/ts/state.js';
import {Action, ActionType} from '../actions.js';

import {cacheEntries, clearCachedEntries, updateMetadata} from './all_entries.js';
import {changeDirectory, updateDirectoryContent, updateFileTasks, updateSelection} from './current_directory.js';
import {search} from './search.js';

/**
 * Root reducer for the State for Files app.
 * It dispatches the ` action` to other reducers that update each part of the
 * State.
 *
 * Every top-level attribute of the State should have its reducer being called
 * from here.
 */
export function rootReducer(currentState: State, action: Action): State {
  // Before any actual Reducer, we cache the entries, so the reducers can just
  // use any entry from `allEntries`.
  const state = cacheEntries(currentState, action);

  switch (action.type) {
    case ActionType.CHANGE_DIRECTORY:
      return changeDirectory(state, action);
    case ActionType.CHANGE_SELECTION:
      return updateSelection(state, action);
    case ActionType.CHANGE_FILE_TASKS:
      return updateFileTasks(state, action);
    case ActionType.CLEAR_STALE_CACHED_ENTRIES:
      return clearCachedEntries(state, action);
    case ActionType.SEARCH:
      return search(state, action);
    case ActionType.UPDATE_DIRECTORY_CONTENT:
      return updateDirectoryContent(state, action);
    case ActionType.UPDATE_METADATA:
      return updateMetadata(state, action);
    default:
      console.error(`invalid action: ${action}`);
      return state;
  }
}
