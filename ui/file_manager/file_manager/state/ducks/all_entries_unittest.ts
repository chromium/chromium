// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {fakeMyFilesVolumeId, MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import {EntryList, FakeEntryImpl, GuestOsPlaceholder, VolumeEntry} from '../../common/js/files_app_entry_types.js';
import {installMockChrome} from '../../common/js/mock_chrome.js';
import {MockFileSystem} from '../../common/js/mock_entry.js';
import {waitUntil} from '../../common/js/test_error_reporting.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {EntryType, FileData, State} from '../../externs/ts/state.js';
import {VolumeInfo} from '../../externs/volume_info.js';
import {constants} from '../../foreground/js/constants.js';
import {MetadataItem} from '../../foreground/js/metadata/metadata_item.js';
import {MockMetadataModel} from '../../foreground/js/metadata/mock_metadata.js';
import {addChildEntries} from '../ducks/all_entries.js';
import {allEntriesSize, assertAllEntriesEqual, cd, changeSelection, createFakeVolumeMetadata, setUpFileManagerOnWindow, setupStore, updMetadata, waitDeepEquals} from '../for_tests.js';
import {getEmptyState, type Store} from '../store.js';

import {clearCachedEntries, convertEntryToFileData, getMyFiles, readSubDirectories} from './all_entries.js';
import {convertVolumeInfoAndMetadataToVolume, myFilesEntryListKey} from './volumes.js';

let store: Store;
let fileSystem: MockFileSystem;

export function setUp() {
  // changeDirectory() reducer uses the VolumeManager.
  // sortEntries() requires the directoryModel on the window.fileManager.
  setUpFileManagerOnWindow();
  installMockChrome({});

  store = setupStore();

  fileSystem = window.fileManager.volumeManager
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

/** Generate MyFiles entry with fake entry list. */
function createMyFilesDataWithEntryList(): FileData {
  const myFilesEntryList =
      new EntryList('My files', VolumeManagerCommon.RootType.MY_FILES);
  return convertEntryToFileData(myFilesEntryList);
}

/** Generate MyFiles entry with real volume entry. */
function createMyFilesDataWithVolumeEntry():
    {fileData: FileData, volumeInfo: VolumeInfo} {
  const volumeManager = new MockVolumeManager();
  const downloadsVolumeInfo = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DOWNLOADS)!;
  const myFilesVolumeEntry = new VolumeEntry(downloadsVolumeInfo);
  const fileData = convertEntryToFileData(myFilesVolumeEntry);
  return {fileData, volumeInfo: downloadsVolumeInfo};
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
  const myFilesRootURL = `filesystem:${fakeMyFilesVolumeId}`;

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
      store, [`${myFilesRootURL}/dir-2`, `${myFilesRootURL}/dir-2/sub-dir`]);

  // Running the clear multiple times should not change:
  store.dispatch(clearCachedEntries());
  store.dispatch(clearCachedEntries());
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
  assertEquals(resultEntry.volumeId, fakeMyFilesVolumeId);
  assertEquals(resultEntry.type, EntryType.FS_API);
  assertFalse(!!resultEntry.metadata.isRestrictedForDestination);

  // Cache a file via changeSelection.
  changeSelection(store, [file1]);
  resultEntry = store.getState().allEntries[file1.toURL()];
  assertTrue(!!resultEntry);
  assertEquals(resultEntry.entry, file1);
  assertFalse(resultEntry.isDirectory);
  assertEquals(resultEntry.label, file1.name);
  assertEquals(resultEntry.volumeId, fakeMyFilesVolumeId);
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
  assertEquals(resultEntry.volumeId, null);
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
  assertEquals(resultEntry.volumeId, fakeMyFilesVolumeId);
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

/**
 * Tests that getMyFiles will return the entry list if the volume is not in the
 * store.
 */
export function testGetMyFilesWithFakeEntryList() {
  const currentState = getEmptyState();
  // Add fake entry list MyFiles to the store.
  const myFilesEntryList = createMyFilesDataWithEntryList();
  currentState.allEntries[myFilesEntryListKey] = myFilesEntryList;
  const {myFilesEntry, myFilesVolume} = getMyFiles(currentState);
  // Expect MyFiles entry list returned, no volume.
  assertEquals(myFilesEntryList.entry, myFilesEntry);
  assertEquals(null, myFilesVolume);
}

/**
 * Tests that getMyFiles will return the volume entry if the volume is already
 * in the store.
 */
