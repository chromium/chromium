// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {State} from '../../externs/ts/state.js';
import {Action, ActionType} from '../actions.js';

import {addChildEntries, cacheEntries, clearCachedEntries, updateMetadata} from './all_entries.js';
import {addAndroidApps} from './android_apps.js';
import {updateBulkPinning} from './bulk_pinning.js';
import {changeDirectory, updateDirectoryContent, updateFileTasks, updateSelection} from './current_directory.js';
import {addFolderShortcut, refreshFolderShortcut, removeFolderShortcut} from './folder_shortcuts.js';
import {refreshNavigationRoots, updateNavigationEntry} from './navigation.js';
import {updatePreferences} from './preferences.js';
import {search} from './search.js';
import {addUiEntry, removeUiEntry} from './ui_entries.js';
import {addVolume, removeVolume, updateIsInteractiveVolume} from './volumes.js';

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
    case ActionType.SEARCH:
      return search(state, action);
    case ActionType.UPDATE_DIRECTORY_CONTENT:
      return updateDirectoryContent(state, action);
    case ActionType.UPDATE_METADATA:
      return updateMetadata(state, action);
    case ActionType.ADD_VOLUME:
      return addVolume(currentState, action);
    case ActionType.REMOVE_VOLUME:
      return removeVolume(currentState, action);
    case ActionType.REFRESH_NAVIGATION_ROOTS:
      return refreshNavigationRoots(currentState, action);
    case ActionType.UPDATE_NAVIGATION_ENTRY:
      return updateNavigationEntry(currentState, action);
    case ActionType.ADD_UI_ENTRY:
      return addUiEntry(currentState, action);
    case ActionType.REMOVE_UI_ENTRY:
      return removeUiEntry(currentState, action);
    case ActionType.REFRESH_FOLDER_SHORTCUT:
      return refreshFolderShortcut(currentState, action);
    case ActionType.ADD_FOLDER_SHORTCUT:
      return addFolderShortcut(currentState, action);
    case ActionType.REMOVE_FOLDER_SHORTCUT:
      return removeFolderShortcut(currentState, action);
    case ActionType.ADD_ANDROID_APPS:
      return addAndroidApps(currentState, action);
    case ActionType.ADD_CHILD_ENTRIES:
      return addChildEntries(currentState, action);
    case ActionType.UPDATE_BULK_PIN_PROGRESS:
      return updateBulkPinning(currentState, action);
    case ActionType.UPDATE_PREFERENCES:
      return updatePreferences(currentState, action);
    case ActionType.UPDATE_IS_INTERACTIVE_VOLUME:
      return updateIsInteractiveVolume(currentState, action);
    default:
      console.error(`invalid action type: ${(action as any)?.type} action: ${
          JSON.stringify(action)}`);
      return state;
  }
}
