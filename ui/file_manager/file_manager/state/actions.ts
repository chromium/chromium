// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PropStatus, SearchOptions} from '../externs/ts/state.js';
import {BaseAction} from '../lib/base_store.js';

import {ClearStaleCachedEntriesAction} from './actions/all_entries.js';
import {ChangeDirectoryAction, ChangeSelectionAction} from './actions/current_directory.js';

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


/** Action to update the search state. */
export interface SearchAction extends BaseAction {
  type: ActionType.SEARCH;
  payload: {
    query?: string,
    status?: PropStatus,
    options?: SearchOptions,
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
