// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import {EntryList, VolumeEntry} from '../../common/js/files_app_entry_types.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {EntryType, FileData, PropStatus} from '../../externs/ts/state.js';
import {VolumeInfo} from '../../externs/volume_info.js';
import {addVolume as addVolumeAction, removeVolume as removeVolumeAction} from '../actions/volumes.js';
import {createFakeFileData, createFakeVolume, createFakeVolumeMetadata} from '../for_tests.js';
import {getEmptyState} from '../store.js';

import {addVolume, driveRootEntryListKey, removeVolume} from './volumes.js';

/** Generate MyFiles entry with real volume entry. */
function createMyFilesDataWithVolumeEntry():
    {fileData: FileData, volumeInfo: VolumeInfo} {
  const volumeManager = new MockVolumeManager();
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

/** Tests that MyFiles volume can be added correctly. */
export function testAddMyFilesVolume() {
  const currentState = getEmptyState();
  const {fileData, volumeInfo} = createMyFilesDataWithVolumeEntry();
  const myFilesVolume = createFakeVolume({
    volumeType: volumeInfo.volumeType,
    volumeId: volumeInfo.volumeId,
    label: volumeInfo.label,
    rootKey: volumeInfo.displayRoot.toURL(),
  });
  currentState.allEntries[fileData.entry.toURL()] = fileData;
  currentState.volumes[volumeInfo.volumeId] = myFilesVolume;
  // Mark its volume entry as disabled.
  (fileData.entry as VolumeEntry).disabled = true;
  // Put MyFiles and its sub volumes in the store.
  const playFilesVolume = createFakeVolume({
    volumeId: 'playFilesId',
    volumeType: VolumeManagerCommon.VolumeType.ANDROID_FILES,
    label: 'Play files',
    rootKey: 'filesystem:chrome://android-files-url',
  });
  currentState.volumes[playFilesVolume.volumeId] = playFilesVolume;
  const crostiniFilesVolume = createFakeVolume({
    volumeId: 'volumeInMyFiles2',
    volumeType: VolumeManagerCommon.VolumeType.CROSTINI,
    label: 'Linux files',
    rootKey: 'filesystem:chrome://crostini-files-url',
  });
  currentState.volumes[crostiniFilesVolume.volumeId] = crostiniFilesVolume;

  const volumeMetadata = createFakeVolumeMetadata(
      {volumeType: volumeInfo.volumeType, volumeId: volumeInfo.volumeId});
  const newState = addVolume(currentState, addVolumeAction({
                               volumeInfo,
                               volumeMetadata,
                             }));
  const volume = newState.volumes[volumeInfo.volumeId];
  // Check all volume fields are set correctly.
  assertEquals(volume.volumeId, volumeMetadata.volumeId);
  assertEquals(volume.volumeType, volumeMetadata.volumeType);
  assertEquals(volume.rootKey, volumeInfo.displayRoot.toURL());
  assertEquals(volume.status, PropStatus.SUCCESS);
  assertEquals(volume.label, volumeInfo.label);
  assertEquals(volume.error, volumeMetadata.mountCondition);
  assertEquals(volume.deviceType, volumeMetadata.deviceType);
  assertEquals(volume.devicePath, volumeMetadata.devicePath);
  assertEquals(volume.isReadOnly, volumeMetadata.isReadOnly);
  assertEquals(
      volume.isReadOnlyRemovableDevice,
      volumeMetadata.isReadOnlyRemovableDevice);
  assertEquals(volume.providerId, volumeMetadata.providerId);
  assertEquals(volume.configurable, volumeMetadata.configurable);
  assertEquals(volume.watchable, volumeMetadata.watchable);
  assertEquals(volume.source, volumeMetadata.source);
  assertEquals(volume.diskFileSystemType, volumeMetadata.diskFileSystemType);
  assertEquals(volume.iconSet, volumeMetadata.iconSet);
  assertEquals(volume.driveLabel, volumeMetadata.driveLabel);
  assertEquals(volume.vmType, volumeMetadata.vmType);
  // Because its volume entry is disabled.
  assertEquals(volume.isDisabled, true);
  assertEquals(volume.prefixKey, undefined);
  // Check all child volumes has prefix key setup.
  assertEquals(
      fileData.entry.toURL(),
      newState.volumes[playFilesVolume.volumeId].prefixKey);
  assertEquals(
      fileData.entry.toURL(),
      newState.volumes[crostiniFilesVolume.volumeId].prefixKey);
}

/** Tests that volume nested in MyFiles can be added correctly. */
export function testAddNestedMyFilesVolume() {
  const currentState = getEmptyState();
  // Put MyFiles in the store.
  const {fileData, volumeInfo} = createMyFilesDataWithVolumeEntry();
  const myFilesVolume = createFakeVolume({
    volumeType: volumeInfo.volumeType,
    volumeId: volumeInfo.volumeId,
    label: volumeInfo.label,
    rootKey: volumeInfo.displayRoot.toURL(),
  });
  currentState.allEntries[fileData.entry.toURL()] = fileData;
  currentState.volumes[volumeInfo.volumeId] = myFilesVolume;

  const playFilesVolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.ANDROID_FILES, 'playFilesId',
      'Play files');
  const playFilesVolumeMetadata = createFakeVolumeMetadata({
    volumeType: playFilesVolumeInfo.volumeType,
    volumeId: playFilesVolumeInfo.volumeId,
  });
  const newState = addVolume(currentState, addVolumeAction({
                               volumeInfo: playFilesVolumeInfo,
                               volumeMetadata: playFilesVolumeMetadata,
                             }));
  // Check the newly added volume has prefix key setup.
  assertEquals(
      fileData.entry.toURL(),
      newState.volumes[playFilesVolumeInfo.volumeId].prefixKey);
}

