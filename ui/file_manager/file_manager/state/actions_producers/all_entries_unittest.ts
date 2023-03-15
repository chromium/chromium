// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {EntryList, FakeEntryImpl, VolumeEntry} from '../../common/js/files_app_entry_types.js';
import {metrics} from '../../common/js/metrics.js';
import {MockFileSystem} from '../../common/js/mock_entry.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {State} from '../../externs/ts/state.js';
import {constants} from '../../foreground/js/constants.js';
import {setUpFileManagerOnWindow, setupStore, waitDeepEquals} from '../for_tests.js';
import {convertEntryToFileData} from '../reducers/all_entries.js';

import {readSubDirectories} from './all_entries.js';

// Global variables to check if the callback is called or not.
let isSuccessCallbackCalled = false;
let isErrorCallbackCalled = false;

function successCallback() {
  isSuccessCallbackCalled = true;
}
function errorCallback() {
  isErrorCallbackCalled = true;
}

export function setUp() {
  // sortEntries() requires the directoryModel on the window.fileManager.
  setUpFileManagerOnWindow();
  // Mock metrics.recordSmallCount.
  metrics.recordSmallCount = function(_name, _value) {};
  // Reset global callback flags.
  isSuccessCallbackCalled = false;
  isErrorCallbackCalled = false;
}

/**
 * Tests that reading sub directories will put the reading result into the
 * store.
 */
export async function testReadSubDirectories(done: () => void) {
  const store = setupStore();
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
  store.getState().allEntries[downloadsEntry.toURL()] = downloadsEntryFileData;

  // Dispatch read sub directories action producer.
  store.dispatch(
      readSubDirectories(downloadsEntry, successCallback, errorCallback));

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
  assertTrue(isSuccessCallbackCalled);
  assertFalse(isErrorCallbackCalled);

  done();
}

/** Tests that reading a null entry does nothing. */
export async function testReadSubDirectoriesWithNullEntry(done: () => void) {
  const store = setupStore();

  // Dispatch read sub directories action producer.
  store.dispatch(readSubDirectories(null, successCallback, errorCallback));

  await waitDeepEquals(store, {}, (state) => state.allEntries);
  assertFalse(isSuccessCallbackCalled);
  assertTrue(isErrorCallbackCalled);

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

  // Check reading non directory entry will call error callback.
  store.dispatch(readSubDirectories(
      fakeFs.entries['/a.txt'], successCallback, errorCallback));

  await waitDeepEquals(store, {}, (state) => state.allEntries);
  assertFalse(isSuccessCallbackCalled);
  assertTrue(isErrorCallbackCalled);

  done();
}

/** Tests that reading a disabled entry does nothing. */
export async function testReadSubDirectoriesWithDisabledEntry() {
  const store = setupStore();

  // Make downloadsEntry as disabled.
  const {volumeManager} = window.fileManager;
  const downloadsVolumeInfo = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DOWNLOADS)!;
  const downloadsEntry = new VolumeEntry(downloadsVolumeInfo);
  downloadsEntry.disabled = true;

  // Check reading disabled volume entry will call error callback.
  store.dispatch(
      readSubDirectories(downloadsEntry, successCallback, errorCallback));

  assertFalse(isSuccessCallbackCalled);
  assertTrue(isErrorCallbackCalled);
}

/**
 * Tests that reading sub directories for fake drive entry will put the reading
 * result into the store and handle grand roots properly.
 */
export async function testReadSubDirectoriesForFakeDriveEntry(
    done: () => void) {
  const store = setupStore();
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
  store.getState().allEntries[driveRootEntryList.toURL()] =
      fakeDriveEntryFileData;

  // Dispatch read sub directories action producer.
  store.dispatch(
      readSubDirectories(driveRootEntryList, successCallback, errorCallback));

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
  want[computersEntry.toURL()].icon = constants.ICON_TYPES.COMPUTERS_GRAND_ROOT;
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
