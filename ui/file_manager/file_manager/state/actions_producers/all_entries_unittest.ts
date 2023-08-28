// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EntryList, FakeEntryImpl, VolumeEntry} from '../../common/js/files_app_entry_types.js';
import {metrics} from '../../common/js/metrics.js';
import {MockFileSystem} from '../../common/js/mock_entry.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {State} from '../../externs/ts/state.js';
import {setUpFileManagerOnWindow, setupStore, waitDeepEquals} from '../for_tests.js';
import {convertEntryToFileData} from '../reducers/all_entries.js';
import {getEmptyState} from '../store.js';

import {readSubDirectories} from './all_entries.js';

export function setUp() {
  // sortEntries() requires the directoryModel on the window.fileManager.
  setUpFileManagerOnWindow();
  // Mock metrics.recordSmallCount.
  metrics.recordSmallCount = function() {};
  metrics.startInterval = function() {};
  metrics.recordInterval = function() {};
}

/**
 * Tests that reading sub directories will put the reading result into the
 * store.
 */
export async function testReadSubDirectories(done: () => void) {
  const initialState = getEmptyState();
  // Populate fake entries in the file system.
  const {volumeManager} = window.fileManager;
  const downloadsVolumeInfo = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DOWNLOADS)!;
  const fakeFs = downloadsVolumeInfo.fileSystem as MockFileSystem;
  fakeFs.populate([
    '/Downloads/',
    '/Downloads/c/',
    '/Downloads/b.txt',
    '/Downloads/a/',
  ]);
  const downloadsEntry = fakeFs.entries['/Downloads'];
  // The entry to be read should be in the store before reading.
  const downloadsEntryFileData = convertEntryToFileData(downloadsEntry);
  initialState.allEntries[downloadsEntry.toURL()] = downloadsEntryFileData;
  // Put it in the uiEntries so it won't be cleared.
  initialState.uiEntries.push(downloadsEntry.toURL());
  const store = setupStore(initialState);

  // Dispatch read sub directories action producer.
  store.dispatch(readSubDirectories(downloadsEntry));

  // Expect store to have all its sub directories.
  const aDirEntry = fakeFs.entries['/Downloads/a'];
  const cDirEntry = fakeFs.entries['/Downloads/c'];
  const want: State['allEntries'] = {
    [downloadsEntry.toURL()]: downloadsEntryFileData,
    [aDirEntry.toURL()]: convertEntryToFileData(aDirEntry),
    [cDirEntry.toURL()]: convertEntryToFileData(cDirEntry),
  };
  want[downloadsEntry.toURL()].children =
      [aDirEntry.toURL(), cDirEntry.toURL()];

  await waitDeepEquals(store, want, (state) => state.allEntries);

  done();
}

/**
 * Tests that reading sub directories recursively will put the reading result of
 * children of grand children into the store.
 */
export async function testReadSubDirectoriesRecursively(done: () => void) {
  const initialState = getEmptyState();
  // Populate fake entries in the file system.
  const {volumeManager} = window.fileManager;
  const downloadsVolumeInfo = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DOWNLOADS)!;
  const fakeFs = downloadsVolumeInfo.fileSystem as MockFileSystem;
  fakeFs.populate([
    '/Downloads/',
    '/Downloads/a/',
    '/Downloads/b/',
    '/Downloads/a/111/',
    '/Downloads/b/222/',
  ]);
  const downloadsEntry = fakeFs.entries['/Downloads'];
  const bDirEntry = fakeFs.entries['/Downloads/b'];
  // The entry to be read should be in the store before reading.
  const downloadsEntryFileData = convertEntryToFileData(downloadsEntry);
  // Set downloadsEntry.expanded = true, so it will be read recursively.
  downloadsEntryFileData.expanded = true;
  initialState.allEntries[downloadsEntry.toURL()] = downloadsEntryFileData;
  const store = setupStore(initialState);

  // Dispatch read sub directories action producer.
  store.dispatch(readSubDirectories(downloadsEntry, /* recursive= */ true));

  // Expect store to have all its sub directories.
  const aDirEntry = fakeFs.entries['/Downloads/a'];
  const dirEntry1 = fakeFs.entries['/Downloads/a/111'];
  const dirEntry2 = fakeFs.entries['/Downloads/b/222'];
  const want: State['allEntries'] = {
    [downloadsEntry.toURL()]: downloadsEntryFileData,
    [aDirEntry.toURL()]: convertEntryToFileData(aDirEntry),
    [bDirEntry.toURL()]: convertEntryToFileData(bDirEntry),
    [dirEntry1.toURL()]: convertEntryToFileData(dirEntry1),
    [dirEntry2.toURL()]: convertEntryToFileData(dirEntry2),
  };
  want[downloadsEntry.toURL()].children =
      [aDirEntry.toURL(), bDirEntry.toURL()];
  want[aDirEntry.toURL()].children = [dirEntry1.toURL()];
  want[bDirEntry.toURL()].children = [dirEntry2.toURL()];

  await waitDeepEquals(store, want, (state) => state.allEntries);

  done();
}

