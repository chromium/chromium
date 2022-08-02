// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';
import {State} from '../../externs/ts/state.js';
import {Action, Actions, ChangeDirectoryAction} from '../actions.js';

function getEntry(state: State, action: ChangeDirectoryAction): Entry|
    FilesAppEntry|null {
  const {newDirectory, key} = action;
  if (newDirectory) {
    return newDirectory;
  }

  const entry = state.allEntries[key!] ? state.allEntries[key!]!.entry : null;
  if (!entry) {
    return null;
  }
  return entry;
}

/** Caches the Action's entry in the `allEntries` attribute. */
export function cacheEntries(currentState: State, action: Action): State {
  if (action.type === Actions.CHANGE_DIRECTORY) {
    const {key} = (action as ChangeDirectoryAction);
    const allEntries = currentState.allEntries || {};

    const entry = getEntry(currentState, (action as ChangeDirectoryAction));
    if (!entry) {
      // Nothing to cache, just continue.
      return currentState;
    }

    const entryData = allEntries[key] || {};
    allEntries[key] = Object.assign(entryData, {
      entry: entry,
    });

    currentState.allEntries = allEntries;
  }

  return currentState;
}
