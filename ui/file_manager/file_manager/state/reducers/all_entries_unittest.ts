// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertArrayEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import {EntryList, FakeEntryImpl, VolumeEntry} from '../../common/js/files_app_entry_types.js';
import {MockFileSystem} from '../../common/js/mock_entry.js';
import {waitUntil} from '../../common/js/test_error_reporting.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {EntryType, FileData} from '../../externs/ts/state.js';
import {VolumeInfo} from '../../externs/volume_info.js';
import {constants} from '../../foreground/js/constants.js';
import {MetadataItem} from '../../foreground/js/metadata/metadata_item.js';
import {MockMetadataModel} from '../../foreground/js/metadata/mock_metadata.js';
import {ActionType} from '../actions.js';
import {addChildEntries as addChildEntriesAction, ClearStaleCachedEntriesAction} from '../actions/all_entries.js';
import {addVolume} from '../actions/volumes.js';
import {allEntriesSize, assertAllEntriesEqual, cd, changeSelection, createFakeFileData, createFakeVolume, createFakeVolumeMetadata, setUpFileManagerOnWindow, setupStore, updMetadata} from '../for_tests.js';
import {getEmptyState, Store} from '../store.js';

import {addChildEntries, cacheEntries, clearCachedEntries, getMyFiles} from './all_entries.js';
import {driveRootEntryListKey, makeRemovableParentKey, myFilesEntryListKey} from './volumes.js';

let store: Store;
let fileSystem: MockFileSystem;

export function setUp() {
  // changeDirectory() reducer uses the VolumeManager.
  setUpFileManagerOnWindow();

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
  return createFakeFileData({
    entry: new EntryList('My files', VolumeManagerCommon.RootType.MY_FILES),
    label: 'My files',
    type: EntryType.ENTRY_LIST,
  });
}

/** Generate MyFiles entry with real volume entry. */
function createMyFilesDataWithVolumeEntry():
    {fileData: FileData, volumeInfo: VolumeInfo} {
  const {volumeManager} = window.fileManager;
  const downloadsVolumeInfo = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DOWNLOADS)!;
  const fileData = createFakeFileData({
    entry: new VolumeEntry(downloadsVolumeInfo),
    volumeType: VolumeManagerCommon.VolumeType.DOWNLOADS,
    label: 'My files',
    type: EntryType.VOLUME_ROOT,
  });
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
  const volume = createFakeVolume({
    volumeType: volumeInfo.volumeType,
    volumeId: volumeInfo.volumeId,
    label: volumeInfo.label,
    rootKey: volumeInfo.displayRoot.toURL(),
  });
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

/** Tests that MyFiles volume entry can be cached correctly. */
export function testCacheEntriesForMyFilesVolume() {
  const currentState = getEmptyState();
  const myFilesFileData = createMyFilesDataWithEntryList();
  const myFilesEntryList = myFilesFileData.entry as EntryList;
  // Put MyFiles entry in the store and add ui entries as its children.
  currentState.allEntries[myFilesEntryList.toURL()] = myFilesFileData;
  const playFilesEntry = new FakeEntryImpl(
      'Play files', VolumeManagerCommon.RootType.ANDROID_FILES);
  myFilesEntryList.addEntry(playFilesEntry);
  const linuxFilesEntry =
      new FakeEntryImpl('Linux files', VolumeManagerCommon.RootType.CROSTINI);
  myFilesEntryList.addEntry(linuxFilesEntry);

  const {volumeManager} = window.fileManager;
  const myFilesVolumeInfo = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DOWNLOADS)!;
  const myFilesVolumeMetadata = createFakeVolumeMetadata({
    volumeId: myFilesVolumeInfo.volumeId,
    volumeType: myFilesVolumeInfo.volumeType,
  });
  cacheEntries(currentState, addVolume({
                 volumeInfo: myFilesVolumeInfo,
                 volumeMetadata: myFilesVolumeMetadata,
               }));

  // cacheEntries() updates state in place.
  const newState = currentState;
  // Expect all existing ui children will be added to the real MyFiles entry.
  const myFilesVolumeEntry: VolumeEntry =
      newState.allEntries[myFilesVolumeInfo.displayRoot!.toURL()].entry;
  const uiChildren = myFilesVolumeEntry.getUIChildren();
  assertEquals(2, uiChildren.length);
  assertEquals(playFilesEntry, uiChildren[0]);
  assertEquals(linuxFilesEntry, uiChildren[1]);
  assertEquals(0, myFilesEntryList.getUIChildren().length);
}

