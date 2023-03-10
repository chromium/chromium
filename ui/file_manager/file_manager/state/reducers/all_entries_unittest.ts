// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import {DialogType} from '../../common/js/dialog_type.js';
import {FakeEntryImpl, VolumeEntry} from '../../common/js/files_app_entry_types.js';
import {MockFileSystem} from '../../common/js/mock_entry.js';
import {waitUntil} from '../../common/js/test_error_reporting.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {Crostini} from '../../externs/background/crostini.js';
import {EntryType, FileData} from '../../externs/ts/state.js';
import {FileSelectionHandler} from '../../foreground/js/file_selection.js';
import {MetadataItem} from '../../foreground/js/metadata/metadata_item.js';
import {MetadataModel} from '../../foreground/js/metadata/metadata_model.js';
import {MockMetadataModel} from '../../foreground/js/metadata/mock_metadata.js';
import {TaskController} from '../../foreground/js/task_controller.js';
import {ActionType} from '../actions.js';
import {ClearStaleCachedEntriesAction} from '../actions/all_entries.js';
import {allEntriesSize, assertAllEntriesEqual, cd, changeSelection, updMetadata} from '../for_tests.js';
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
    metadataModel: new MockMetadataModel({}) as unknown as MetadataModel,
    crostini: {} as unknown as Crostini,
    selectionHandler: {} as unknown as FileSelectionHandler,
    taskController: {} as unknown as TaskController,
    dialogType: DialogType.FULL_PAGE,
  };

  store.init(getEmptyState());

  fileSystem = volumeManager
                   .getCurrentProfileVolumeInfo(
                       VolumeManagerCommon.VolumeType.DOWNLOADS)!.fileSystem as
      MockFileSystem;
  fileSystem.populate([
    '/dir-1/',
    '/dir-2/sub-dir/',
    '/dir-2/file-1.txt',
    '/dir-2/file-2.txt',
    '/dir-3/',
  ]);
}

/** Tests that entries get cached in the allEntries. */
export function testAllEntries() {
  assertEquals(
      0, allEntriesSize(store.getState()), 'allEntries should start empty');

  const dir1 = fileSystem.entries['/dir-1'];
  const dir2 = fileSystem.entries['/dir-2'];
  const dir2SubDir = fileSystem.entries['/dir-2/sub-dir'];
  cd(store, dir1);
  assertEquals(1, allEntriesSize(store.getState()), 'dir-1 should be cached');

  cd(store, dir2);
  assertEquals(2, allEntriesSize(store.getState()), 'dir-2 should be cached');

  cd(store, dir2SubDir);
  assertEquals(
      3, allEntriesSize(store.getState()), 'dir-2/sub-dir/ should be cached');
}

export async function testClearStaleEntries(done: () => void) {
  const dir1 = fileSystem.entries['/dir-1'];
  const dir2 = fileSystem.entries['/dir-2'];
  const dir3 = fileSystem.entries['/dir-3'];
  const dir2SubDir = fileSystem.entries['/dir-2/sub-dir'];

  cd(store, dir1);
  cd(store, dir2);
  cd(store, dir3);
  cd(store, dir2SubDir);

  assertEquals(
      4, allEntriesSize(store.getState()), 'all entries should be cached');

  // Wait for the async clear to be called.
  await waitUntil(() => allEntriesSize(store.getState()) < 4);

  // It should keep the current directory and all its parents that were
  // previously cached.
  assertEquals(
      2, allEntriesSize(store.getState()),
      'only dir-2 and dir-2/sub-dir should be cached');
  assertAllEntriesEqual(
      store,
      ['filesystem:downloads/dir-2', 'filesystem:downloads/dir-2/sub-dir']);

  // Running the clear multiple times should not change:
  const action: ClearStaleCachedEntriesAction = {
    type: ActionType.CLEAR_STALE_CACHED_ENTRIES,
    payload: undefined,
  };
  clearCachedEntries(store.getState(), action);
  clearCachedEntries(store.getState(), action);
  assertEquals(
      2, allEntriesSize(store.getState()),
      'only dir-2 and dir-2/sub-dir should be cached');

  done();
}

