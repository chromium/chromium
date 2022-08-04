// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {State} from '../../externs/ts/state.js';
import {Action, Actions, ChangeDirectoryAction} from '../actions.js';

import {cacheEntries, clearCachedEntries} from './all_entries.js';
import {changeDirectory} from './current_directory.js';

/**
 * Root reducer for Files app.
 * It dispatches to other reducers to update different parts of the State.
 */
export function rootReducer(currentState: State, action: Action): State {
  // Before any actual Reducer, we cache the entries, so the reducers can just
  // use any entry from `allEntries`.
  const state = cacheEntries(currentState, action);

  switch (action.type) {
    case Actions.CHANGE_DIRECTORY:
      return Object.assign(state, {
        currentDirectory:
            changeDirectory(state, action as ChangeDirectoryAction),
      });

    case Actions.CLEAR_STALE_CACHED_ENTRIES:
      return clearCachedEntries(state, action);

    default:
      console.error(`invalid action: ${action.type}`);
      return state;
  }
}
