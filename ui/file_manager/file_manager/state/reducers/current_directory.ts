// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CurrentDirectory, FileKey, FileTasks, PropStatus, Selection, State} from '../../externs/ts/state.js';
import {PathComponent} from '../../foreground/js/path_component.js';
import {ChangeDirectoryAction, ChangeFileTasksAction, ChangeSelectionAction} from '../actions/current_directory.js';

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
        policyDefaultHandlerStatus: undefined,
        defaultTask: undefined,
        status: PropStatus.SUCCESS,
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

      const locationInfo = volumeManager.getLocationInfo(fileData.entry!);
      currentDirectory.rootType = locationInfo?.rootType;
    }
  }

  return {
    ...currentState,
    currentDirectory,
  };
}

function getEmptySelection(keys: FileKey[] = []): Selection {
  return {
    keys,
    dirCount: 0,
    fileCount: 0,
    // hostedCount might be updated to undefined in the for loop below.
    hostedCount: 0,
    // offlineCachedCount might be updated to undefined in the for loop below.
    offlineCachedCount: 0,
    fileTasks: {
      tasks: [],
      defaultTask: undefined,
      policyDefaultHandlerStatus: undefined,
      status: PropStatus.STARTED,
    },
  };
}

/**
 * Updates the `currentDirectory.selection` state.
 */
export function updateSelection(
    currentState: State, action: ChangeSelectionAction): State {
  // TODO: When we have all the keys from current directory, we should validate
  // that selectedKeys belongs to current directory.
  const selection = getEmptySelection(action.payload.selectedKeys);

  for (const key of action.payload.selectedKeys) {
    const fileData = currentState.allEntries[key];
    if (!fileData) {
      console.warn(`Missing entry: ${key}`);
      continue;
    }
    if (fileData.isDirectory) {
      selection.dirCount++;
    } else {
      selection.fileCount++;
    }

    // Update hostedCount to undefined if any entry doesn't have the metadata
    // yet.
    const isHosted = fileData.metadata?.hosted;
    if (isHosted === undefined) {
      selection.hostedCount = undefined;
    } else {
      if (selection.hostedCount !== undefined && isHosted) {
        selection.hostedCount++;
      }
    }

    // Update offlineCachedCount to undefined if any entry doesn't have the
    // metadata yet.
    const isOfflineCached = fileData.metadata?.offlineCached;
    if (isOfflineCached === undefined) {
      selection.offlineCachedCount = undefined;
    } else {
      if (selection.offlineCachedCount !== undefined && isOfflineCached) {
        selection.offlineCachedCount++;
      }
    }
  }

  const currentDirectory: CurrentDirectory = {
    ...currentState.currentDirectory,
    selection,
  } as CurrentDirectory;

  return {
    ...currentState,
    currentDirectory,
  };
}

/** Updates the FileTasks in the selection for the current directory. */
export function updateFileTasks(
    currentState: State, action: ChangeFileTasksAction): State {
  const initialSelection =
      currentState.currentDirectory?.selection ?? getEmptySelection();

  // Apply the changes over the current selection.
  const fileTasks: FileTasks = {
    ...initialSelection.fileTasks,
    ...action.payload,
  };

  // Update the selection and current directory objects.
  const selection: Selection = {
    ...initialSelection,
    fileTasks,
  };
  const currentDirectory: CurrentDirectory = {
    ...currentState.currentDirectory,
    selection,
  } as CurrentDirectory;

  return {
    ...currentState,
    currentDirectory,
  };
}
