// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CurrentDirectory, PropStatus, State} from '../../externs/ts/state.js';
import {PathComponent} from '../../foreground/js/path_component.js';
import {ChangeDirectoryAction} from '../actions.js';

/**
 * @fileoverview
 * @suppress {checkTypes}
 */

/**
 * Reducer that updates the currentDirectory property of the state and returns
 * the modified state.
 */
export function changeDirectory(
    currentState: State, action: ChangeDirectoryAction): State {
  const fileData = currentState.allEntries[action.payload.key];

  let selection = currentState.currentDirectory?.selection;
  // Use an empty selection when a selection isn't defined or it's navigating to
  // a new directory.
  if (!selection || currentState.currentDirectory?.key !== action.payload.key) {
    selection = {
      keys: [],
      dirCount: 0,
      fileCount: 0,
      hostedCount: undefined,
      offlineCachedCount: undefined,
      fileTasks: {
        tasks: [],
        defaultHandlerPolicy: undefined,
        status: PropStatus.SUCCESS,
        keys: [],
      },
    };
  }

  let currentDirectory: CurrentDirectory = {
    key: action.payload.key,
    status: action.payload.status,
    pathComponents: [],
    rootType: undefined,
    selection,
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
