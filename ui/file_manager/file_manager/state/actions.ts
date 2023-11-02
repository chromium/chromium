// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PropStatus} from '../externs/ts/state.js';
import {BaseAction} from '../lib/base_store.js';

import {FileKey} from './file_key.js';

/**
 * Union of all types of Actions in Files app.
 *
 * It enforces the type/enum for the `type` and `payload` attributes.
 * A good explanation of this feature is here:
 * https://mariusschulz.com/blog/tagged-union-types-in-typescript
 */
export type Action =
    ChangeDirectoryAction|ClearStaleCachedEntriesAction|SearchAction;


/** Enum to identify every Action in Files app. */
export const enum ActionType {
  CHANGE_DIRECTORY = 'change-directory',
  CLEAR_STALE_CACHED_ENTRIES = 'clear-stale-cached-entries',
  SEARCH = 'search',
}

/** Action to request to change the Current Directory. */
export interface ChangeDirectoryAction extends BaseAction {
  type: ActionType.CHANGE_DIRECTORY;
  payload: {
    newDirectory?: Entry, key: FileKey, status: PropStatus,
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
  };
}

/** Factory for the ChangeDirectoryAction. */
export function changeDirectory(
    {to, toKey, status}: {to?: Entry, toKey: FileKey, status?: PropStatus}):
    ChangeDirectoryAction {
  return {
    type: ActionType.CHANGE_DIRECTORY,
    payload: {
      newDirectory: to,
      key: toKey ? toKey : to!.toURL(),
      status: status ? status : PropStatus.STARTED,
    },
  };
}

export function searchAction(
    {query: query, status}: {query?: string, status?: PropStatus}):
    SearchAction {
  return {
    type: ActionType.SEARCH,
    payload: {
      query: query,
      status,
    },
  };
}
