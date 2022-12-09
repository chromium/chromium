// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FileData, FileKey, State} from '../externs/ts/state.js';
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
 */
let store: null|Store = null;

/**
 * Returns the singleton instance for the Files app's Store.
 *
 * NOTE: This doesn't guarantee the Store's initialization. This should be done
 * at the app's main entry point.
 */
export function getStore(): Store {
  if (!store) {
    store =
        new BaseStore<State, Action>({allEntries: {}} as State, rootReducer);
  }

  return store;
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
 * @throws {Error} if it can't find the FileData.
 */
export function getFileData(state: State, key: FileKey): FileData {
  const entry = state.allEntries[key];
  if (!entry) {
    throw new Error(`Key ${key} not found in the store`);
  }
  return entry;
}

/**
 * Returns FileData for each key.
 * @throws {Error} if it can't find the FileData.
 */
export function getFilesData(state: State, keys: FileKey[]): FileData[] {
  const filesData: FileData[] = [];
  for (const key of keys) {
    filesData.push(getFileData(state, key));
  }

  return filesData;
}

/**
 * A function used to select part of the Store.
 * TODO: Can the return type be stronger than `any`?
 */
export type StateSelector = (state: State) => any;
