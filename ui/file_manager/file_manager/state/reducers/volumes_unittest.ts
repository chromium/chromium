// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import {EntryList, FakeEntryImpl, VolumeEntry} from '../../common/js/files_app_entry_types.js';
import {waitUntil} from '../../common/js/test_error_reporting.js';
import {str} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {FileData, State, Volume} from '../../externs/ts/state.js';
import {VolumeInfo} from '../../externs/volume_info.js';
import {constants} from '../../foreground/js/constants.js';
import {addVolume, removeVolume} from '../actions/volumes.js';
import {createFakeVolumeMetadata, setUpFileManagerOnWindow, setupStore, waitDeepEquals} from '../for_tests.js';
import {getEmptyState, getEntry} from '../store.js';

import {convertEntryToFileData} from './all_entries.js';
import {convertVolumeInfoAndMetadataToVolume, driveRootEntryListKey} from './volumes.js';

export function setUp() {
  setUpFileManagerOnWindow();
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
  const {volumeManager} = window.fileManager;
  const downloadsVolumeInfo = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DOWNLOADS)!;
  const fileData = convertEntryToFileData(new VolumeEntry(downloadsVolumeInfo));
  return {fileData, volumeInfo: downloadsVolumeInfo};
}

/** Tests that MyFiles volume can be added correctly. */
export async function testAddMyFilesVolume(done: () => void) {
  const initialState = getEmptyState();
  // Put MyFiles entry list in the store.
  const myFilesFileData = createMyFilesDataWithEntryList();
  const myFilesEntryList = myFilesFileData.entry as EntryList;
  initialState.allEntries[myFilesEntryList.toURL()] = myFilesFileData;
  initialState.uiEntries.push(myFilesEntryList.toURL());
  // Put Play files placeholder UI entry in the store.
  const playFilesEntry = new FakeEntryImpl(
      'Play files', VolumeManagerCommon.RootType.ANDROID_FILES);
  initialState.allEntries[playFilesEntry.toURL()] =
      convertEntryToFileData(playFilesEntry);
  myFilesEntryList.addEntry(playFilesEntry);
  initialState.uiEntries.push(playFilesEntry.toURL());
  myFilesFileData.children.push(playFilesEntry.toURL());
  // Put Linux files volume entry in the store.
  const linuxFilesVolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.CROSTINI, 'linuxFilesId', 'Linux files');
  const linuxFilesEntry = new VolumeEntry(linuxFilesVolumeInfo);
  const {volumeManager} = window.fileManager;
  volumeManager.volumeInfoList.add(linuxFilesVolumeInfo);
  initialState.allEntries[linuxFilesEntry.toURL()] =
      convertEntryToFileData(linuxFilesEntry);
  const linuxFilesVolume = convertVolumeInfoAndMetadataToVolume(
      linuxFilesVolumeInfo, createFakeVolumeMetadata(linuxFilesVolumeInfo));
  initialState.volumes[linuxFilesVolume.volumeId] = linuxFilesVolume;
  myFilesFileData.children.push(linuxFilesEntry.toURL());
  myFilesEntryList.addEntry(linuxFilesEntry);

  const store = setupStore(initialState);

  // Dispatch an action to add MyFiles volume.
  const {fileData, volumeInfo} = createMyFilesDataWithVolumeEntry();
  const myFilesVolumeEntry = fileData.entry as VolumeEntry;
  const volumeMetadata = createFakeVolumeMetadata(volumeInfo);
  store.dispatch(addVolume({
    volumeInfo,
    volumeMetadata,
  }));

  // Expect the newly added volume is in the store.
  myFilesVolumeEntry.addEntry(playFilesEntry);
  myFilesVolumeEntry.addEntry(linuxFilesEntry);
  const want: Partial<State> = {
    allEntries: {
      [playFilesEntry.toURL()]: convertEntryToFileData(playFilesEntry),
      [linuxFilesEntry.toURL()]: convertEntryToFileData(linuxFilesEntry),
      [myFilesVolumeEntry.toURL()]: fileData,
    },
    volumes: {
      [linuxFilesVolumeInfo.volumeId]: {
        ...linuxFilesVolume,
        // Updated to MyFiles volume key.
        prefixKey: fileData.entry.toURL(),
      },
      [volumeInfo.volumeId]:
          convertVolumeInfoAndMetadataToVolume(volumeInfo, volumeMetadata),
    },
    uiEntries: [playFilesEntry.toURL()],
  };
  await waitDeepEquals(store, want, (state) => ({
                                      allEntries: state.allEntries,
                                      volumes: state.volumes,
                                      uiEntries: state.uiEntries,
                                    }));

  done();
}