export function testCacheEntries() {
  const dir1 = fileSystem.entries['/dir-1'];
  const file1 = fileSystem.entries['/dir-2/file-1.txt'];
  const md = window.fileManager.metadataModel as unknown as MockMetadataModel;
  md.set(file1, {isRestrictedForDestination: true});

  // Cache a directory via changeDirectory.
  cd(store, dir1);
  let resultEntry: FileData = store.getState().allEntries[dir1.toURL()];
  assertTrue(!!resultEntry);
  assertEquals(resultEntry.entry, dir1);
  assertTrue(resultEntry.isDirectory);
  assertEquals(resultEntry.label, dir1.name);
  assertEquals(
      resultEntry.volumeType, VolumeManagerCommon.VolumeType.DOWNLOADS);
  assertEquals(resultEntry.type, EntryType.FS_API);
  assertFalse(!!resultEntry.metadata.isRestrictedForDestination);

  // Cache a file via changeSelection.
  changeSelection(store, [file1]);
  resultEntry = store.getState().allEntries[file1.toURL()];
  assertTrue(!!resultEntry);
  assertEquals(resultEntry.entry, file1);
  assertFalse(resultEntry.isDirectory);
  assertEquals(resultEntry.label, file1.name);
  assertEquals(
      resultEntry.volumeType, VolumeManagerCommon.VolumeType.DOWNLOADS);
  assertEquals(resultEntry.type, EntryType.FS_API);
  assertTrue(!!resultEntry.metadata.isRestrictedForDestination);
  const recentRoot =
      new FakeEntryImpl('Recent', VolumeManagerCommon.RootType.RECENT);
  cd(store, recentRoot);
  resultEntry = store.getState().allEntries[recentRoot.toURL()];
  assertTrue(!!resultEntry);
  assertEquals(resultEntry.entry, recentRoot);
  assertTrue(resultEntry.isDirectory);
  assertEquals(resultEntry.label, recentRoot.name);
  assertEquals(resultEntry.volumeType, null);
  assertEquals(resultEntry.type, EntryType.RECENT);

  const volumeInfo =
      window.fileManager.volumeManager.getCurrentProfileVolumeInfo(
          VolumeManagerCommon.VolumeType.DOWNLOADS)!;
  const volumeEntry = new VolumeEntry(volumeInfo);
  cd(store, volumeEntry);
  resultEntry = store.getState().allEntries[volumeEntry.toURL()];
  assertTrue(!!resultEntry);
  assertEquals(resultEntry.entry, volumeEntry);
  assertTrue(resultEntry.isDirectory);
  assertEquals(resultEntry.label, volumeEntry.name);
  assertEquals(
      resultEntry.volumeType, VolumeManagerCommon.VolumeType.DOWNLOADS);
  assertEquals(resultEntry.type, EntryType.VOLUME_ROOT);
  assertFalse(!!resultEntry.metadata.isRestrictedForDestination);
}

export function testUpdateMetadata() {
  const dir1 = fileSystem.entries['/dir-1'];
  const file1 = fileSystem.entries['/dir-2/file-1.txt'];
  const file2 = fileSystem.entries['/dir-2/file-2.txt'];
  const md = window.fileManager.metadataModel as unknown as MockMetadataModel;
  md.set(file1, {isRestrictedForDestination: true});
  md.set(file2, {isRestrictedForDestination: false});

  // Cache a directory via changeDirectory.
  cd(store, dir1);
  assertEquals(1, allEntriesSize(store.getState()));
  let resultEntry: FileData = store.getState().allEntries[dir1.toURL()];
  assertEquals(undefined, resultEntry.metadata.isRestrictedForDestination);
  assertEquals(undefined, resultEntry.metadata.isDlpRestricted);

  // Cache a file via changeSelection.
  changeSelection(store, [file1]);
  assertEquals(2, allEntriesSize(store.getState()));
  resultEntry = store.getState().allEntries[file1.toURL()];
  assertTrue(!!resultEntry.metadata.isRestrictedForDestination);
  assertEquals(undefined, resultEntry.metadata.isDlpRestricted);

  // Update the metadata: it should first cache missing entries, and append the
  // new metadata to the already fetched values.
  const metadata: MetadataItem = new MetadataItem();
  metadata.isDlpRestricted = true;
  updMetadata(store, [{entry: file1, metadata}, {entry: file2, metadata}]);

  resultEntry = store.getState().allEntries[dir1.toURL()];
  assertEquals(undefined, resultEntry.metadata.isRestrictedForDestination);
  assertEquals(undefined, resultEntry.metadata.isDlpRestricted);

  resultEntry = store.getState().allEntries[file1.toURL()];
  assertTrue(!!resultEntry.metadata.isRestrictedForDestination);
  assertTrue(!!resultEntry.metadata.isDlpRestricted);

  resultEntry = store.getState().allEntries[file2.toURL()];
  assertFalse(!!resultEntry.metadata.isRestrictedForDestination);
  assertTrue(!!resultEntry.metadata.isDlpRestricted);
}