/** Tests that volume nested in MyFiles volume can be cached correctly. */
export function testCacheEntriesForNestedVolumeInMyFilesVolume() {
  const currentState = getEmptyState();
  // Put MyFiles and play files ui entry in the store.
  const {fileData, volumeInfo} = createMyFilesDataWithVolumeEntry();
  const myFilesVolumeEntry = fileData.entry as VolumeEntry;
  const myFilesVolume = createFakeVolume({
    volumeType: volumeInfo.volumeType,
    volumeId: volumeInfo.volumeId,
    label: volumeInfo.label,
    rootKey: volumeInfo.displayRoot.toURL(),
  });
  currentState.allEntries[fileData.entry.toURL()] = fileData;
  currentState.volumes[volumeInfo.volumeId] = myFilesVolume;
  // Placeholder ui entry and the volume entry it represents have the same
  // label.
  const label = 'Play files';
  const playFilesUiEntry =
      new FakeEntryImpl(label, VolumeManagerCommon.RootType.ANDROID_FILES);
  myFilesVolumeEntry.addEntry(playFilesUiEntry);
  fileData.children.push(playFilesUiEntry.toURL());

  const {volumeManager} = window.fileManager;
  const playFilesVolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.ANDROID_FILES, 'playFilesId', label);
  volumeManager.volumeInfoList.add(playFilesVolumeInfo);
  const playFilesVolumeMetadata = createFakeVolumeMetadata({
    volumeType: playFilesVolumeInfo.volumeType,
    volumeId: playFilesVolumeInfo.volumeId,
  });
  cacheEntries(currentState, addVolume({
                 volumeInfo: playFilesVolumeInfo,
                 volumeMetadata: playFilesVolumeMetadata,
               }));
  // cacheEntries() updates state in place.
  const newState = currentState;
  // Expect the new play file volume will be nested inside MyFiles and the old
  // placeholder will be removed.
  const playFilesVolumeEntry =
      newState.allEntries[playFilesVolumeInfo.displayRoot!.toURL()].entry;
  const newMyFilesFileData: FileData =
      newState.allEntries[myFilesVolumeEntry.toURL()];
  assertEquals(1, myFilesVolumeEntry.getUIChildren().length);
  assertEquals(playFilesVolumeEntry, myFilesVolumeEntry.getUIChildren()[0]);
  assertEquals(1, newMyFilesFileData.children.length);
  assertEquals(playFilesVolumeEntry.toURL(), newMyFilesFileData.children[0]);
}

