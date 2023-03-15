// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FilesAppEntry} from '../externs/files_app_entry_interfaces.js';
import {FileData, FileKey, SearchFileType, SearchLocation, SearchOptions, SearchRecency, State} from '../externs/ts/state.js';
import {BaseStore} from '../lib/base_store.js';

import {Action} from './actions.js';
import {rootReducer} from './reducers/root.js';

/**
 * Files app's Store type.
 *
 * It enforces the types for the State and the Actions managed by Files app.
 */
export type Store = BaseStore<State, Action>;

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
    window.store =
        new BaseStore<State, Action>({allEntries: {}} as State, rootReducer);
  }

  return window.store;
}

export function getEmptyState(): State {
  // TODO(b/241707820): Migrate State to allow optional attributes.
  return {
    allEntries: {},
    currentDirectory: undefined,
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
    androidApps: [],
  };
}

/**
 * Search options to be used if the user did not specify their own.
 */
export function getDefaultSearchOptions(): SearchOptions {
  return {
    location: SearchLocation.THIS_FOLDER,
    recency: SearchRecency.ANYTIME,
    type: SearchFileType.ALL_TYPES,
  } as SearchOptions;
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

/**
 * A function used to select part of the Store.
 * TODO: Can the return type be stronger than `any`?
 */
export type StateSelector = (state: State) => any;
