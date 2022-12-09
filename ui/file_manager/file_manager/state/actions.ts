// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SearchData} from '../externs/ts/state.js';
import {BaseAction} from '../lib/base_store.js';

import {ClearStaleCachedEntriesAction} from './actions/all_entries.js';
import {ChangeDirectoryAction, ChangeFileTasksAction, ChangeSelectionAction} from './actions/current_directory.js';

/**
 * Union of all types of Actions in Files app.
 *
 * It enforces the type/enum for the `type` and `payload` attributes.
 * A good explanation of this feature is here:
 * https://mariusschulz.com/blog/tagged-union-types-in-typescript
 */
export type Action = ChangeDirectoryAction|ChangeSelectionAction|
    ChangeFileTasksAction|ClearStaleCachedEntriesAction|SearchAction;


/** Enum to identify every Action in Files app. */
export const enum ActionType {
  CHANGE_DIRECTORY = 'change-directory',
  CHANGE_SELECTION = 'change-selection',
  CHANGE_FILE_TASKS = 'change-file-tasks',
  CLEAR_STALE_CACHED_ENTRIES = 'clear-stale-cached-entries',
  SEARCH = 'search',
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