/** Tests that drive volume can be cached correctly. */
export function testAddDriveVolume(done: () => void) {
  const currentState = getEmptyState();

  const {volumeManager} = window.fileManager;
  const driveVolumeInfo = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DRIVE)!;
  const driveVolumeMetadata = createFakeVolumeMetadata({
    volumeType: driveVolumeInfo.volumeType,
    volumeId: driveVolumeInfo.volumeId,
  });
  // DriveFS takes time to resolve.
  driveVolumeInfo.resolveDisplayRoot(() => {
    cacheEntries(currentState, addVolume({
                   volumeInfo: driveVolumeInfo,
                   volumeMetadata: driveVolumeMetadata,
                 }));
    // cacheEntries() updates state in place.
    const newState = currentState;
    // Expect all fake entries inside Drive will be added as its children.
    const driveFakeRootEntry: EntryList =
        newState.allEntries[driveRootEntryListKey].entry;
    assertEquals(
        VolumeManagerCommon.RootType.DRIVE_FAKE_ROOT,
        driveFakeRootEntry.rootType);
    const driveChildren = driveFakeRootEntry.getUIChildren();
    assertEquals(5, driveChildren.length);
    // My Drive.
    const myDriveEntry: VolumeEntry =
        newState.allEntries[driveChildren[0]!.toURL()].entry;
    assertEquals(myDriveEntry, driveChildren[0]);
    // Shared drives root.
    const sharedDrivesRootEntry: DirectoryEntry =
        newState.allEntries[driveChildren[1]!.toURL()].entry;
    assertEquals('/team_drives', sharedDrivesRootEntry.fullPath);
    assertEquals(sharedDrivesRootEntry, driveChildren[1]);
    // Computers root.
    const computersRootEntry: DirectoryEntry =
        newState.allEntries[driveChildren[2]!.toURL()].entry;
    assertEquals('/Computers', computersRootEntry.fullPath);
    assertEquals(computersRootEntry, driveChildren[2]);
    // Shared with me.
    const sharedWithMeEntry: FakeEntryImpl =
        newState.allEntries[driveChildren[3]!.toURL()].entry;
    assertEquals(
        VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME,
        sharedWithMeEntry.rootType);
    assertEquals(sharedWithMeEntry, driveChildren[3]);
    // Offline.
    const offlineEntry: FakeEntryImpl =
        newState.allEntries[driveChildren[4]!.toURL()].entry;
    assertEquals(offlineEntry, driveChildren[4]);
    assertEquals(
        VolumeManagerCommon.RootType.DRIVE_OFFLINE, offlineEntry.rootType);
    assertArrayEquals(
        [sharedWithMeEntry.toURL(), offlineEntry.toURL()], newState.uiEntries);

    done();
  });
}

/** Tests that multiple partition volumes can be cached correctly. */
export function testCacheEntriesForMultipleUsbPartitionsGrouping() {
  const currentState = getEmptyState();
  // Add partition-1 into the store.
  const {volumeManager} = window.fileManager;
  const partition1VolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:partition1',
      'Partition 1', '/device/path/1');
  volumeManager.volumeInfoList.add(partition1VolumeInfo);
  const partition1VolumeEntry = new VolumeEntry(partition1VolumeInfo);
  const partition1FileData = createFakeFileData({
    entry: partition1VolumeEntry,
    label: partition1VolumeInfo.label,
    type: EntryType.VOLUME_ROOT,
  });
  const partition1Volume = createFakeVolume({
    volumeId: partition1VolumeInfo.volumeId,
    volumeType: VolumeManagerCommon.VolumeType.REMOVABLE,
    rootKey: partition1VolumeInfo.displayRoot!.toURL(),
    label: partition1VolumeInfo.label,
    devicePath: partition1VolumeInfo.devicePath,
    driveLabel: 'USB_Drive',
  });
  currentState.volumes[partition1Volume.volumeId] = partition1Volume;
  currentState.allEntries[partition1VolumeEntry.toURL()] = partition1FileData;

  const partition2VolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:partition2',
      'Partition 2', partition1Volume.devicePath);
  volumeManager.volumeInfoList.add(partition2VolumeInfo);
  const partition2VolumeMetadata = createFakeVolumeMetadata({
    volumeType: partition2VolumeInfo.volumeType,
    volumeId: partition2VolumeInfo.volumeId,
    devicePath: partition1Volume.devicePath,
    driveLabel: partition1Volume.driveLabel,
  });
  cacheEntries(currentState, addVolume({
                 volumeInfo: partition2VolumeInfo,
                 volumeMetadata: partition2VolumeMetadata,
               }));
  // cacheEntries() updates state in place.
  const newState = currentState;
  // Expect a fake parent entry list will be created.
  const parentEntryFileData: FileData =
      newState.allEntries[makeRemovableParentKey(partition1Volume)];
  const parentEntry = parentEntryFileData.entry as EntryList;
  assertEquals('USB_Drive', parentEntry.label);
  assertEquals(VolumeManagerCommon.RootType.REMOVABLE, parentEntry.rootType);
  assertTrue(parentEntryFileData.isEjectable);
  // Expect both partition1 and partition2 will be added as children.
  const partition2VolumeEntry: VolumeEntry =
      newState.allEntries[partition2VolumeInfo.displayRoot!.toURL()].entry;
  assertEquals(2, parentEntry.getUIChildren().length);
  assertEquals(partition1VolumeEntry, parentEntry.getUIChildren()[0]);
  assertEquals(partition2VolumeEntry, parentEntry.getUIChildren()[1]);
  assertEquals(
      constants.ICON_TYPES.UNKNOWN_REMOVABLE,
      newState.allEntries[partition1VolumeEntry.toURL()].icon);
  assertEquals(
      constants.ICON_TYPES.UNKNOWN_REMOVABLE,
      newState.allEntries[partition2VolumeEntry.toURL()].icon);
}

