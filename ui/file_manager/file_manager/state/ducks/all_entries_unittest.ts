// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {fakeMyFilesVolumeId, MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import type {VolumeInfo} from '../../background/js/volume_info.js';
import {EntryList, FakeEntryImpl, GuestOsPlaceholder, VolumeEntry} from '../../common/js/files_app_entry_types.js';
import {installMockChrome} from '../../common/js/mock_chrome.js';
import {MockFileSystem} from '../../common/js/mock_entry.js';
import {waitUntil} from '../../common/js/test_error_reporting.js';
import {RootType, VolumeType} from '../../common/js/volume_manager_types.js';
import {ICON_TYPES} from '../../foreground/js/constants.js';
import {MetadataItem} from '../../foreground/js/metadata/metadata_item.js';
import type {MockMetadataModel} from '../../foreground/js/metadata/mock_metadata.js';
import {EntryType, type FileData, type MaterializedView, type State} from '../../state/state.js';
import {allEntriesSize, assertAllEntriesEqual, cd, changeSelection, createFakeVolumeMetadata, setUpFileManagerOnWindow, setupStore, updMetadata, waitDeepEquals} from '../for_tests.js';
import {getEmptyState, type Store} from '../store.js';

import {addChildEntries, cacheMaterializedViews, clearCachedEntries, convertEntryToFileData, getMyFiles, readSubDirectories, traverseAndExpandPathEntriesInternal, updateFileData} from './all_entries.js';
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
                       VolumeType.DOWNLOADS)!.fileSystem as MockFileSystem;
  fileSystem.populate(
      [
        '/dir-1/',
        '/dir-2/sub-dir/',
        '/dir-2/file-1.txt',
        '/dir-2/file-2.txt',
        '/dir-3/',
      ],
      /* opt_clear= */ true);
}

/** Generate MyFiles entry with fake entry list. */
function createMyFilesDataWithEntryList(): FileData {
  const myFilesEntryList = new EntryList('My files', RootType.MY_FILES);
  return convertEntryToFileData(myFilesEntryList);
}

/** Generate MyFiles entry with real volume entry. */
function createMyFilesDataWithVolumeEntry():
    {fileData: FileData, volumeInfo: VolumeInfo} {
  const volumeManager = new MockVolumeManager();
  const downloadsVolumeInfo =
      volumeManager.getCurrentProfileVolumeInfo(VolumeType.DOWNLOADS)!;
  const myFilesVolumeEntry = new VolumeEntry(downloadsVolumeInfo);
  const fileData = convertEntryToFileData(myFilesVolumeEntry);
  return {fileData, volumeInfo: downloadsVolumeInfo};
}

/** Tests that entries get cached in the allEntries. */
export function testAllEntries() {
  assertEquals(
      0, allEntriesSize(store.getState()), 'allEntries should start empty');

  const dir1 = fileSystem.entries['/dir-1'] as DirectoryEntry;
  const dir2 = fileSystem.entries['/dir-2'] as DirectoryEntry;
  const dir2SubDir = fileSystem.entries['/dir-2/sub-dir'] as DirectoryEntry;
  cd(store, dir1);
  assertEquals(1, allEntriesSize(store.getState()), 'dir-1 should be cached');

  cd(store, dir2);
  assertEquals(2, allEntriesSize(store.getState()), 'dir-2 should be cached');

  cd(store, dir2SubDir);
  assertEquals(
      3, allEntriesSize(store.getState()), 'dir-2/sub-dir/ should be cached');
}

