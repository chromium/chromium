// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CurrentDirectory, State} from '../../externs/ts/state.js';
import {PathComponent} from '../../foreground/js/path_component.js';
import {ChangeDirectoryAction} from '../actions.js';

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