/** Tests that child entries can be added to the store correctly. */
export function testAddChildEntries() {
  const currentState = getEmptyState();

  // Add parent/children entries to the store.
  const fakeFs = new MockFileSystem('fake-fs');
  fakeFs.populate([
    '/aaa/',
    '/aaa/1/',
    '/aaa/2/',
    '/aaa/2/123/',
  ]);
  currentState.allEntries[fakeFs.entries['/aaa'].toURL()] = createFakeFileData({
    entry: fakeFs.entries['/aaa'],
    label: 'AAA',
    type: EntryType.FS_API,
  });
  currentState.allEntries[fakeFs.entries['/aaa/1'].toURL()] =
      createFakeFileData({
        entry: fakeFs.entries['/aaa/1'],
        label: 'AAA 1',
        type: EntryType.FS_API,
      });
  currentState.allEntries[fakeFs.entries['/aaa/2'].toURL()] =
      createFakeFileData({
        entry: fakeFs.entries['/aaa/2'],
        label: 'AAA 2',
        type: EntryType.FS_API,
        shouldDelayLoadingChildren: true,
      });
  currentState.allEntries[fakeFs.entries['/aaa/2/123'].toURL()] =
      createFakeFileData({
        entry: fakeFs.entries['/aaa/2/123'],
        label: 'AAA 123',
        type: EntryType.FS_API,
      });

  // Add child entries for /aaa/.
  const newState1 = addChildEntries(
      currentState, addChildEntriesAction({
        parentKey: fakeFs.entries['/aaa'].toURL(),
        entries: [fakeFs.entries['/aaa/1'], fakeFs.entries['/aaa/2']],
      }));
  // Expect the children filed is updated.
  const newChildren1 =
      newState1.allEntries[fakeFs.entries['/aaa'].toURL()].children;
  assertEquals(2, newChildren1.length);
  assertEquals(fakeFs.entries['/aaa/1'].toURL(), newChildren1[0]);
  assertEquals(fakeFs.entries['/aaa/2'].toURL(), newChildren1[1]);

  // Add child entries for /aaa/2 who has shouldDelayLoadingChildren.
  assertFalse(currentState.allEntries[fakeFs.entries['/aaa/2/123'].toURL()]
                  .shouldDelayLoadingChildren);
  const newState2 =
      addChildEntries(currentState, addChildEntriesAction({
                        parentKey: fakeFs.entries['/aaa/2'].toURL(),
                        entries: [fakeFs.entries['/aaa/2/123']],
                      }));
  // Expect child entry also has shouldDelayLoadingChildren=true.
  const newChildren2 =
      newState2.allEntries[fakeFs.entries['/aaa/2'].toURL()].children;
  assertEquals(1, newChildren2.length);
  assertEquals(fakeFs.entries['/aaa/2/123'].toURL(), newChildren2[0]);
  assertTrue(newState2.allEntries[fakeFs.entries['/aaa/2/123'].toURL()]
                 .shouldDelayLoadingChildren);

  // Add child entries for non-existed parent.
  const newState3 = addChildEntries(currentState, addChildEntriesAction({
                                      parentKey: 'non-exist-key',
                                      entries: [fakeFs.entries['/aaa/1']],
                                    }));
  // Expect nothing happens.
  assertEquals(currentState, newState3);
}