export async function testClearStaleEntries(done: () => void) {
  const dir1 = fileSystem.entries['/dir-1'] as DirectoryEntry;
  const dir2 = fileSystem.entries['/dir-2'] as DirectoryEntry;
  const dir3 = fileSystem.entries['/dir-3'] as DirectoryEntry;
  const dir2SubDir = fileSystem.entries['/dir-2/sub-dir'] as DirectoryEntry;
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
  const dir1 = fileSystem.entries['/dir-1'] as DirectoryEntry;
  const file1 = fileSystem.entries['/dir-2/file-1.txt']!;
  const md = window.fileManager.metadataModel as unknown as MockMetadataModel;
  md.set(file1, {isRestrictedForDestination: true});

  // Cache a directory via changeDirectory.
  cd(store, dir1);
  let resultEntry: FileData = store.getState().allEntries[dir1.toURL()]!;
  assertTrue(!!resultEntry);
  assertEquals(resultEntry.entry, dir1);
  assertTrue(resultEntry.isDirectory);
  assertEquals(resultEntry.label, dir1.name);
  assertEquals(resultEntry.volumeId, fakeMyFilesVolumeId);
  assertEquals(resultEntry.type, EntryType.FS_API);
  assertFalse(!!resultEntry.metadata.isRestrictedForDestination);

  // Cache a file via changeSelection.
  changeSelection(store, [file1]);
  resultEntry = store.getState().allEntries[file1.toURL()]!;
  assertTrue(!!resultEntry);
  assertEquals(resultEntry.entry, file1);
  assertFalse(resultEntry.isDirectory);
  assertEquals(resultEntry.label, file1.name);
  assertEquals(resultEntry.volumeId, fakeMyFilesVolumeId);
  assertEquals(resultEntry.type, EntryType.FS_API);
  assertTrue(!!resultEntry.metadata.isRestrictedForDestination);
  const recentRoot = new FakeEntryImpl('Recent', RootType.RECENT);
  cd(store, recentRoot);
  resultEntry = store.getState().allEntries[recentRoot.toURL()]!;
  assertTrue(!!resultEntry);
  assertEquals(resultEntry.entry, recentRoot);
  assertTrue(resultEntry.isDirectory);
  assertEquals(resultEntry.label, recentRoot.name);
  assertEquals(resultEntry.volumeId, null);
  assertEquals(resultEntry.type, EntryType.RECENT);

  const volumeInfo =
      window.fileManager.volumeManager.getCurrentProfileVolumeInfo(
          VolumeType.DOWNLOADS)!;
  const volumeEntry = new VolumeEntry(volumeInfo);
  cd(store, volumeEntry);
  resultEntry = store.getState().allEntries[volumeEntry.toURL()]!;
  assertTrue(!!resultEntry);
  assertEquals(resultEntry.entry, volumeEntry);
  assertTrue(resultEntry.isDirectory);
  assertEquals(resultEntry.label, volumeEntry.name);
  assertEquals(resultEntry.volumeId, fakeMyFilesVolumeId);
  assertEquals(resultEntry.type, EntryType.VOLUME_ROOT);
  assertFalse(!!resultEntry.metadata.isRestrictedForDestination);
}

export function testUpdateMetadata() {
  const dir1 = fileSystem.entries['/dir-1'] as DirectoryEntry;
  const file1 = fileSystem.entries['/dir-2/file-1.txt']!;
  const file2 = fileSystem.entries['/dir-2/file-2.txt']!;
  const md = window.fileManager.metadataModel as unknown as MockMetadataModel;
  md.set(file1, {isRestrictedForDestination: true});
  md.set(file2, {isRestrictedForDestination: false});

  // Cache a directory via changeDirectory.
  cd(store, dir1);
  assertEquals(1, allEntriesSize(store.getState()));
  let resultEntry = store.getState().allEntries[dir1.toURL()]!;
  assertEquals(undefined, resultEntry.metadata.isRestrictedForDestination);
  assertEquals(undefined, resultEntry.metadata.isDlpRestricted);

  // Cache a file via changeSelection.
  changeSelection(store, [file1]);
  assertEquals(2, allEntriesSize(store.getState()));
  resultEntry = store.getState().allEntries[file1.toURL()]!;
  assertTrue(!!resultEntry.metadata.isRestrictedForDestination);
  assertEquals(undefined, resultEntry.metadata.isDlpRestricted);

  // Update the metadata: it should first cache missing entries, and append the
  // new metadata to the already fetched values.
  const metadata: MetadataItem = new MetadataItem();
  metadata.isDlpRestricted = true;
  updMetadata(store, [{entry: file1, metadata}, {entry: file2, metadata}]);

  resultEntry = store.getState().allEntries[dir1.toURL()]!;
  assertEquals(undefined, resultEntry.metadata.isRestrictedForDestination);
  assertEquals(undefined, resultEntry.metadata.isDlpRestricted);

  resultEntry = store.getState().allEntries[file1.toURL()]!;
  assertTrue(!!resultEntry.metadata.isRestrictedForDestination);
  assertTrue(!!resultEntry.metadata.isDlpRestricted);

  resultEntry = store.getState().allEntries[file2.toURL()]!;
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
  currentState.allEntries[fileData.key] = fileData;
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
  const myFilesFileData = currentState.allEntries[myFilesEntryListKey]!;
  assertNotEquals(undefined, myFilesFileData);
  const myFIlesEntryList = myFilesFileData.entry as EntryList;
  assertEquals(RootType.MY_FILES, myFIlesEntryList.rootType);
  assertEquals(myFIlesEntryList, myFilesEntry);
  assertEquals(null, myFilesVolume);
}