export function testGetMyFilesWithVolumeEntry() {
  const currentState = getEmptyState();
  // Add MyFiles volume to the store.
  const {fileData, volumeInfo} = createMyFilesDataWithVolumeEntry();
  const volumeMetadata = createFakeVolumeMetadata(volumeInfo);
  const volume =
      convertVolumeInfoAndMetadataToVolume(volumeInfo, volumeMetadata);
  currentState.allEntries[fileData.entry.toURL()] = fileData;
  currentState.volumes[volumeInfo.volumeId] = volume;
  const {myFilesEntry, myFilesVolume} = getMyFiles(currentState);
  // Expect MyFiles volume entry and volume returned.
  assertEquals(fileData.entry, myFilesEntry);
  assertEquals(volume, myFilesVolume);
}

/**
 * Tests that getMyFiles will create a entry list if no MyFiles entry
 * in the store.
 */
export function testGetMyFilesCreateEntryList() {
  const currentState = getEmptyState();
  const {myFilesEntry, myFilesVolume} = getMyFiles(currentState);
  // Expect entry list is created in place.
  const myFilesFileData: FileData =
      currentState.allEntries[myFilesEntryListKey];
  assertNotEquals(undefined, myFilesFileData);
  const myFIlesEntryList = myFilesFileData.entry as EntryList;
  assertEquals(
      VolumeManagerCommon.RootType.MY_FILES, myFIlesEntryList.rootType);
  assertEquals(myFIlesEntryList, myFilesEntry);
  assertEquals(null, myFilesVolume);
}

/** Tests that child entries can be added to the store correctly. */
export async function testAddChildEntries(done: () => void) {
  const initialState = getEmptyState();

  // Add parent/children entries to the store.
  fileSystem.populate([
    '/a/',
    '/a/1/',
    '/a/2/',
    '/a/2/b/',
  ]);
  const aEntry = fileSystem.entries['/a'];
  initialState.allEntries[aEntry.toURL()] = convertEntryToFileData(aEntry);
  // Make sure aEntry won't be cleared.
  initialState.uiEntries.push(aEntry.toURL());

  const store = setupStore(initialState);

  // Dispatch an action to add child entries for /aaa/.
  const a1Entry = fileSystem.entries['/a/1'];
  const a2Entry = fileSystem.entries['/a/2'];
  store.dispatch(addChildEntries({
    parentKey: aEntry.toURL(),
    entries: [a1Entry, a2Entry],
  }));

  // Expect the children filed of /a is updated.
  const want1: State['allEntries'] = {
    [aEntry.toURL()]: {
      ...convertEntryToFileData(aEntry),
      children: [a1Entry.toURL(), a2Entry.toURL()],
    },
    [a1Entry.toURL()]: convertEntryToFileData(a1Entry),
    [a2Entry.toURL()]: convertEntryToFileData(a2Entry),
  };
  await waitDeepEquals(store, want1, (state) => state.allEntries);

  // Set shouldDelayLoadingChildren=true for /a/2.
  store.getState().allEntries[a2Entry.toURL()].shouldDelayLoadingChildren =
      true;
  // Dispatch an action to add child entries for /a/2.
  const bEntry = fileSystem.entries['/a/2/b'];
  store.dispatch(addChildEntries({
    parentKey: a2Entry.toURL(),
    entries: [bEntry],
  }));

  // Expect child entry /a/2/b also has shouldDelayLoadingChildren=true.
  const want2: State['allEntries'] = {
    ...want1,
    [a2Entry.toURL()]: {
      ...convertEntryToFileData(a2Entry),
      shouldDelayLoadingChildren: true,
      children: [bEntry.toURL()],
    },
    [bEntry.toURL()]: {
      ...convertEntryToFileData(bEntry),
      shouldDelayLoadingChildren: true,
    },
  };
  await waitDeepEquals(store, want2, (state) => state.allEntries);

  // Dispatch an action to add child entries for non-existed parent entry.
  store.dispatch(addChildEntries({
    parentKey: 'non-exist-key',
    entries: [a1Entry],
  }));

  // Expect nothing changes in the store.
  await waitDeepEquals(store, want2, (state) => state.allEntries);

  done();
}

