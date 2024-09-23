// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import type {VolumeInfo} from '../../background/js/volume_info.js';
import {isInteractiveVolume, isSameEntry} from '../../common/js/entry_utils.js';
import {EntryList, FakeEntryImpl, VolumeEntry} from '../../common/js/files_app_entry_types.js';
import {isSinglePartitionFormatEnabled} from '../../common/js/flags.js';
import {installMockChrome, MockMetrics} from '../../common/js/mock_chrome.js';
import {waitUntil} from '../../common/js/test_error_reporting.js';
import {str} from '../../common/js/translations.js';
import {RootType, VolumeType} from '../../common/js/volume_manager_types.js';
import {ICON_TYPES} from '../../foreground/js/constants.js';
import type {FileData, State, Volume} from '../../state/state.js';
import {convertEntryToFileData} from '../ducks/all_entries.js';
import {createFakeVolumeMetadata, setUpFileManagerOnWindow, setupStore, waitDeepEquals} from '../for_tests.js';
import {getEmptyState, getEntry} from '../store.js';

import {addVolume, convertVolumeInfoAndMetadataToVolume, driveRootEntryListKey, removeVolume, updateIsInteractiveVolume} from './volumes.js';


export function setUp() {
  setUpFileManagerOnWindow();
  // Mock Chrome APIs.
  const mockChrome = {
    metricsPrivate: new MockMetrics(),
  };
  installMockChrome(mockChrome);
}

/** Generate MyFiles entry with fake entry list. */
function createMyFilesDataWithEntryList(): FileData {
  const myFilesEntryList = new EntryList('My files', RootType.MY_FILES);
  return convertEntryToFileData(myFilesEntryList);
}

