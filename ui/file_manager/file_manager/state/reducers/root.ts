// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {State} from '../../externs/ts/state.js';
import {Action, ActionType} from '../actions.js';
import {androidAppsReducersMap} from '../ducks/android_apps.js';
import {bulkPinningReducersMap} from '../ducks/bulk_pinning.js';
import {folderShortcutsReducersMap} from '../ducks/folder_shortcuts.js';
import {navigationReducersMap} from '../ducks/navigation.js';
import {searchReducersMap} from '../ducks/search.js';
import {uiEntriesReducersMap} from '../ducks/ui_entries.js';
import {volumesReducersMap} from '../ducks/volumes.js';

import {addChildEntries, cacheEntries, clearCachedEntries, updateMetadata} from './all_entries.js';
import {changeDirectory, updateDirectoryContent, updateFileTasks, updateSelection} from './current_directory.js';
import {updatePreferences} from './preferences.js';

// Reducers map created from merging together each slice's exported reducersMap.
const rootReducersMap = new Map([
  ...searchReducersMap,
  ...volumesReducersMap,
  ...bulkPinningReducersMap,
  ...uiEntriesReducersMap,
  ...androidAppsReducersMap,
  ...folderShortcutsReducersMap,
  ...navigationReducersMap,
]);

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

  if (window.DEBUG_STORE) {
    console.groupCollapsed(`Action: ${action.type}`);
    console.dir(action.payload);
    console.groupEnd();
  }

  switch (action.type) {
    case ActionType.CHANGE_DIRECTORY:
      return changeDirectory(state, action);
    case ActionType.CHANGE_SELECTION:
      return updateSelection(state, action);
    case ActionType.CHANGE_FILE_TASKS:
      return updateFileTasks(state, action);
    case ActionType.CLEAR_STALE_CACHED_ENTRIES:
      return clearCachedEntries(state, action);
    case ActionType.UPDATE_DIRECTORY_CONTENT:
      return updateDirectoryContent(state, action);
    case ActionType.UPDATE_METADATA:
      return updateMetadata(state, action);
    case ActionType.ADD_CHILD_ENTRIES:
      return addChildEntries(currentState, action);
    case ActionType.UPDATE_PREFERENCES:
      return updatePreferences(currentState, action);
    default:
      // Handles ducks reducers.
      const reducers = rootReducersMap.get(action.type);
      if (!reducers) {
        console.error(`No registered reducers for action: ${action.type}`);
        return currentState;
      }
      return reducers.reduce(
          (state, reducer) => reducer(state, action.payload), currentState);
  }
}
