// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import {MockFileSystem} from '../../common/js/mock_entry.js';
import {waitUntil} from '../../common/js/test_error_reporting.js';
import {ActionType, changeDirectory, ClearStaleCachedEntriesAction} from '../actions.js';
import {getEmptyState, getStore, Store} from '../store.js';

import {clearCachedEntries} from './all_entries.js';

let store: Store;
let fileSystem: MockFileSystem;

export function setUp() {
  store = getStore();

  // changeDirectory() reducer uses the VolumeManager.
  const volumeManager = new MockVolumeManager();
  window.fileManager = {
    volumeManager: volumeManager,
  };

  store.init(getEmptyState());

  fileSystem = volumeManager.getCurrentProfileVolumeInfo(
                                'downloads')!.fileSystem as MockFileSystem;
  fileSystem.populate([
    '/dir-1/',
    '/dir-2/sub-dir/',
    '/dir-3/',
  ]);
}

function allEntriesSize(): number {
  return Object.keys(store.getState().allEntries).length;
}

function cd(directory: DirectoryEntry) {
  store.dispatch(changeDirectory({to: directory, toKey: directory.toURL()}));
}

/** Tests that entries get cached in the allEntries. */
export function testAllEntries() {
  assertEquals(0, allEntriesSize(), 'allEntries should start empty');

  const dir1 = fileSystem.entries['/dir-1'];
  const dir2 = fileSystem.entries['/dir-2'];
  const dir2SubDir = fileSystem.entries['/dir-2/sub-dir'];
  cd(dir1);
  assertEquals(1, allEntriesSize(), 'dir-1 should be cached');

  cd(dir2);
  assertEquals(2, allEntriesSize(), 'dir-2 should be cached');

  cd(dir2SubDir);
  assertEquals(3, allEntriesSize(), 'dir-2/sub-dir/ should be cached');
}

export async function testClearStaleEntries(done: () => void) {
  const dir1 = fileSystem.entries['/dir-1'];
  const dir2 = fileSystem.entries['/dir-2'];
  const dir3 = fileSystem.entries['/dir-3'];
  const dir2SubDir = fileSystem.entries['/dir-2/sub-dir'];

  cd(dir1);
  cd(dir2);
  cd(dir3);
  cd(dir2SubDir);

  assertEquals(4, allEntriesSize(), 'all entries should be cached');

  // Wait for the async clear to be called.
  await waitUntil(() => allEntriesSize() < 4);

  // It should keep the current directory and all its parents that were
  // previously cached.
  assertEquals(
      2, allEntriesSize(), 'only dir-2 and dir-2/sub-dir should be cached');
  assertDeepEquals(
      Object.keys(store.getState().allEntries).sort(),
      ['filesystem:downloads/dir-2', 'filesystem:downloads/dir-2/sub-dir']);

  // Running the clear multiple times should not change:
  const action: ClearStaleCachedEntriesAction = {
    type: ActionType.CLEAR_STALE_CACHED_ENTRIES,
    payload: undefined,
  };
  clearCachedEntries(store.getState(), action);
  clearCachedEntries(store.getState(), action);
  assertEquals(
      2, allEntriesSize(), 'only dir-2 and dir-2/sub-dir should be cached');

  done();
}