/** Tests that volume nested in MyFiles can be added correctly. */
export async function testAddNestedMyFilesVolume(done: () => void) {
  const initialState = getEmptyState();
  // Put MyFiles in the store.
  const {fileData, volumeInfo} = createMyFilesDataWithVolumeEntry();
  const myFilesVolumeEntry = fileData.entry as VolumeEntry;
  const myFilesVolume = convertVolumeInfoAndMetadataToVolume(
      volumeInfo, createFakeVolumeMetadata(volumeInfo));
  initialState.allEntries[myFilesVolumeEntry.toURL()] = fileData;
  initialState.volumes[volumeInfo.volumeId] = myFilesVolume;
  // Put Play files placeholder UI entry in the store.
  const playFilesUiEntry = new FakeEntryImpl(
      'Play files', VolumeManagerCommon.RootType.ANDROID_FILES);
  myFilesVolumeEntry.addEntry(playFilesUiEntry);
  fileData.children.push(playFilesUiEntry.toURL());
  initialState.uiEntries.push(playFilesUiEntry.toURL());
  initialState.allEntries[playFilesUiEntry.toURL()] =
      convertEntryToFileData(playFilesUiEntry);

  const store = setupStore(initialState);

  // Dispatch an action to add Play files volume.
  const {volumeManager} = window.fileManager;
  const playFilesVolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.ANDROID_FILES, 'playFilesId',
      playFilesUiEntry.label);
  volumeManager.volumeInfoList.add(playFilesVolumeInfo);
  const playFilesVolumeMetadata = createFakeVolumeMetadata(playFilesVolumeInfo);
  store.dispatch(addVolume({
    volumeInfo: playFilesVolumeInfo,
    volumeMetadata: playFilesVolumeMetadata,
  }));

  // Expect the new play file volume will be nested inside MyFiles and the old
  // placeholder will be removed.
  const playFilesVolumeEntry = new VolumeEntry(playFilesVolumeInfo);
  myFilesVolumeEntry.addEntry(playFilesVolumeEntry);
  const want: Partial<State> = {
    allEntries: {
      [myFilesVolumeEntry.toURL()]: {
        ...fileData,
        children: [playFilesVolumeEntry.toURL()],
      },
      [playFilesVolumeEntry.toURL()]:
          convertEntryToFileData(playFilesVolumeEntry),
    },
    volumes: {
      [myFilesVolume.volumeId]: myFilesVolume,
      [playFilesVolumeInfo.volumeId]: {
        ...convertVolumeInfoAndMetadataToVolume(
            playFilesVolumeInfo, playFilesVolumeMetadata),
        prefixKey: myFilesVolumeEntry.toURL(),
      },
    },
    uiEntries: [],
  };
  await waitDeepEquals(store, want, (state) => ({
                                      allEntries: state.allEntries,
                                      volumes: state.volumes,
                                      uiEntries: state.uiEntries,
                                    }));

  done();
}