/** Tests that drive volume can be added correctly. */
export function testAddDriveVolume(done: () => void) {
  const currentState = getEmptyState();
  // Put FakeDriveRoot in the store.
  const fakeDriveRootEntry = new EntryList(
      'Google Drive', VolumeManagerCommon.RootType.DRIVE_FAKE_ROOT);
  currentState.allEntries[driveRootEntryListKey] = createFakeFileData({
    entry: fakeDriveRootEntry,
    label: 'Google Drive',
    type: EntryType.ENTRY_LIST,
  });

  const volumeManager = new MockVolumeManager();
  const driveVolumeInfo = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DRIVE)!;
  const driveVolumeMetadata = createFakeVolumeMetadata({
    volumeType: driveVolumeInfo.volumeType,
    volumeId: driveVolumeInfo.volumeId,
  });
  // DriveFS takes time to resolve.
  driveVolumeInfo.resolveDisplayRoot(() => {
    const newState = addVolume(currentState, addVolumeAction({
                                 volumeInfo: driveVolumeInfo,
                                 volumeMetadata: driveVolumeMetadata,
                               }));
    // Check the newly added volume has prefix key setup.
    assertEquals(
        fakeDriveRootEntry.toURL(),
        newState.volumes[driveVolumeInfo.volumeId].prefixKey);

    done();
  });
}

/** Tests that multiple partition volumes can be added correctly. */
export function testAddVolumeForMultipleUsbPartitionsGrouping() {
  const currentState = getEmptyState();
  // Add USB/partition-1 into the store.
  const devicePath = 'device/path/1';
  const partition1 = createFakeVolume({
    volumeId: 'removable:partition1',
    volumeType: VolumeManagerCommon.VolumeType.REMOVABLE,
    rootKey: 'partition1-url',
    label: 'Partition 1',
    devicePath,
    driveLabel: 'USB_Drive',
  });
  currentState.volumes[partition1.volumeId] = partition1;
  // Add its parent entry to the store.
  const parentEntry = new EntryList(
      partition1.driveLabel!, VolumeManagerCommon.RootType.REMOVABLE,
      partition1.devicePath);
  currentState.allEntries[parentEntry.toURL()] = createFakeFileData({
    entry: parentEntry,
    label: partition1.driveLabel!,
    type: EntryType.ENTRY_LIST,
  });

  const partition2VolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'removable:partition2',
      'Partition 2', devicePath);
  const partition2VolumeMetadata = createFakeVolumeMetadata({
    volumeType: partition2VolumeInfo.volumeType,
    volumeId: partition2VolumeInfo.volumeId,
    devicePath: partition1.devicePath,
    driveLabel: partition1.driveLabel,
  });
  const newState = addVolume(currentState, addVolumeAction({
                               volumeInfo: partition2VolumeInfo,
                               volumeMetadata: partition2VolumeMetadata,
                             }));
  // Check the newly added volume and all existing volumes belonging to the same
  // group have prefix key setup.
  assertEquals(
      parentEntry.toURL(), newState.volumes[partition1.volumeId].prefixKey);
  assertEquals(
      parentEntry.toURL(),
      newState.volumes[partition2VolumeInfo.volumeId].prefixKey);
}

/** Tests that volume can be removed correctly. */
export function testRemoveVolume() {
  const currentState = getEmptyState();
  const volume = createFakeVolume({
    volumeId: 'test',
    volumeType: VolumeManagerCommon.VolumeType.ARCHIVE,
    label: 'test.zip',
    rootKey: 'test-root',
  });
  currentState.volumes[volume.volumeId] = volume;
  const newState = removeVolume(
      currentState, removeVolumeAction({volumeId: volume.volumeId}));
  assertEquals(undefined, newState.volumes[volume.volumeId]);
}