/** Tests converting VolumeEntry into FileData. */
export async function testConvertVolumeEntryToFileData(done: () => void) {
  const {volumeManager} = window.fileManager;
  const downloadsVolumeInfo = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DOWNLOADS)!;
  const downloadsEntry = new VolumeEntry(downloadsVolumeInfo);
  const got = convertEntryToFileData(downloadsEntry);
  const want: FileData = {
    entry: downloadsEntry,
    icon: constants.ICON_TYPES.MY_FILES,
    type: EntryType.VOLUME_ROOT,
    isDirectory: true,
    label: 'Downloads',
    volumeId: fakeMyFilesVolumeId,
    rootType: VolumeManagerCommon.RootType.DOWNLOADS,
    metadata: {} as MetadataItem,
    expanded: false,
    disabled: false,
    isRootEntry: true,
    isEjectable: false,
    shouldDelayLoadingChildren: false,
    children: [],
  };
  assertDeepEquals(want, got);

  // Now disabled the volume in the volume manager.
  volumeManager.isDisabled = (volumeType) =>
      volumeType === VolumeManagerCommon.VolumeType.DOWNLOADS;
  const fileData = convertEntryToFileData(downloadsEntry);
  assertEquals(true, fileData.disabled);

  done();
}

/** Tests converting EntryList into FileData. */
export async function testConvertEntryListToFileData(done: () => void) {
  const myFilesEntryList =
      new EntryList('My files', VolumeManagerCommon.RootType.MY_FILES);
  const got = convertEntryToFileData(myFilesEntryList);
  const want: FileData = {
    entry: myFilesEntryList,
    icon: constants.ICON_TYPES.MY_FILES,
    type: EntryType.ENTRY_LIST,
    isDirectory: true,
    label: 'My files',
    volumeId: null,  // No volume info for the entry list.
    rootType: VolumeManagerCommon.RootType.MY_FILES,
    metadata: {} as MetadataItem,
    expanded: false,
    disabled: false,
    isRootEntry: true,
    isEjectable: false,
    shouldDelayLoadingChildren: false,
    children: [],
  };
  assertDeepEquals(want, got);

  done();
}

/** Tests converting FakeEntry into FileData. */
export async function testConvertFakeEntryToFileData(done: () => void) {
  const androidFakeEntry = new GuestOsPlaceholder(
      'Android files', 0, chrome.fileManagerPrivate.VmType.ARCVM);
  const got = convertEntryToFileData(androidFakeEntry);
  const want: FileData = {
    entry: androidFakeEntry,
    icon: constants.ICON_TYPES.ANDROID_FILES,
    type: EntryType.PLACEHOLDER,
    isDirectory: true,
    label: 'Android files',
    volumeId: null,  // No volume info for the placeholder entry.
    rootType: VolumeManagerCommon.RootType.GUEST_OS,
    metadata: {} as MetadataItem,
    expanded: false,
    disabled: false,
    isRootEntry: true,
    isEjectable: false,
    shouldDelayLoadingChildren: false,
    children: [],
  };
  assertDeepEquals(want, got);

  done();
}

/** Tests converting native file entry into FileData. */
export async function testConvertNativeFileEntryToFileData(done: () => void) {
  const fileEntry = fileSystem.entries['/dir-2/file-1.txt'];
  const got = convertEntryToFileData(fileEntry);
  const want: FileData = {
    entry: fileEntry,
    icon: 'text',
    type: EntryType.FS_API,
    isDirectory: false,
    label: 'file-1.txt',
    volumeId: fakeMyFilesVolumeId,
    rootType: VolumeManagerCommon.RootType.DOWNLOADS,
    metadata: {} as MetadataItem,
    expanded: false,
    disabled: false,
    isRootEntry: false,
    isEjectable: false,
    shouldDelayLoadingChildren: false,
    children: [],
  };
  assertDeepEquals(want, got);

  done();
}

/** Tests converting native directory entry into FileData. */
export async function testConvertNativeDirectoryEntryToFileData(
    done: () => void) {
  const directoryEntry = fileSystem.entries['/dir-1'];
  const got = convertEntryToFileData(directoryEntry);
  const want: FileData = {
    entry: directoryEntry,
    icon: constants.ICON_TYPES.FOLDER,
    type: EntryType.FS_API,
    isDirectory: true,
    label: 'dir-1',
    volumeId: fakeMyFilesVolumeId,
    rootType: VolumeManagerCommon.RootType.DOWNLOADS,
    metadata: {} as MetadataItem,
    expanded: false,
    disabled: false,
    isRootEntry: false,
    isEjectable: false,
    shouldDelayLoadingChildren: false,
    children: [],
  };
  assertDeepEquals(want, got);

  done();
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