/** Generate MyFiles entry with real volume entry. */
export function createMyFilesDataWithVolumeEntry():
    {fileData: FileData, volumeInfo: VolumeInfo} {
  const {volumeManager} = window.fileManager;
  const downloadsVolumeInfo =
      volumeManager.getCurrentProfileVolumeInfo(VolumeType.DOWNLOADS)!;
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
  const playFilesEntry =
      new FakeEntryImpl('Play files', RootType.ANDROID_FILES);
  initialState.allEntries[playFilesEntry.toURL()] =
      convertEntryToFileData(playFilesEntry);
  myFilesEntryList.addEntry(playFilesEntry);
  initialState.uiEntries.push(playFilesEntry.toURL());
  myFilesFileData.children.push(playFilesEntry.toURL());
  // Put Linux files volume entry in the store.
  const linuxFilesVolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeType.CROSTINI, 'linuxFilesId', 'Linux files');
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
  store.dispatch(addVolume(volumeInfo, volumeMetadata));

  // Expect the newly added volume is in the store.
  myFilesVolumeEntry.addEntry(playFilesEntry);
  myFilesVolumeEntry.addEntry(linuxFilesEntry);
  const want: Partial<State> = {
    allEntries: {
      [playFilesEntry.toURL()]: convertEntryToFileData(playFilesEntry),
      [linuxFilesEntry.toURL()]: convertEntryToFileData(linuxFilesEntry),
      [myFilesVolumeEntry.toURL()]: {
        ...fileData,
        canExpand: true,
        children: [playFilesEntry.toURL(), linuxFilesEntry.toURL()],
      },
    },
    volumes: {
      [linuxFilesVolumeInfo.volumeId]: {
        ...linuxFilesVolume,
        // Updated to MyFiles volume key.
        prefixKey: fileData.key,
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
  const playFilesUiEntry =
      new FakeEntryImpl('Play files', RootType.ANDROID_FILES);
  myFilesVolumeEntry.addEntry(playFilesUiEntry);
  fileData.children.push(playFilesUiEntry.toURL());
  initialState.uiEntries.push(playFilesUiEntry.toURL());
  initialState.allEntries[playFilesUiEntry.toURL()] =
      convertEntryToFileData(playFilesUiEntry);

  const store = setupStore(initialState);

  // Dispatch an action to add Play files volume.
  const {volumeManager} = window.fileManager;
  const playFilesVolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeType.ANDROID_FILES, 'playFilesId', playFilesUiEntry.label);
  volumeManager.volumeInfoList.add(playFilesVolumeInfo);
  const playFilesVolumeMetadata = createFakeVolumeMetadata(playFilesVolumeInfo);
  store.dispatch(addVolume(playFilesVolumeInfo, playFilesVolumeMetadata));

  // Expect the new play file volume will be nested inside MyFiles and the old
  // placeholder will be removed from MyFiles children but still in the store.
  const playFilesVolumeEntry = new VolumeEntry(playFilesVolumeInfo);
  myFilesVolumeEntry.addEntry(playFilesVolumeEntry);
  const want: Partial<State> = {
    allEntries: {
      [myFilesVolumeEntry.toURL()]: {
        ...fileData,
        canExpand: true,
        children: [playFilesVolumeEntry.toURL()],
      },
      [playFilesUiEntry.toURL()]: convertEntryToFileData(playFilesUiEntry),
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
    uiEntries: [playFilesUiEntry.toURL()],
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
  const driveVolumeInfo =
      volumeManager.getCurrentProfileVolumeInfo(VolumeType.DRIVE)!;
  const driveVolumeMetadata = createFakeVolumeMetadata(driveVolumeInfo);
  // DriveFS takes time to resolve.
  await driveVolumeInfo.resolveDisplayRoot();
  store.dispatch(addVolume(driveVolumeInfo, driveVolumeMetadata));

  // Expect all fake entries inside Drive will be added as its children.
  const myFilesFileData = createMyFilesDataWithEntryList();
  const driveFakeRootEntryList =
      new EntryList(str('DRIVE_DIRECTORY_LABEL'), RootType.DRIVE_FAKE_ROOT);
  const driveVolumeEntry = new VolumeEntry(driveVolumeInfo);
  const {sharedDriveDisplayRoot, computersDisplayRoot, fakeEntries} =
      driveVolumeInfo;
  const fakeSharedWithMeEntry = fakeEntries[RootType.DRIVE_SHARED_WITH_ME]!;
  const fakeOfflineEntry = fakeEntries[RootType.DRIVE_OFFLINE]!;
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
      [myFilesFileData.key]: myFilesFileData,
      // Fake Drive root entry list.
      [driveRootEntryListKey]: {
        ...convertEntryToFileData(driveFakeRootEntryList),
        canExpand: true,
        children: [
          driveVolumeEntry.toURL(),
          fakeSharedWithMeEntry.toURL(),
          fakeOfflineEntry.toURL(),
        ],
      },
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
      myFilesFileData.key,
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

/** Tests that single partition volume can be added correctly. */
async function addVolumeForSinglePartitionRemovable(done: () => void) {
  const initialState = getEmptyState();
  const store = setupStore(initialState);

  // Dispatch an action to add single partition volume.
  const volumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeType.REMOVABLE, 'removable:hoge', 'USB Drive', '/device/path/1');
  const volumeMetadata = createFakeVolumeMetadata(volumeInfo);
  store.dispatch(addVolume(volumeInfo, volumeMetadata));

  // Expect the volume is in the store.
  const myFilesFileData = createMyFilesDataWithEntryList();
  const volumeEntry = new VolumeEntry(volumeInfo);
  const parentEntry = new EntryList(
      volumeMetadata.driveLabel || '', RootType.REMOVABLE,
      volumeMetadata.devicePath);

  // There should be a parent wrapper if the flag is on.
  const hasParentWrapper = isSinglePartitionFormatEnabled();
  if (hasParentWrapper) {
    parentEntry.addEntry(volumeEntry);
  }

  const want: Partial<State> = {
    allEntries: {
      // Single partition volume.
      [volumeEntry.toURL()]: {
        ...convertEntryToFileData(volumeEntry),
        // When there is a parent wrapper, icon and ejectable values are
        // different.
        ...(hasParentWrapper ? {
          icon: ICON_TYPES.UNKNOWN_REMOVABLE,
          isEjectable: false,
        } :
                               {
                                 icon: ICON_TYPES.USB,
                                 isEjectable: true,
                               }),
      },
      // My Files entry list.
      [myFilesFileData.key]: myFilesFileData,
      // Parent wrapper entry.
      ...(hasParentWrapper ? {
        [parentEntry.toURL()]: {
          ...convertEntryToFileData(parentEntry),
          isEjectable: true,
          canExpand: true,
          children: [volumeEntry.toURL()],
        },
      } :
                             {}),
    },
    volumes: {
      [volumeInfo.volumeId]: {
        ...convertVolumeInfoAndMetadataToVolume(volumeInfo, volumeMetadata),
        ...(hasParentWrapper ? {prefixKey: parentEntry.toURL()} : {}),
      },
    },
  };
  await waitDeepEquals(store, want, (state) => ({
                                      allEntries: state.allEntries,
                                      volumes: state.volumes,
                                    }));

  done();
}

/** Run the above test with FilesSinglePartitionFormat flag off. */
export async function testAddVolumeForSinglePartitionRemovableWithFlagOff(
    done: () => void) {
  loadTimeData.overrideValues({FILES_SINGLE_PARTITION_FORMAT_ENABLED: false});
  addVolumeForSinglePartitionRemovable(done);
}

/** Run the above test with FilesSinglePartitionFormat flag on. */
export async function testAddVolumeForSinglePartitionRemovableWithFlagOn(
    done: () => void) {
  loadTimeData.overrideValues({FILES_SINGLE_PARTITION_FORMAT_ENABLED: true});
  addVolumeForSinglePartitionRemovable(done);
}

/** Tests that multiple partition volumes can be added correctly. */
async function addVolumeForMultipleUsbPartitionsGrouping(done: () => void) {
  const store = setupStore();
  // Dispatch an action to add partition-1 volume.
  const {volumeManager} = window.fileManager;
  const partition1VolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeType.REMOVABLE, 'removable:partition1', 'Partition 1',
      '/device/path/1');
  volumeManager.volumeInfoList.add(partition1VolumeInfo);
  const partition1VolumeMetadata =
      createFakeVolumeMetadata(partition1VolumeInfo);
  partition1VolumeMetadata.driveLabel = 'USB_Drive';
  store.dispatch(addVolume(partition1VolumeInfo, partition1VolumeMetadata));

  // Dispatch an action to add partition-2 volume.
  const partition2VolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeType.REMOVABLE, 'removable:partition2', 'Partition 2',
      partition1VolumeInfo.devicePath);
  const partition2VolumeMetadata =
      createFakeVolumeMetadata(partition2VolumeInfo);
  partition2VolumeMetadata.driveLabel = partition1VolumeMetadata.driveLabel;
  store.dispatch(addVolume(partition2VolumeInfo, partition2VolumeMetadata));

  // Dispatch an action to add partition-3 volume.
  const partition3VolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeType.REMOVABLE, 'removable:partition3', 'Partition 3',
      partition1VolumeInfo.devicePath);
  const partition3VolumeMetadata =
      createFakeVolumeMetadata(partition3VolumeInfo);
  partition3VolumeMetadata.driveLabel = partition1VolumeMetadata.driveLabel;
  store.dispatch(addVolume(partition3VolumeInfo, partition3VolumeMetadata));

  // Expect all 3 partition volumes are in the store and there will be a wrapper
  // entry created to group all 3 partitions.
  const myFilesFileData = createMyFilesDataWithEntryList();
  const partition1VolumeEntry = new VolumeEntry(partition1VolumeInfo);
  const partition2VolumeEntry = new VolumeEntry(partition2VolumeInfo);
  const partition3VolumeEntry = new VolumeEntry(partition3VolumeInfo);
  const parentEntry = new EntryList(
      partition1VolumeMetadata.driveLabel, RootType.REMOVABLE,
      partition1VolumeInfo.devicePath);
  parentEntry.addEntry(partition1VolumeEntry);
  parentEntry.addEntry(partition2VolumeEntry);
  parentEntry.addEntry(partition3VolumeEntry);
  const want: Partial<State> = {
    allEntries: {
      // Partition-1 volume.
      [partition1VolumeEntry.toURL()]: {
        ...convertEntryToFileData(partition1VolumeEntry),
        icon: ICON_TYPES.UNKNOWN_REMOVABLE,
        isEjectable: false,
      },
      // Partition-2 volume.
      [partition2VolumeEntry.toURL()]: {
        ...convertEntryToFileData(partition2VolumeEntry),
        icon: ICON_TYPES.UNKNOWN_REMOVABLE,
        isEjectable: false,
      },
      // Partition-3 volume.
      [partition3VolumeEntry.toURL()]: {
        ...convertEntryToFileData(partition3VolumeEntry),
        icon: ICON_TYPES.UNKNOWN_REMOVABLE,
        isEjectable: false,
      },
      // My Files entry list.
      [myFilesFileData.key]: myFilesFileData,
      // Parent wrapper entry.
      [parentEntry.toURL()]: {
        ...convertEntryToFileData(parentEntry),
        isEjectable: true,
        canExpand: true,
        children: [
          partition1VolumeEntry.toURL(),
          partition2VolumeEntry.toURL(),
          partition3VolumeEntry.toURL(),
        ],
      },
    },
    volumes: {
      [partition1VolumeInfo.volumeId]: {
        ...convertVolumeInfoAndMetadataToVolume(
            partition1VolumeInfo, partition1VolumeMetadata),
        prefixKey: parentEntry.toURL(),
      },
      [partition2VolumeInfo.volumeId]: {
        ...convertVolumeInfoAndMetadataToVolume(
            partition2VolumeInfo, partition2VolumeMetadata),
        prefixKey: parentEntry.toURL(),
      },
      [partition3VolumeInfo.volumeId]: {
        ...convertVolumeInfoAndMetadataToVolume(
            partition3VolumeInfo, partition3VolumeMetadata),
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

/** Run the above test with FilesSinglePartitionFormat flag off. */
export async function testAddVolumeForMultipleUsbPartitionsGroupingWithFlagOff(
    done: () => void) {
  loadTimeData.overrideValues({FILES_SINGLE_PARTITION_FORMAT_ENABLED: false});
  addVolumeForMultipleUsbPartitionsGrouping(done);
}


/** Run the above test with FilesSinglePartitionFormat flag on. */
export async function testAddVolumeForMultipleUsbPartitionsGroupingWithFlagOn(
    done: () => void) {
  loadTimeData.overrideValues({FILES_SINGLE_PARTITION_FORMAT_ENABLED: true});
  addVolumeForMultipleUsbPartitionsGrouping(done);
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
      VolumeType.CROSTINI, 'crostini', 'Linux files');
  volumeManager.volumeInfoList.add(volumeInfo);
  const volumeMetadata = createFakeVolumeMetadata(volumeInfo);
  // Disable crostini volume type.
  volumeManager.isDisabled = (volumeType) => {
    return volumeType === VolumeType.CROSTINI;
  };
  store.dispatch(addVolume(volumeInfo, volumeMetadata));

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
  const driveVolumeInfo =
      volumeManager.getCurrentProfileVolumeInfo(VolumeType.DRIVE)!;
  // DriveFS takes time to resolve.
  await driveVolumeInfo.resolveDisplayRoot();
  const driveVolumeMetadata = createFakeVolumeMetadata(driveVolumeInfo);
  // Disable Drive volume type.
  volumeManager.isDisabled = (volumeType) => {
    return volumeType === VolumeType.DRIVE;
  };
  store.dispatch(addVolume(driveVolumeInfo, driveVolumeMetadata));

  // Expect the volume entry is being disabled.
  await waitUntil(() => {
    const state = store.getState();
    const driveFakeRootEntryList =
        getEntry(state, driveRootEntryListKey) as EntryList;
    const driveVolumeEntry =
        getEntry(store.getState(), driveVolumeInfo.displayRoot.toURL()) as
        VolumeEntry;
    const driveVolume = state.volumes[driveVolumeInfo.volumeId]!;
    return driveFakeRootEntryList && driveFakeRootEntryList.disabled === true &&
        driveVolumeEntry && driveVolumeEntry.disabled === true && driveVolume &&
        driveVolume.isDisabled === true;
  });

  done();
}

/** Tests that archive volume can be added correctly. */
export async function testAddArchiveVolume(done: () => void) {
  const initialState = getEmptyState();
  const store = setupStore(initialState);

  const {volumeManager} = window.fileManager;
  const volumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeType.ARCHIVE, 'test', 'test.zip');
  volumeManager.volumeInfoList.add(volumeInfo);
  const volumeEntry = new VolumeEntry(volumeInfo);
  const volumeMetadata = createFakeVolumeMetadata(volumeInfo);

  // Dispatch an action to add the archive volume.
  store.dispatch(addVolume(volumeInfo, volumeMetadata));

  // Expect the volume will be added from the store.
  const myFilesFileData = createMyFilesDataWithEntryList();
  const want: Partial<State> = {
    allEntries: {
      // My Files entry list.
      [myFilesFileData.key]: myFilesFileData,
      // Archive.
      [volumeEntry.toURL()]: {
        ...convertEntryToFileData(volumeEntry),
        isEjectable: true,
      },
    },
    volumes: {
      [volumeInfo.volumeId]: {
        ...convertVolumeInfoAndMetadataToVolume(volumeInfo, volumeMetadata),
      },
    },
  };
  await waitDeepEquals(store, want, (state) => ({
                                      allEntries: state.allEntries,
                                      volumes: state.volumes,
                                    }));

  done();
}

/** Tests that volume can be removed correctly. */
export async function testRemoveVolume(done: () => void) {
  const initialState = getEmptyState();
  const {volumeManager} = window.fileManager;
  const volumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeType.ARCHIVE, 'test', 'test.zip');
  volumeManager.volumeInfoList.add(volumeInfo);
  const volume = convertVolumeInfoAndMetadataToVolume(
      volumeInfo, createFakeVolumeMetadata(volumeInfo));
  initialState.volumes[volume.volumeId] = volume;

  const store = setupStore(initialState);

  // Dispatch an action to remove the volume.
  store.dispatch(removeVolume(volume.volumeId));

  // Expect the volume will be removed from the store.
  await waitDeepEquals(store, {}, (state) => state.volumes);

  done();
}

/**
 * Tests removing volume from MyFiles will also update the MyFiles entry.
 */
export async function testRemoveVolumeFromMyFiles(done: () => void) {
  const initialState = getEmptyState();
  // Put MyFiles in the store.
  const {fileData, volumeInfo} = createMyFilesDataWithVolumeEntry();
  const myFilesVolumeEntry = fileData.entry as VolumeEntry;
  const myFilesVolume = convertVolumeInfoAndMetadataToVolume(
      volumeInfo, createFakeVolumeMetadata(volumeInfo));
  initialState.allEntries[myFilesVolumeEntry.toURL()] = fileData;
  initialState.volumes[volumeInfo.volumeId] = myFilesVolume;
  // Put Crostini in the store.
  const {volumeManager} = window.fileManager;
  const crostiniVolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeType.CROSTINI, 'crostiniId', 'Linux files');
  volumeManager.volumeInfoList.add(crostiniVolumeInfo);
  const crostiniVolume = convertVolumeInfoAndMetadataToVolume(
      crostiniVolumeInfo, createFakeVolumeMetadata(crostiniVolumeInfo));
  const crostiniVolumeEntry = new VolumeEntry(crostiniVolumeInfo);
  const crostiniFileData = convertEntryToFileData(crostiniVolumeEntry);
  initialState.allEntries[crostiniVolumeEntry.toURL()] = crostiniFileData;
  crostiniVolume.prefixKey = myFilesVolumeEntry.toURL();
  initialState.volumes[crostiniVolume.volumeId] = crostiniVolume;
  fileData.children.push(crostiniVolumeEntry.toURL());
  myFilesVolumeEntry.addEntry(crostiniVolumeEntry);
  // Put Linux files placeholder in the store.
  const linuxFilesUiEntry =
      new FakeEntryImpl(crostiniVolume.label, RootType.CROSTINI);
  initialState.uiEntries.push(linuxFilesUiEntry.toURL());
  initialState.allEntries[linuxFilesUiEntry.toURL()] =
      convertEntryToFileData(linuxFilesUiEntry);

  const store = setupStore(initialState);

  // Dispatch an action to remove volume.
  store.dispatch(removeVolume(crostiniVolume.volumeId));

  // Expect the volume entry has been removed from MyFiles and the UI entry has
  // been added back.
  const want: Partial<State> = {
    allEntries: {
      [myFilesVolumeEntry.toURL()]: {
        ...convertEntryToFileData(myFilesVolumeEntry),
        canExpand: true,
        children: [linuxFilesUiEntry.toURL()],
      },
      [linuxFilesUiEntry.toURL()]: convertEntryToFileData(linuxFilesUiEntry),
    },
    volumes: {
      [myFilesVolume.volumeId]: myFilesVolume,
    },
  };
  await waitDeepEquals(store, want, (state) => ({
                                      allEntries: state.allEntries,
                                      volumes: state.volumes,
                                    }));

  // Check the volume entry has also been removed from MyFiles entry.
  const uiChildren = myFilesVolumeEntry.getUiChildren();
  assertEquals(1, uiChildren.length);
  assertTrue(isSameEntry(linuxFilesUiEntry, uiChildren[0]!));

  done();
}

