// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CurrentDirectory, State} from '../../externs/ts/state.js';
import {PathComponent} from '../../foreground/js/path_component.js';
import {ChangeDirectoryAction} from '../actions.js';

/**
 * Reducer that updates the currentDirectory property of the state and returns
 * the modified state.
 */
export function changeDirectory(
    currentState: State, action: ChangeDirectoryAction): State {
  const fileData = currentState.allEntries[action.payload.key];

  let currentDirectory: CurrentDirectory = {
    key: action.payload.key,
    status: action.payload.status,
    pathComponents: [],
  };

  // The new directory might not be in the allEntries yet, this might happen
  // when starting to change the directory for a entry that isn't cached.
  // At the end of the change directory, DirectoryContents will send an Action
  // with the Entry to be cached.
  if (fileData) {
    const volumeManager = window.fileManager?.volumeManager;
    if (!volumeManager) {
      console.debug(`VolumeManager not available yet.`);
      currentDirectory = currentState.currentDirectory || currentDirectory;
    } else {
      const components = PathComponent.computeComponentsFromEntry(
          fileData.entry!, volumeManager);
      currentDirectory.pathComponents = components.map(c => {
        return {
          name: c.name,
          label: c.name,
          key: c.url_,
        };
      });
    }
  }

  return {
    ...currentState,
    currentDirectory,
  };
}
