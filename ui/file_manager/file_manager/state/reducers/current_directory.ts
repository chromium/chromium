// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CurrentDirectory, State} from '../../externs/ts/state.js';
import {PathComponent} from '../../foreground/js/path_component.js';
import {ChangeDirectoryAction} from '../actions.js';

/** Reducer that updates the `currentDirectory` attributes. */
export function changeDirectory(
    currentState: State, action: ChangeDirectoryAction): CurrentDirectory {
  const fileData = currentState.allEntries[action.payload.key];
  const emptyDir = {
    key: action.payload.key,
    status: action.payload.status,
    pathComponents: [],
  };

  if (!fileData) {
    // The new directory might not be in the allEntries yet, this might happen
    // when starting to change the directory for a entry that isn't cached.
    // At the end of the change directory, DirectoryContents will send an Action
    // with the Entry to be cached.
    return emptyDir;
  }

  const volumeManager = window.fileManager?.volumeManager;
  if (!volumeManager) {
    console.debug(`VolumeManager not available yet.`);
    return currentState.currentDirectory || emptyDir;
  }

  const components =
      PathComponent.computeComponentsFromEntry(fileData.entry!, volumeManager);

  return Object.assign(currentState.currentDirectory || {}, {
    status: action.payload.status,
    key: action.payload.key,
    pathComponents: components.map(c => {
      return {
        name: c.name,
        label: c.name,
        key: c.url_,
      };
    }),
  });
}