/** Tests that reading a null entry does nothing. */
export async function testReadSubDirectoriesWithNullEntry(done: () => void) {
  const store = setupStore();

  // Check reading null entry will do nothing.
  store.dispatch(readSubDirectories(null));

  await waitDeepEquals(store, {}, (state) => state.allEntries);

  done();
}

/** Tests that reading a non directory entry does nothing. */
export async function testReadSubDirectoriesWithNonDirectoryEntry(
    done: () => void) {
  const store = setupStore();

  // Populate a fake file entry in the file system.
  const fakeFs = new MockFileSystem('fake-fs');
  fakeFs.populate([
    '/a.txt',
  ]);

  // Check reading non directory entry will do nothing.
  store.dispatch(readSubDirectories(fakeFs.entries['/a.txt']));

  await waitDeepEquals(store, {}, (state) => state.allEntries);

  done();
}

/** Tests that reading a disabled entry does nothing. */
export async function testReadSubDirectoriesWithDisabledEntry(
    done: () => void) {
  const store = setupStore();

  // Make downloadsEntry as disabled.
  const {volumeManager} = window.fileManager;
  const downloadsVolumeInfo = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DOWNLOADS)!;
  const downloadsEntry = new VolumeEntry(downloadsVolumeInfo);
  downloadsEntry.disabled = true;

  // Check reading disabled volume entry will do nothing.
  store.dispatch(readSubDirectories(downloadsEntry));

  await waitDeepEquals(store, {}, (state) => state.allEntries);

  done();
}

/**
 * Tests that reading sub directories for fake drive entry will put the reading
 * result into the store and handle grand roots properly.
 */
export async function testReadSubDirectoriesForFakeDriveEntry(
    done: () => void) {
  const initialState = getEmptyState();
  // MockVolumeManager will populate Drive's /root, /team_drives, /Computers
  // automatically.
  const {volumeManager} = window.fileManager;
  const driveVolumeInfo = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DRIVE)!;
  const driveFs = driveVolumeInfo.fileSystem as MockFileSystem;

  // Create drive root entry list and add all its children.
  const driveRootEntryList = new EntryList(
      'Google Drive', VolumeManagerCommon.RootType.DRIVE_FAKE_ROOT);
  const sharedWithMeEntry = new FakeEntryImpl(
      'Shared with me', VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME);
  const offlineEntry =
      new FakeEntryImpl('Offline', VolumeManagerCommon.RootType.DRIVE_OFFLINE);
  const driveEntry = driveFs.entries['/root'];
  const computersEntry = driveFs.entries['/Computers'];
  driveRootEntryList.addEntry(driveEntry);
  driveRootEntryList.addEntry(driveFs.entries['/team_drives']);
  driveRootEntryList.addEntry(computersEntry);
  driveRootEntryList.addEntry(sharedWithMeEntry);
  driveRootEntryList.addEntry(offlineEntry);
  // Add child entries.
  driveFs.populate([
    '/root/a/',
    '/root/b.txt',
    '/Computers/c.txt',
  ]);
  // Drive root entry list needs to be in the store before reading.
  const fakeDriveEntryFileData = convertEntryToFileData(driveRootEntryList);
  initialState.allEntries[driveRootEntryList.toURL()] = fakeDriveEntryFileData;
  // Put it in the uiEntries so it won't be cleared.
  initialState.uiEntries.push(driveRootEntryList.toURL());
  const store = setupStore(initialState);

  // Dispatch read sub directories action producer.
  store.dispatch(readSubDirectories(driveRootEntryList));

  // Expect its direct sub directories and grand sub directories of /Computers
  // should be in the store.
  const want: State['allEntries'] = {
    [driveRootEntryList.toURL()]: fakeDriveEntryFileData,
    [driveEntry.toURL()]: convertEntryToFileData(driveEntry),
    [computersEntry.toURL()]: convertEntryToFileData(computersEntry),
    [sharedWithMeEntry.toURL()]: convertEntryToFileData(sharedWithMeEntry),
    [offlineEntry.toURL()]: convertEntryToFileData(offlineEntry),
    // /team_drives/ won't be here because it doesn't have children.
  };
  want[driveRootEntryList.toURL()].children = [
    driveEntry.toURL(),
    computersEntry.toURL(),
    sharedWithMeEntry.toURL(),
    offlineEntry.toURL(),
  ];
  // /root children won't be read.

  await waitDeepEquals(store, want, (state) => state.allEntries);

  done();
}