/** Tests that drive volume can be added correctly. */
export async function testAddDriveVolume(done: () => void) {
  const initialState = getEmptyState();
  const store = setupStore(initialState);

  // Dispatch an action to add Drive volume.
  const {volumeManager} = window.fileManager;
  const driveVolumeInfo = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DRIVE)!;
  const driveVolumeMetadata = createFakeVolumeMetadata(driveVolumeInfo);
  // DriveFS takes time to resolve.
  await driveVolumeInfo.resolveDisplayRoot();
  store.dispatch(addVolume({
    volumeInfo: driveVolumeInfo,
    volumeMetadata: driveVolumeMetadata,
  }));

  // Expect all fake entries inside Drive will be added as its children.
  const myFilesFileData = createMyFilesDataWithEntryList();
  const driveFakeRootEntryList = new EntryList(
      str('DRIVE_DIRECTORY_LABEL'),
      VolumeManagerCommon.RootType.DRIVE_FAKE_ROOT);
  const driveVolumeEntry = new VolumeEntry(driveVolumeInfo);
  const {sharedDriveDisplayRoot, computersDisplayRoot, fakeEntries} =
      driveVolumeInfo;
  const fakeSharedWithMeEntry =
      fakeEntries[VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME];
  const fakeOfflineEntry =
      fakeEntries[VolumeManagerCommon.RootType.DRIVE_OFFLINE];
  driveFakeRootEntryList.addEntry(driveVolumeEntry);
  driveFakeRootEntryList.addEntry(sharedDriveDisplayRoot);
  driveFakeRootEntryList.addEntry(computersDisplayRoot);
  driveFakeRootEntryList.addEntry(fakeSharedWithMeEntry);
  driveFakeRootEntryList.addEntry(fakeOfflineEntry);
  const want: Partial<State> = {
    allEntries: {
      // My Drive.
      [driveVolumeEntry.toURL()]: convertEntryToFileData(driveVolumeEntry),
      // My Files entry list.
      [myFilesFileData.entry.toURL()]: myFilesFileData,
      // Fake Drive root entry list.
      [driveRootEntryListKey]: convertEntryToFileData(driveFakeRootEntryList),
      // Shared with me.
      [fakeSharedWithMeEntry.toURL()]:
          convertEntryToFileData(fakeSharedWithMeEntry),
      // Offline.
      [fakeOfflineEntry.toURL()]: convertEntryToFileData(fakeOfflineEntry),
      // Shared drives and Computers won't be here because they will be cleared.
    },
    volumes: {
      [driveVolumeInfo.volumeId]: {
        ...convertVolumeInfoAndMetadataToVolume(
            driveVolumeInfo, driveVolumeMetadata),
        prefixKey: driveRootEntryListKey,
      },
    },
    uiEntries: [
      myFilesFileData.entry.toURL(),
      driveRootEntryListKey,
      fakeSharedWithMeEntry.toURL(),
      fakeOfflineEntry.toURL(),
    ],
  };
  await waitDeepEquals(store, want, (state) => ({
                                      allEntries: state.allEntries,
                                      volumes: state.volumes,
                                      uiEntries: state.uiEntries,
                                    }));

  done();
}

/** Tests that multiple partition volumes can be added correctly. */
export async function testAddVolumeForMultipleUsbPartitionsGrouping(
    done: () => void) {
  const initialState = getEmptyState();
  // Put partition-1 volume in the store.
  const {volumeManager} = window.fileManager;
  const partition1VolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:partition1',
      'Partition 1', '/device/path/1');
  volumeManager.volumeInfoList.add(partition1VolumeInfo);
  const partition1VolumeEntry = new VolumeEntry(partition1VolumeInfo);
  const partition1FileData = convertEntryToFileData(partition1VolumeEntry);
  const partition1VolumeMetadata =
      createFakeVolumeMetadata(partition1VolumeInfo);
  partition1VolumeMetadata.driveLabel = 'USB_Drive';
  const partition1Volume = convertVolumeInfoAndMetadataToVolume(
      partition1VolumeInfo, partition1VolumeMetadata);
  initialState.volumes[partition1Volume.volumeId] = partition1Volume;
  initialState.allEntries[partition1VolumeEntry.toURL()] = partition1FileData;

  const store = setupStore(initialState);

  // Dispatch an action to add partition-2 volume.
  const partition2VolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:partition2',
      'Partition 2', partition1VolumeInfo.devicePath);
  const partition2VolumeMetadata =
      createFakeVolumeMetadata(partition2VolumeInfo);
  partition2VolumeMetadata.driveLabel = partition1VolumeMetadata.driveLabel;
  store.dispatch(addVolume({
    volumeInfo: partition2VolumeInfo,
    volumeMetadata: partition2VolumeMetadata,
  }));

  // Expect the partition-2 volume is in the store and there will be a wrapper
  // entry created to group both partition-1 and partition-2.
  const myFilesFileData = createMyFilesDataWithEntryList();
  const partition2VolumeEntry = new VolumeEntry(partition2VolumeInfo);
  const parentEntry = new EntryList(
      partition1VolumeMetadata.driveLabel,
      VolumeManagerCommon.RootType.REMOVABLE, partition1VolumeInfo.devicePath);
  parentEntry.addEntry(partition1VolumeEntry);
  parentEntry.addEntry(partition2VolumeEntry);
  const want: Partial<State> = {
    allEntries: {
      // Partition-1 volume.
      [partition1VolumeEntry.toURL()]: {
        ...partition1FileData,
        icon: constants.ICON_TYPES.UNKNOWN_REMOVABLE,
        isEjectable: false,
      },
      // Partition-2 volume.
      [partition2VolumeEntry.toURL()]: {
        ...convertEntryToFileData(partition2VolumeEntry),
        icon: constants.ICON_TYPES.UNKNOWN_REMOVABLE,
        isEjectable: false,
      },
      // My Files entry list.
      [myFilesFileData.entry.toURL()]: myFilesFileData,
      // Parent wrapper entry.
      [parentEntry.toURL()]: {
        ...convertEntryToFileData(parentEntry),
        isEjectable: true,
      },
    },
    volumes: {
      [partition1VolumeInfo.volumeId]: {
        ...partition1Volume,
        prefixKey: parentEntry.toURL(),
      },
      [partition2VolumeInfo.volumeId]: {
        ...convertVolumeInfoAndMetadataToVolume(
            partition2VolumeInfo, partition2VolumeMetadata),
        prefixKey: parentEntry.toURL(),
      },
    },
  };
  await waitDeepEquals(store, want, (state) => ({
                                      allEntries: state.allEntries,
                                      volumes: state.volumes,
                                    }));

  done();
}

