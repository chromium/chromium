// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertDeepEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {FilesAppDirEntry} from '../externs/files_app_entry_interfaces.js';
import {PropStatus} from '../externs/ts/state.js';

import {changeDirectory, updateSelection} from './actions/current_directory.js';
import {Store} from './store.js';

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
