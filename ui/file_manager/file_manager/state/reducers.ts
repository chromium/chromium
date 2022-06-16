// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PathComponent} from '../foreground/js/path_component.js';

import {Action, Actions, ChangeDirectoryAction} from './actions.js';
import {CurrentDirectory, State} from './state.js';

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
            changeDirectory(state, action as ChangeDirectoryAction)
      });

    default:
      console.error(`invalid action: ${action.type}`);
      return state;
  }
}

/** Caches the Action's entry in the `allEntries` attribute. */
export function cacheEntries(currentState: State, action: Action): State {
  if (action.type === Actions.CHANGE_DIRECTORY) {
    const {newDirectory, key} = (action as ChangeDirectoryAction);

    const allEntries = currentState.allEntries || {};
    const entryData = allEntries[key] || {};
    allEntries[key] = Object.assign(entryData, {
      entry: newDirectory,
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
    console.debug(`Can't find the entry with ${action.key}`);
    return currentState.currentDirectory || null;
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
    key: (action as ChangeDirectoryAction).key,
    pathComponents: components.map(c => {
      return {
        name: c.name,
        label: c.name,
        key: c.url_,
      };
    })
  });
}
