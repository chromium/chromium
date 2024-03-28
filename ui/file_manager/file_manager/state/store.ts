// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {FilesAppEntry} from '../common/js/files_app_entry_types.js';
import type {VolumeType} from '../common/js/volume_manager_types.js';
import {BaseStore} from '../lib/base_store.js';

import {allEntriesSlice} from './ducks/all_entries.js';
import {androidAppsSlice} from './ducks/android_apps.js';
import {bulkPinningSlice} from './ducks/bulk_pinning.js';
import {currentDirectorySlice} from './ducks/current_directory.js';
import {deviceSlice} from './ducks/device.js';
import {driveSlice} from './ducks/drive.js';
import {folderShortcutsSlice} from './ducks/folder_shortcuts.js';
import {launchParamsSlice} from './ducks/launch_params.js';
import {materializedViewsSlice} from './ducks/materialized_views.js';
import {navigationSlice} from './ducks/navigation.js';
import {preferencesSlice} from './ducks/preferences.js';
import {searchSlice} from './ducks/search.js';
import {uiEntriesSlice} from './ducks/ui_entries.js';
import {volumesSlice} from './ducks/volumes.js';
import type {FileData, FileKey, State, Volume} from './state.js';

/**
 * Files app's Store type.
 *
 * It enforces the types for the State and the Actions managed by Files app.
 */
export type Store = BaseStore<State>;

/**
 * Store singleton instance.
 * It's only exposed via `getStore()` to guarantee it's a single instance.
 * TODO(b/272120634): Use window.store temporarily, uncomment below code after
 * the duplicate store issue is resolved.
 */
// let store: null|Store = null;

/**
 * Returns the singleton instance for the Files app's Store.
 *
 * NOTE: This doesn't guarantee the Store's initialization. This should be done
 * at the app's main entry point.
 */
export function getStore(): Store {
  // TODO(b/272120634): Put the store on window to prevent Store being created
  // twice.
  if (!window.store) {
    window.store = new BaseStore<State>(getEmptyState(), [
      searchSlice,
      volumesSlice,
      bulkPinningSlice,
      uiEntriesSlice,
      androidAppsSlice,
      folderShortcutsSlice,
      navigationSlice,
      preferencesSlice,
      deviceSlice,
      driveSlice,
      currentDirectorySlice,
      allEntriesSlice,
      launchParamsSlice,
      materializedViewsSlice,
    ]);
  }

  return window.store;
}

export function getEmptyState(): State {
  // TODO(b/241707820): Migrate State to allow optional attributes.
  return {
    allEntries: {},
    currentDirectory: undefined,
    device: {
      connection: chrome.fileManagerPrivate.DeviceConnectionState.ONLINE,
    },
    drive: {
      connectionType: chrome.fileManagerPrivate.DriveConnectionStateType.ONLINE,
      offlineReason: undefined,
    },
    search: {
      query: undefined,
      status: undefined,
      options: undefined,
    },
    navigation: {
      roots: [],
    },
    volumes: {},
    uiEntries: [],
    folderShortcuts: [],
    androidApps: {},
    bulkPinning: undefined,
    preferences: undefined,
    launchParams: {
      dialogType: undefined,
    },
    materializedViews: [],
  };
}

/**
 * Promise resolved when the store's state in an desired condition.
 *
 * For each Store update the `checker` function is called, when it returns True
 * the promise is resolved.
 *
 * Resolves with the State when it's in the desired condition.
 */
export async function waitForState(
    store: Store, checker: (state: State) => boolean): Promise<State> {
  // Check if the store is already in the desired state.
  if (checker(store.getState())) {
    return store.getState();
  }

  return new Promise((resolve) => {
    const observer = {
      onStateChanged(newState: State) {
        if (checker(newState)) {
          resolve(newState);
          store.unsubscribe(this);
        }
      },
    };
    store.subscribe(observer);
  });
}

/**
 * Returns the `FileData` from a FileKey.
 */
export function getFileData(state: State, key: FileKey): FileData|null {
  const fileData = state.allEntries[key];
  if (fileData) {
    return fileData;
  }
  return null;
}

/**
 * Returns FileData for each key.
 * NOTE: It might return less results than the requested keys when the key isn't
 * found.
 */
export function getFilesData(state: State, keys: FileKey[]): FileData[] {
  const filesData: FileData[] = [];
  for (const key of keys) {
    const fileData = getFileData(state, key);
    if (fileData) {
      filesData.push(fileData);
    }
  }

  return filesData;
}

export function getEntry(state: State, key: FileKey): Entry|FilesAppEntry|null {
  const fileData = state.allEntries[key];
  return fileData?.entry ?? null;
}

export function getVolume(state: State, fileData?: FileData|null): Volume|null {
  const volumeId = fileData?.volumeId;
  return (volumeId && state.volumes[volumeId]) || null;
}

export function getVolumeType(
    state: State, fileData?: FileData|null): VolumeType|null {
  return getVolume(state, fileData)?.volumeType ?? null;
}

/**
 * A function used to select part of the Store.
 * TODO: Can the return type be stronger than `any`?
 */
export type StateSelector = (state: State) => any;
