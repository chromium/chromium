// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {State} from '../externs/ts/state.js';
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
    },
  };
}
