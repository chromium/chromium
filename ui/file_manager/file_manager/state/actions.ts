// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FilesAppDirEntry, FilesAppEntry} from '../externs/files_app_entry_interfaces.js';
import {PropStatus, SearchOptions} from '../externs/ts/state.js';
import {BaseAction} from '../lib/base_store.js';

import {FileKey} from './file_key.js';

/**
 * Union of all types of Actions in Files app.
 *
 * It enforces the type/enum for the `type` and `payload` attributes.
 * A good explanation of this feature is here:
 * https://mariusschulz.com/blog/tagged-union-types-in-typescript
 */
export type Action = ChangeDirectoryAction|ChangeSelectionAction|
    ClearStaleCachedEntriesAction|SearchAction;


/** Enum to identify every Action in Files app. */
export const enum ActionType {
  CHANGE_DIRECTORY = 'change-directory',
  CHANGE_SELECTION = 'change-selection',
  CLEAR_STALE_CACHED_ENTRIES = 'clear-stale-cached-entries',
  SEARCH = 'search',
}

/** Action to request to change the Current Directory. */
export interface ChangeDirectoryAction extends BaseAction {
  type: ActionType.CHANGE_DIRECTORY;
  payload: {
    newDirectory?: DirectoryEntry|FilesAppDirEntry,
                key: FileKey,
                status: PropStatus,
  };
}

/** Action to update the currently selected files/folders. */
export interface ChangeSelectionAction extends BaseAction {
  type: ActionType.CHANGE_SELECTION;
  payload: {
    selectedKeys: FileKey[],
    entries: Array<Entry|FilesAppEntry>,
  };
}

export interface ClearStaleCachedEntriesAction extends BaseAction {
  type: ActionType.CLEAR_STALE_CACHED_ENTRIES;
  payload?: undefined;
}

/** Action to update the search state. */
export interface SearchAction extends BaseAction {
  type: ActionType.SEARCH;
  payload: {
    query?: string,
    status?: PropStatus,
    options?: SearchOptions,
  };
}

/** Factory for the ChangeDirectoryAction. */
export function changeDirectory({to, toKey, status}: {
  to?: DirectoryEntry|FilesAppDirEntry, toKey: FileKey,
  status?: PropStatus,
}): ChangeDirectoryAction {
  return {
    type: ActionType.CHANGE_DIRECTORY,
    payload: {
      newDirectory: to,
      key: toKey ? toKey : to!.toURL(),
      status: status ? status : PropStatus.STARTED,
    },
  };
}

/** Factory for the ChangeSelectionAction. */
export function updateSelection(payload: ChangeSelectionAction['payload']):
    ChangeSelectionAction {
  return {
    type: ActionType.CHANGE_SELECTION,
    payload,
  };
}

export function searchAction(
    {query, status, options}:
        {query?: string, status?: PropStatus, options?: SearchOptions}):
    SearchAction {
  return {
    type: ActionType.SEARCH,
    payload: {
      query: query,
      status,
      options,
    },
  };
}