/** Tests that child entries can be added to the store correctly. */
export async function testAddChildEntries(done: () => void) {
  const initialState = getEmptyState();

  // Add parent/children entries to the store.
  fileSystem.populate(
      [
        '/a/',
        '/a/1/',
        '/a/2/',
        '/a/2/b/',
      ],
      /* opt_clear= */ true);
  const aEntry = fileSystem.entries['/a']!;
  initialState.allEntries[aEntry.toURL()] = convertEntryToFileData(aEntry);
  // Make sure aEntry won't be cleared.
  initialState.uiEntries.push(aEntry.toURL());

  const store = setupStore(initialState);

  // Dispatch an action to add child entries for /aaa/.
  const a1Entry = fileSystem.entries['/a/1']!;
  const a2Entry = fileSystem.entries['/a/2']!;
  store.dispatch(addChildEntries({
    parentKey: aEntry.toURL(),
    entries: [a1Entry, a2Entry],
  }));

  // Expect the children filed of /a is updated.
  const want1: State['allEntries'] = {
    [aEntry.toURL()]: {
      ...convertEntryToFileData(aEntry),
      children: [a1Entry.toURL(), a2Entry.toURL()],
      canExpand: true,
    },
    [a1Entry.toURL()]: convertEntryToFileData(a1Entry),
    [a2Entry.toURL()]: convertEntryToFileData(a2Entry),
  };
  await waitDeepEquals(store, want1, (state) => state.allEntries);

  // Dispatch an action to add child entries for /a/2.
  const bEntry = fileSystem.entries['/a/2/b']!;
  store.dispatch(addChildEntries({
    parentKey: a2Entry.toURL(),
    entries: [bEntry],
  }));

  const want2: State['allEntries'] = {
    ...want1,
    [a2Entry.toURL()]: {
      ...convertEntryToFileData(a2Entry),
      children: [bEntry.toURL()],
      canExpand: true,
    },
    [bEntry.toURL()]: {
      ...convertEntryToFileData(bEntry),
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
  const downloadsVolumeInfo =
      volumeManager.getCurrentProfileVolumeInfo(VolumeType.DOWNLOADS)!;
  const downloadsEntry = new VolumeEntry(downloadsVolumeInfo);
  const got = convertEntryToFileData(downloadsEntry);
  const want: FileData = {
    key: downloadsEntry.toURL(),
    fullPath: downloadsEntry.fullPath,
    entry: downloadsEntry,
    icon: ICON_TYPES.MY_FILES,
    type: EntryType.VOLUME_ROOT,
    isDirectory: true,
    label: 'Downloads',
    volumeId: fakeMyFilesVolumeId,
    rootType: RootType.DOWNLOADS,
    metadata: {} as MetadataItem,
    expanded: false,
    disabled: false,
    isRootEntry: true,
    isEjectable: false,
    canExpand: false,
    children: [],
  };
  assertDeepEquals(want, got);

  // Now disabled the volume in the volume manager.
  volumeManager.isDisabled = (volumeType) =>
      volumeType === VolumeType.DOWNLOADS;
  const fileData = convertEntryToFileData(downloadsEntry);
  assertEquals(true, fileData.disabled);

  done();
}

/**
 * Tests the icon for DocumentsProvider FileData should be a generic icon with
 * an empty IconSet provided.
 */
export async function testGenericIconInDocumentsProviderFileData(
    done: () => void) {
  // By default an empty IconSet is used in this mock function.
  const documentsProviderVolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeType.DOCUMENTS_PROVIDER, 'documentProviderId', 'Google Photos');
  const {volumeManager} = window.fileManager;
  volumeManager.volumeInfoList.add(documentsProviderVolumeInfo);
  const documentsProviderEntry = new VolumeEntry(documentsProviderVolumeInfo);
  const got = convertEntryToFileData(documentsProviderEntry);
  const want: FileData = {
    key: documentsProviderEntry.toURL(),
    fullPath: documentsProviderEntry.fullPath,
    entry: documentsProviderEntry,
    icon: ICON_TYPES.GENERIC,
    type: EntryType.VOLUME_ROOT,
    isDirectory: true,
    label: 'Google Photos',
    volumeId: 'documentProviderId',
    rootType: RootType.DOCUMENTS_PROVIDER,
    metadata: {} as MetadataItem,
    expanded: false,
    disabled: false,
    isRootEntry: true,
    isEjectable: false,
    canExpand: false,
    children: [],
  };
  assertDeepEquals(want, got);

  done();
}

/** Tests converting EntryList into FileData. */
export async function testConvertEntryListToFileData(done: () => void) {
  const myFilesEntryList = new EntryList('My files', RootType.MY_FILES);
  const got = convertEntryToFileData(myFilesEntryList);
  const want: FileData = {
    key: myFilesEntryList.toURL(),
    fullPath: myFilesEntryList.fullPath,
    entry: myFilesEntryList,
    icon: ICON_TYPES.MY_FILES,
    type: EntryType.ENTRY_LIST,
    isDirectory: true,
    label: 'My files',
    volumeId: null,  // No volume info for the entry list.
    rootType: RootType.MY_FILES,
    metadata: {} as MetadataItem,
    expanded: false,
    disabled: false,
    isRootEntry: true,
    isEjectable: false,
    canExpand: false,
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
    key: androidFakeEntry.toURL(),
    fullPath: androidFakeEntry.fullPath,
    entry: androidFakeEntry,
    icon: ICON_TYPES.ANDROID_FILES,
    type: EntryType.PLACEHOLDER,
    isDirectory: true,
    label: 'Android files',
    volumeId: null,  // No volume info for the placeholder entry.
    rootType: RootType.GUEST_OS,
    metadata: {} as MetadataItem,
    expanded: false,
    disabled: false,
    isRootEntry: true,
    isEjectable: false,
    canExpand: false,
    children: [],
  };
  assertDeepEquals(want, got);

  done();
}

/** Tests converting native file entry into FileData. */
export async function testConvertNativeFileEntryToFileData(done: () => void) {
  const fileEntry = fileSystem.entries['/dir-2/file-1.txt']!;
  const got = convertEntryToFileData(fileEntry);
  const want: FileData = {
    key: fileEntry.toURL(),
    fullPath: fileEntry.fullPath,
    entry: fileEntry,
    icon: 'text',
    type: EntryType.FS_API,
    isDirectory: false,
    label: 'file-1.txt',
    volumeId: fakeMyFilesVolumeId,
    rootType: RootType.DOWNLOADS,
    metadata: {} as MetadataItem,
    expanded: false,
    disabled: false,
    isRootEntry: false,
    isEjectable: false,
    canExpand: false,
    children: [],
  };
  assertDeepEquals(want, got);

  done();
}

/** Tests converting native directory entry into FileData. */
export async function testConvertNativeDirectoryEntryToFileData(
    done: () => void) {
  const directoryEntry = fileSystem.entries['/dir-1']!;
  const got = convertEntryToFileData(directoryEntry);
  const want: FileData = {
    key: directoryEntry.toURL(),
    fullPath: directoryEntry.fullPath,
    entry: directoryEntry,
    icon: ICON_TYPES.FOLDER,
    type: EntryType.FS_API,
    isDirectory: true,
    label: 'dir-1',
    volumeId: fakeMyFilesVolumeId,
    rootType: RootType.DOWNLOADS,
    metadata: {} as MetadataItem,
    expanded: false,
    disabled: false,
    isRootEntry: false,
    isEjectable: false,
    canExpand: false,
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
  const downloadsVolumeInfo =
      volumeManager.getCurrentProfileVolumeInfo(VolumeType.DOWNLOADS)!;
  const fakeFs = downloadsVolumeInfo.fileSystem as MockFileSystem;
  fakeFs.populate(
      [
        '/Downloads/',
        '/Downloads/c/',
        '/Downloads/b.txt',
        '/Downloads/a/',
      ],
      /* opt_clear= */ true);
  const downloadsEntry = fakeFs.entries['/Downloads']!;
  // The entry to be read should be in the store before reading.
  const downloadsEntryFileData = convertEntryToFileData(downloadsEntry);
  initialState.allEntries[downloadsEntry.toURL()] = downloadsEntryFileData;
  // Put it in the uiEntries so it won't be cleared.
  initialState.uiEntries.push(downloadsEntry.toURL());
  const store = setupStore(initialState);

  // Dispatch read sub directories action producer.
  store.dispatch(readSubDirectories(downloadsEntry.toURL()));

  // Expect store to have all its sub directories.
  const aDirEntry = fakeFs.entries['/Downloads/a']!;
  const cDirEntry = fakeFs.entries['/Downloads/c']!;
  const want: State['allEntries'] = {
    [downloadsEntry.toURL()]: {...downloadsEntryFileData, canExpand: true},
    [aDirEntry.toURL()]: convertEntryToFileData(aDirEntry),
    [cDirEntry.toURL()]: convertEntryToFileData(cDirEntry),
  };
  want[downloadsEntry.toURL()]!.children =
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
  const downloadsVolumeInfo =
      volumeManager.getCurrentProfileVolumeInfo(VolumeType.DOWNLOADS)!;
  const fakeFs = downloadsVolumeInfo.fileSystem as MockFileSystem;
  fakeFs.populate(
      [
        '/Downloads/',
        '/Downloads/a/',
        '/Downloads/b/',
        '/Downloads/a/111/',
        '/Downloads/b/222/',
      ],
      /* opt_clear= */ true);
  const downloadsEntry = fakeFs.entries['/Downloads']!;
  const bDirEntry = fakeFs.entries['/Downloads/b']!;
  // The entry to be read should be in the store before reading.
  const downloadsEntryFileData = convertEntryToFileData(downloadsEntry);
  const bEntryFileData = convertEntryToFileData(bDirEntry);
  // Set expanded = true, so it will be read deeper.
  downloadsEntryFileData.expanded = true;
  bEntryFileData.expanded = true;
  initialState.allEntries[downloadsEntry.toURL()] = downloadsEntryFileData;
  initialState.allEntries[bDirEntry.toURL()] = bEntryFileData;
  const store = setupStore(initialState);

  // Dispatch read sub directories action producer.
  store.dispatch(
      readSubDirectories(downloadsEntry.toURL(), /* recursive= */ true));

  // Expect store to have all its sub directories.
  const aDirEntry = fakeFs.entries['/Downloads/a']!;
  const dirEntry2 = fakeFs.entries['/Downloads/b/222']!;
  const want: State['allEntries'] = {
    [downloadsEntry.toURL()]: {
      ...downloadsEntryFileData,
      children: [aDirEntry.toURL(), bDirEntry.toURL()],
      canExpand: true,
    },
    [aDirEntry.toURL()]: {
      ...convertEntryToFileData(aDirEntry),
      // Partial scan for a/ (no `children` but `canExpand: true`) because it's
      // not expanded.
      canExpand: true,
      children: [],
    },
    // Full scan for b/ (updated `children`) because it's expanded.
    [bDirEntry.toURL()]: {
      ...bEntryFileData,
      children: [dirEntry2.toURL()],
      canExpand: true,
    },
    [dirEntry2.toURL()]: convertEntryToFileData(dirEntry2),
    // Entry /a/111/ is not here because its parent a/ is not expanded.
  };

  await waitDeepEquals(store, want, (state) => state.allEntries);

  done();
}

/** Tests that reading a null entry does nothing. */
export async function testReadSubDirectoriesWithNullEntry(done: () => void) {
  const store = setupStore();

  // Check reading null entry will do nothing.
  store.dispatch(readSubDirectories(''));

  await waitDeepEquals(store, {}, (state) => state.allEntries);

  done();
}

/** Tests that reading a non directory entry does nothing. */
export async function testReadSubDirectoriesWithNonDirectoryEntry(
    done: () => void) {
  const store = setupStore();

  // Populate a fake file entry in the file system.
  const fakeFs = new MockFileSystem('fake-fs');
  fakeFs.populate(
      [
        '/a.txt',
      ],
      /* opt_clear= */ true);

  // Check reading non directory entry will do nothing.
  store.dispatch(readSubDirectories(fakeFs.entries['/a.txt']!.toURL()));

  await waitDeepEquals(store, {}, (state) => state.allEntries);

  done();
}

/** Tests that reading a disabled entry does nothing. */
export async function testReadSubDirectoriesWithDisabledEntry(
    done: () => void) {
  const store = setupStore();

  // Make downloadsEntry as disabled.
  const {volumeManager} = window.fileManager;
  const downloadsVolumeInfo =
      volumeManager.getCurrentProfileVolumeInfo(VolumeType.DOWNLOADS)!;
  const downloadsEntry = new VolumeEntry(downloadsVolumeInfo);
  downloadsEntry.disabled = true;

  // Check reading disabled volume entry will do nothing.
  store.dispatch(readSubDirectories(downloadsEntry.toURL()));

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
  const driveVolumeInfo =
      volumeManager.getCurrentProfileVolumeInfo(VolumeType.DRIVE)!;
  const driveFs = driveVolumeInfo.fileSystem as MockFileSystem;

  // Create drive root entry list and add all its children.
  const driveRootEntryList =
      new EntryList('Google Drive', RootType.DRIVE_FAKE_ROOT);
  const sharedWithMeEntry =
      new FakeEntryImpl('Shared with me', RootType.DRIVE_SHARED_WITH_ME);
  const offlineEntry = new FakeEntryImpl('Offline', RootType.DRIVE_OFFLINE);
  const driveEntry = driveFs.entries['/root']!;
  const computersEntry = driveFs.entries['/Computers']!;
  driveRootEntryList.addEntry(driveEntry);
  driveRootEntryList.addEntry(driveFs.entries['/team_drives']!);
  driveRootEntryList.addEntry(computersEntry);
  driveRootEntryList.addEntry(sharedWithMeEntry);
  driveRootEntryList.addEntry(offlineEntry);
  // Add child entries.
  driveFs.populate(
      [
        '/root/a/',
        '/root/b.txt',
        '/Computers/c.txt',
      ],
      /* opt_clear= */ true);
  // Drive root entry list needs to be in the store before reading.
  const fakeDriveEntryFileData = convertEntryToFileData(driveRootEntryList);
  initialState.allEntries[driveRootEntryList.toURL()] = fakeDriveEntryFileData;
  // Put it in the uiEntries so it won't be cleared.
  initialState.uiEntries.push(driveRootEntryList.toURL());
  const store = setupStore(initialState);

  // Dispatch read sub directories action producer.
  store.dispatch(readSubDirectories(driveRootEntryList.toURL()));

  // Expect its direct sub directories and grand sub directories of /Computers
  // should be in the store.
  const want: State['allEntries'] = {
    [driveRootEntryList.toURL()]: {...fakeDriveEntryFileData, canExpand: true},
    [driveEntry.toURL()]: convertEntryToFileData(driveEntry),
    [computersEntry.toURL()]: convertEntryToFileData(computersEntry),
    [sharedWithMeEntry.toURL()]: convertEntryToFileData(sharedWithMeEntry),
    [offlineEntry.toURL()]: convertEntryToFileData(offlineEntry),
    // /team_drives/ won't be here because it doesn't have children.
  };
  want[driveRootEntryList.toURL()]!.children = [
    driveEntry.toURL(),
    computersEntry.toURL(),
    sharedWithMeEntry.toURL(),
    offlineEntry.toURL(),
  ];
  // /root children won't be read.

  await waitDeepEquals(store, want, (state) => state.allEntries);

  done();
}

/**
 * Tests that traverse path entries will read entries for each parent and
 * expand them if the child entry could be found.
 */
export async function testTraverseAndExpandPathEntriesFound(
    done: VoidCallback) {
  const initialState = getEmptyState();
  const {volumeManager} = window.fileManager;
  // Populate some fake entries.
  const downloadsVolumeInfo =
      volumeManager.getCurrentProfileVolumeInfo(VolumeType.DOWNLOADS)!;
  const fakeFs = downloadsVolumeInfo.fileSystem as MockFileSystem;
  fakeFs.populate(
      [
        '/a/',
        '/a/b/',
        '/a/b/c/',
      ],
      /* opt_clear= */ true);
  const volumeRootEntry = fakeFs.entries['/']!;
  const dirA = fakeFs.entries['/a']!;
  const dirB = fakeFs.entries['/a/b']!;
  const dirC = fakeFs.entries['/a/b/c']!;

  // Put the volume root and the last child entry in the store.
  const volumeRootEntryFileData = convertEntryToFileData(volumeRootEntry);
  initialState.allEntries[volumeRootEntry.toURL()] = volumeRootEntryFileData;
  initialState.volumes[downloadsVolumeInfo.volumeId] =
      convertVolumeInfoAndMetadataToVolume(
          downloadsVolumeInfo, createFakeVolumeMetadata(downloadsVolumeInfo));
  const dirCEntryFileData = convertEntryToFileData(dirC);
  initialState.allEntries[dirC.toURL()] = dirCEntryFileData;

  const store = setupStore(initialState);

  // Dispatch action producer to traverse on the entry path.
  store.dispatch(traverseAndExpandPathEntriesInternal([
    volumeRootEntry.toURL(),
    dirA.toURL(),
    dirB.toURL(),
    dirC.toURL(),
  ]));

  // Expect dirA and dirB will be in the store and expanded.
  const want: State['allEntries'] = {
    [volumeRootEntry.toURL()]: {
      ...volumeRootEntryFileData,
      expanded: true,
      children: [dirA.toURL()],
      canExpand: true,
    },
    [dirA.toURL()]: {
      ...convertEntryToFileData(dirA),
      expanded: true,
      children: [dirB.toURL()],
      canExpand: true,
    },
    [dirB.toURL()]: {
      ...convertEntryToFileData(dirB),
      expanded: true,
      children: [dirC.toURL()],
      canExpand: true,
    },
    [dirC.toURL()]: {
      ...convertEntryToFileData(dirC),
      expanded: false,
      children: [],
      canExpand: false,
    },
  };

  await waitDeepEquals(store, want, (state) => state.allEntries);

  done();
}

/**
 * Tests that traverse path entries will read entries for each parent, it
 * won't expand any parent entry if the child entry can not be found.
 */
export async function testTraverseAndExpandPathEntriesNotFound(
    done: VoidCallback) {
  const initialState = getEmptyState();
  const {volumeManager} = window.fileManager;
  // Populate some fake entries.
  const downloadsVolumeInfo =
      volumeManager.getCurrentProfileVolumeInfo(VolumeType.DOWNLOADS)!;
  const fakeFs = downloadsVolumeInfo.fileSystem as MockFileSystem;
  fakeFs.populate(
      [
        '/a/',
        '/a/b/',
        '/a/b/c/',
      ],
      /* opt_clear= */ true);
  const volumeRootEntry = fakeFs.entries['/']!;
  const dirA = fakeFs.entries['/a']!;
  const dirB = fakeFs.entries['/a/b']!;
  const dirC = fakeFs.entries['/a/b/c']!;

  // Put the volume root and the last child entry in the store.
  const volumeRootEntryFileData = convertEntryToFileData(volumeRootEntry);
  initialState.allEntries[volumeRootEntry.toURL()] = volumeRootEntryFileData;
  initialState.volumes[downloadsVolumeInfo.volumeId] =
      convertVolumeInfoAndMetadataToVolume(
          downloadsVolumeInfo, createFakeVolumeMetadata(downloadsVolumeInfo));
  const dirCEntryFileData = convertEntryToFileData(dirC);
  initialState.allEntries[dirC.toURL()] = dirCEntryFileData;

  const store = setupStore(initialState);

  // Dispatch action producer to traverse on the entry path.
  store.dispatch(traverseAndExpandPathEntriesInternal([
    volumeRootEntry.toURL(),
    dirA.toURL(),
    'not-exist-url',
    dirC.toURL(),
  ]));

  // Expect dirA and dirB will be in the store but no entry is expanded.
  const want: State['allEntries'] = {
    [volumeRootEntry.toURL()]: {
      ...volumeRootEntryFileData,
      expanded: false,
      children: [dirA.toURL()],
      canExpand: true,
    },
    [dirA.toURL()]: {
      ...convertEntryToFileData(dirA),
      expanded: false,
      children: [dirB.toURL()],
      canExpand: true,
    },
    [dirB.toURL()]: {
      ...convertEntryToFileData(dirB),
      expanded: false,
      // dirB's children is not being read because read stops when non-exist-url
      // is encountered.
      children: [],
      canExpand: false,
    },
    // dirC is cleared because it's not referenced by any other entries.
  };

  await waitDeepEquals(store, want, (state) => state.allEntries);

  done();
}

/** Tests that file data can be updated correctly. */
export async function testUpdateFileData(done: () => void) {
  const initialState = getEmptyState();
  // Add MyFiles entry to the store.
  const {fileData} = createMyFilesDataWithVolumeEntry();
  const myFilesEntryKey = fileData.key;
  initialState.allEntries[myFilesEntryKey] = fileData;

  const store = setupStore(initialState);

  // Dispatch an action to update file data.
  store.dispatch(updateFileData(
      {key: myFilesEntryKey, partialFileData: {expanded: true}}));

  // Expect MyFiles entry is expanded in the store.
  await waitDeepEquals(
      store, true, (state) => state.allEntries[myFilesEntryKey]?.expanded);

  done();
}

/** Tests that file data won't be updated without valid file data. */
export async function testUpdateFileDataWithoutValidFileData(done: () => void) {
  const initialState = getEmptyState();
  const store = setupStore(initialState);

  // Dispatch an action to update an non existed file data.
  store.dispatch(updateFileData(
      {key: 'not-exist-key', partialFileData: {expanded: true}}));

  // Check state won't be touched.
  await waitDeepEquals(store, initialState, (state) => state);

  done();
}

/**
 * Test adding a Materialized View to the allEntries.
 */
export async function testcacheMaterializedViews() {
  const initialState = getEmptyState();
  const store = setupStore(initialState);
  const state = store.getState();
  const key = 'materialized-view://1/';

  const views: MaterializedView[] =
      [{id: '1', key, label: 'test view', isRoot: true, icon: 'the-icon'}];
  cacheMaterializedViews(store.getState(), views);

  const want: FileData = {
    key,
    fullPath: '//1/',
    icon: 'the-icon',
    label: 'test view',
    type: EntryType.MATERIALIZED_VIEW,
    isDirectory: true,
    volumeId: null,
    rootType: null,
    metadata: {},
    isRootEntry: true,
    isEjectable: false,
    canExpand: true,
    children: [],
    expanded: false,
    disabled: false,
  };
  assertDeepEquals(want, state.allEntries[key]);
}
