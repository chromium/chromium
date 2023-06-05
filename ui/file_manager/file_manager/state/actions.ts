// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AddChildEntriesAction, ClearStaleCachedEntriesAction, UpdateMetadataAction} from './actions/all_entries.js';
import {AddAndroidAppsAction} from './actions/android_apps.js';
import {UpdateBulkPinProgressAction} from './actions/bulk_pinning.js';
import {ChangeDirectoryAction, ChangeFileTasksAction, ChangeSelectionAction, UpdateDirectoryContentAction} from './actions/current_directory.js';
import {AddFolderShortcutAction, RefreshFolderShortcutAction, RemoveFolderShortcutAction} from './actions/folder_shortcuts.js';
import {RefreshNavigationRootsAction, UpdateNavigationEntryAction} from './actions/navigation.js';
import {UpdatePreferencesAction} from './actions/preferences.js';
import {SearchAction} from './actions/search.js';
import {AddUiEntryAction, RemoveUiEntryAction} from './actions/ui_entries.js';
import {AddVolumeAction, RemoveVolumeAction, UpdateIsInteractiveVolumeAction} from './actions/volumes.js';

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
    UpdateNavigationEntryAction|UpdateBulkPinProgressAction|
    UpdatePreferencesAction|UpdateIsInteractiveVolumeAction;


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
  UPDATE_BULK_PIN_PROGRESS = 'update-bulk-pin-progress',
  UPDATE_PREFERENCES = 'update-preferences',
  UPDATE_IS_INTERACTIVE_VOLUME = 'update-is-interactive-volume',
}