/**
 * Tests that volume will be disabled if it is disabled in the volume manager.
 */
export async function testAddDisabledVolume(done: () => void) {
  const initialState = getEmptyState();
  const store = setupStore(initialState);

  // Dispatch an action to add crostini volume.
  const {volumeManager} = window.fileManager;
  const volumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.CROSTINI, 'crostini', 'Linux files');
  volumeManager.volumeInfoList.add(volumeInfo);
  const volumeMetadata = createFakeVolumeMetadata(volumeInfo);
  // Disable crostini volume type.
  volumeManager.isDisabled = (volumeType) => {
    return volumeType === VolumeManagerCommon.VolumeType.CROSTINI;
  };
  store.dispatch(addVolume({volumeInfo, volumeMetadata}));

  // Expect the volume entry is being disabled.
  await waitUntil(() => {
    const state = store.getState();
    const volumeEntry =
        getEntry(state, volumeInfo.displayRoot.toURL()) as VolumeEntry;
    const volume = state.volumes[volumeInfo.volumeId] as Volume;
    return volumeEntry && volumeEntry.disabled === true && volume &&
        volume.isDisabled === true;
  });

  done();
}

/**
 * Tests that drive fake root entry list will be disabled if the drive volume is
 * disabled in the volume manager.
 */
export async function testAddDisabledDriveVolume(done: () => void) {
  const initialState = getEmptyState();
  const store = setupStore(initialState);

  // Dispatch an action to add drive volume.
  const {volumeManager} = window.fileManager;
  const driveVolumeInfo = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DRIVE)!;
  // DriveFS takes time to resolve.
  await driveVolumeInfo.resolveDisplayRoot();
  const driveVolumeMetadata = createFakeVolumeMetadata(driveVolumeInfo);
  // Disable Drive volume type.
  volumeManager.isDisabled = (volumeType) => {
    return volumeType === VolumeManagerCommon.VolumeType.DRIVE;
  };
  store.dispatch(addVolume(
      {volumeInfo: driveVolumeInfo, volumeMetadata: driveVolumeMetadata}));

  // Expect the volume entry is being disabled.
  await waitUntil(() => {
    const state = store.getState();
    const driveFakeRootEntryList =
        getEntry(state, driveRootEntryListKey) as EntryList;
    const driveVolumeEntry =
        getEntry(store.getState(), driveVolumeInfo.displayRoot.toURL()) as
        VolumeEntry;
    const driveVolume = state.volumes[driveVolumeInfo.volumeId];
    return driveFakeRootEntryList && driveFakeRootEntryList.disabled === true &&
        driveVolumeEntry && driveVolumeEntry.disabled === true && driveVolume &&
        driveVolume.isDisabled === true;
  });

  done();
}

/** Tests that volume can be removed correctly. */
export async function testRemoveVolume(done: () => void) {
  const initialState = getEmptyState();
  const {volumeManager} = window.fileManager;
  const volumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.ARCHIVE, 'test', 'test.zip');
  volumeManager.volumeInfoList.add(volumeInfo);
  const volume = convertVolumeInfoAndMetadataToVolume(
      volumeInfo, createFakeVolumeMetadata(volumeInfo));
  initialState.volumes[volume.volumeId] = volume;

  const store = setupStore(initialState);

  // Dispatch an action to remove the volume.
  store.dispatch(removeVolume({volumeId: volume.volumeId}));

  // Expect the volume will be removed from the store.
  await waitDeepEquals(store, {}, (state) => state.volumes);

  done();
}