/**
 * Tests removing grouped removable volume will also update the parent UI entry.
 */
export async function testRemoveGroupedRemovableVolume(done: () => void) {
  const initialState = getEmptyState();
  // Put partition-1 volume in the store.
  const {volumeManager} = window.fileManager;
  const partition1VolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeType.REMOVABLE, 'removable:partition1', 'Partition 1',
      '/device/path');
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

  // Put partition-2 volume in the store.
  const partition2VolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeType.REMOVABLE, 'removable:partition2', 'Partition 2',
      '/device/path');
  volumeManager.volumeInfoList.add(partition2VolumeInfo);
  const partition2VolumeEntry = new VolumeEntry(partition2VolumeInfo);
  const partition2FileData = convertEntryToFileData(partition2VolumeEntry);
  const partition2VolumeMetadata =
      createFakeVolumeMetadata(partition2VolumeInfo);
  partition2VolumeMetadata.driveLabel = 'USB_Drive';
  const partition2Volume = convertVolumeInfoAndMetadataToVolume(
      partition2VolumeInfo, partition2VolumeMetadata);
  initialState.volumes[partition2Volume.volumeId] = partition2Volume;
  initialState.allEntries[partition2VolumeEntry.toURL()] = partition2FileData;

  // Put parent wrapper in the store.
  const parentEntry = new EntryList(
      partition1VolumeMetadata.driveLabel || '', RootType.REMOVABLE,
      partition1VolumeMetadata.devicePath);
  parentEntry.addEntry(partition1VolumeEntry);
  initialState.volumes[partition1Volume.volumeId]!.prefixKey =
      parentEntry.toURL();
  parentEntry.addEntry(partition2VolumeEntry);
  initialState.volumes[partition2Volume.volumeId]!.prefixKey =
      parentEntry.toURL();
  const parentFileData = convertEntryToFileData(parentEntry);
  parentFileData.children =
      [partition1VolumeEntry.toURL(), partition2VolumeEntry.toURL()];
  initialState.allEntries[parentEntry.toURL()] = parentFileData;
  initialState.uiEntries.push(parentEntry.toURL());

  const store = setupStore(initialState);

  // Dispatch an action to remove partition1.
  store.dispatch(removeVolume(partition1Volume.volumeId));

  // Expect the partition1 entry has been removed from its parent entry.
  const wantAfterRemovingPartition1: Partial<State> = {
    allEntries: {
      // parent entry.
      [parentEntry.toURL()]: {
        ...parentFileData,
        canExpand: true,
        children: [partition2VolumeEntry.toURL()],
      },
      // partition2 entry.
      [partition2VolumeEntry.toURL()]: {
        ...partition2FileData,
        icon: ICON_TYPES.UNKNOWN_REMOVABLE,
      },
    },
    volumes: {
      [partition2Volume.volumeId]: partition2Volume,
    },
    uiEntries: [parentEntry.toURL()],
  };
  await waitDeepEquals(
      store, wantAfterRemovingPartition1, (state) => ({
                                            allEntries: state.allEntries,
                                            uiEntries: state.uiEntries,
                                            volumes: state.volumes,
                                          }));

  // Check the partition1 entry has also been removed from parent entry.
  const uiChildrenAfterRemovingPartition1 = parentEntry.getUiChildren();
  assertEquals(1, uiChildrenAfterRemovingPartition1.length);
  assertTrue(isSameEntry(
      partition2VolumeEntry, uiChildrenAfterRemovingPartition1[0]!));

  // Dispatch an action to remove partition2.
  store.dispatch(removeVolume(partition2Volume.volumeId));

  // Expect the partition2 entry has been removed from its parent entry, but it
  // still exists in the allEntries, waiting for the schedule cleaning.
  const wantAfterRemovingPartition2: Partial<State> = {
    allEntries: {
      // parent entry.
      [parentEntry.toURL()]: {
        ...parentFileData,
        canExpand: true,
        children: [
          partition2VolumeEntry.toURL(),
        ],
      },
      // partition2 entry.
      [partition2VolumeEntry.toURL()]: {
        ...partition2FileData,
        icon: ICON_TYPES.UNKNOWN_REMOVABLE,
      },
    },
    volumes: {},
    uiEntries: [],
  };
  await waitDeepEquals(
      store, wantAfterRemovingPartition2, (state) => ({
                                            allEntries: state.allEntries,
                                            uiEntries: state.uiEntries,
                                            volumes: state.volumes,
                                          }));

  // Check parent entry now has no children.
  const uiChildrenAfterRemovingPartition2 = parentEntry.getUiChildren();
  assertEquals(0, uiChildrenAfterRemovingPartition2.length);

  done();
}

export async function testUpdateIsInteractiveVolume(done: () => void) {
  const initialState = getEmptyState();
  const {volumeManager} = window.fileManager;
  const volumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeType.ARCHIVE, 'test', 'test.zip');
  volumeManager.volumeInfoList.add(volumeInfo);
  const volume = convertVolumeInfoAndMetadataToVolume(
      volumeInfo, createFakeVolumeMetadata(volumeInfo));
  initialState.volumes[volume.volumeId] = volume;

  const store = setupStore(initialState);

  // Expect that the ODFS volume is interactive.
  assertTrue(isInteractiveVolume(volumeInfo));

  // Dispatch an action to set |isInteractive| for the volume to false.
  store.dispatch(updateIsInteractiveVolume({
    volumeId: volumeInfo.volumeId,
    isInteractive: false,
  }));

  // Expect that the volume is set to non-interactive.
  const wantUpdatedVol = {
    volumes: {
      [volumeInfo.volumeId]: {
        ...volume,
        isInteractive: false,
      },
    },
  };
  await waitDeepEquals(store, wantUpdatedVol, (state) => ({
                                                volumes: state.volumes,
                                              }));

  done();
}
