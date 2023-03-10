// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertDeepEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {FilesAppDirEntry} from '../externs/files_app_entry_interfaces.js';
import {FileKey, PropStatus, State} from '../externs/ts/state.js';

import {EntryMetadata, updateMetadata} from './actions/all_entries.js';
import {changeDirectory, updateDirectoryContent, updateSelection} from './actions/current_directory.js';
import {StateSelector, Store, waitForState} from './store.js';

/**
 * Compares 2 State objects and fails with nicely formatted message when it
 * fails.
 */
export function assertStateEquals(want: any, got: any) {
  assertDeepEquals(
      want, got,
      `\nWANT:\n${JSON.stringify(want, null, 2)}\nGOT:\n${
          JSON.stringify(got, null, 2)}\n\n`);
}

/**
 * Returns the `allEntries` size of the passed State.
 */
export function allEntriesSize(state: State): number {
  return Object.keys(state.allEntries).length;
}

/**
 * Compares the current state's allEntries field to the expected list. Fails
 * with a nicely formatted message if there's a mismatch.
 */
export function assertAllEntriesEqual(store: Store, want: FileKey[]) {
  const got = Object.keys(store.getState().allEntries).sort();
  assertDeepEquals(
      want, got,
      `\nWANT:\n${JSON.stringify(want, null, 2)}\nGOT:\n${
          JSON.stringify(got, null, 2)}\n\n`);
}

/** Change the directory in the store. */
export function cd(store: Store, directory: DirectoryEntry|FilesAppDirEntry) {
  store.dispatch(changeDirectory(
      {to: directory, toKey: directory.toURL(), status: PropStatus.SUCCESS}));
}

/** Updates the selection in the store. */
export function changeSelection(store: Store, entries: Entry[]) {
  store.dispatch(updateSelection({
    selectedKeys: entries.map(e => e.toURL()),
    entries,
  }));
}

/** Updates the metadata in the store. */
export function updMetadata(store: Store, metadata: EntryMetadata[]) {
  store.dispatch(updateMetadata({metadata}));
}

/** Updates the directory content in the store. */
export function updateContent(store: Store, entries: Entry[]) {
  store.dispatch(updateDirectoryContent({entries}));
}

/**
 * Waits for a part of the Store to be in the expected state.
 *
 * Waits a maximum of 10 seconds, since in the unittest the Store manipulation
 * has all async APIs mocked.
 *
 * Usage:
 * let want: StoreSomething = {somePartOfStore: 'desired state'};
 * store.dispatch(someActionsProducer(...));
 * await waitDeepEquals(store, want, (state) => state.something);
 */
export async function waitDeepEquals(
    store: Store, want: any, stateSelection: StateSelector) {
  let got: any;
  const timeout = new Promise((_, reject) => {
    setTimeout(() => {
      reject(new Error(`waitDeepEquals timed out waiting for \n${want}`));
    }, 10000);
  });

  const checker = waitForState(store, (state) => {
    try {
      got = stateSelection(state);
      assertDeepEquals(want, got);
      return true;
    } catch (error: any) {
      if (error.constructor?.name === 'AssertionError') {
        return false;
      }
      console.log(error.stack);
      console.error(error);
      throw error;
    }
  });

  await Promise.race([checker, timeout]);
}
