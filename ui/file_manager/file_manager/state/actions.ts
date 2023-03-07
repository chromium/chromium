// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SearchData} from '../externs/ts/state.js';
import {BaseAction} from '../lib/base_store.js';

import {AddChildEntriesAction, ClearStaleCachedEntriesAction, UpdateMetadataAction} from './actions/all_entries.js';
import {AddAndroidAppsAction} from './actions/android_apps.js';
import {ChangeDirectoryAction, ChangeFileTasksAction, ChangeSelectionAction, UpdateDirectoryContentAction} from './actions/current_directory.js';
import {AddFolderShortcutAction, RefreshFolderShortcutAction, RemoveFolderShortcutAction} from './actions/folder_shortcuts.js';
import {RefreshNavigationRootsAction, UpdateNavigationEntryAction} from './actions/navigation.js';
import {AddUiEntryAction, RemoveUiEntryAction} from './actions/ui_entries.js';
import {AddVolumeAction, RemoveVolumeAction} from './actions/volumes.js';

/**
 * Union of all types of Actions in Files app.
 *
 * It enforces the type/enum for the `type` and `payload` attributes.
 * A good explanation of this feature is here:
 * https://mariusschulz.com/blog/tagged-union-types-in-typescript
 */
export type Action = AddVolumeAction|RemoveVolumeAction|
    RefreshNavigationRootsAction|ChangeDirectoryAction|ChangeSelectionAction|
    ChangeFileTasksAction|ClearStaleCachedEntriesAction|SearchAction|
    AddUiEntryAction|RemoveUiEntryAction|UpdateDirectoryContentAction|
    UpdateMetadataAction|RefreshFolderShortcutAction|AddFolderShortcutAction|
    RemoveFolderShortcutAction|AddAndroidAppsAction|AddChildEntriesAction|
    UpdateNavigationEntryAction;


/** Enum to identify every Action in Files app. */
export const enum ActionType {
  ADD_VOLUME = 'add-volume',
  REMOVE_VOLUME = 'remove-volume',
  ADD_UI_ENTRY = 'add-ui-entry',
  REMOVE_UI_ENTRY = 'remove-ui-entry',
  REFRESH_FOLDER_SHORTCUT = 'refresh-folder-shortcut',
  ADD_FOLDER_SHORTCUT = 'add-folder-shortcut',
  REMOVE_FOLDER_SHORTCUT = 'remove-folder-shortcut',
  ADD_ANDROID_APPS = 'add-android-apps',
  REFRESH_NAVIGATION_ROOTS = 'refresh-navigation-roots',
  UPDATE_NAVIGATION_ENTRY = 'update-navigation-entry',
  CHANGE_DIRECTORY = 'change-directory',
  CHANGE_SELECTION = 'change-selection',
  CHANGE_FILE_TASKS = 'change-file-tasks',
  CLEAR_STALE_CACHED_ENTRIES = 'clear-stale-cached-entries',
  SEARCH = 'search',
  UPDATE_DIRECTORY_CONTENT = 'update-directory-content',
  UPDATE_METADATA = 'update-metadata',
  ADD_CHILD_ENTRIES = 'add-child-entries',
}


/** Action to update the search state. */
export interface SearchAction extends BaseAction {
  type: ActionType.SEARCH;
  payload: SearchData;
}

/**
 * Generates a search action based on the supplied data.
 * Query, status and options can be adjusted independently of each other.
 */
export function updateSearch(data: SearchData): SearchAction {
  return {
    type: ActionType.SEARCH,
    payload: {
      query: data.query,
      status: data.status,
      options: data.options,
    },
  };
}

/**
 * Clears all search settings.
 */
export function clearSearch(): SearchAction {
  return {
    type: ActionType.SEARCH,
    payload: {
      query: undefined,
      status: undefined,
      options: undefined,
    },
  };
}
