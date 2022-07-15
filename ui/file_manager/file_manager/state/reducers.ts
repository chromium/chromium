// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FilesAppEntry} from '../externs/files_app_entry_interfaces.js';
import {CurrentDirectory, State} from '../externs/ts/state.js';
import {PathComponent} from '../foreground/js/path_component.js';

import {Action, Actions, ChangeDirectoryAction} from './actions.js';

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

    default:
      console.error(`invalid action: ${action.type}`);
      return state;
  }
}

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

/** Reducer that updates the `currentDirectory` attributes. */
export function changeDirectory(
    currentState: State, action: ChangeDirectoryAction): CurrentDirectory|null {
  const fileData = currentState.allEntries[action.key];
  if (!fileData) {
    // The new directory might not be in the allEntries yet.
    return {
      key: action.key,
      status: action.status,
      pathComponents: [],
    };
  }

  // TODO(lucmult): Find a correct way to grab the VolumeManager.
  const volumeManager = window.fileManager.volumeManager;
  if (!volumeManager) {
    console.debug(`VolumeManager not available yet.`);
    return currentState.currentDirectory || null;
  }

  const components =
      PathComponent.computeComponentsFromEntry(fileData.entry, volumeManager);

  return Object.assign(currentState.currentDirectory || {}, {
    status: action.status,
    key: (action as ChangeDirectoryAction).key,
    pathComponents: components.map(c => {
      return {
        name: c.name,
        label: c.name,
        key: c.url_,
      };
    }),
  });
}
