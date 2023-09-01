// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

import {EntryList, VolumeEntry} from '../../common/js/files_app_entry_types.js';
import {util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {PropStatus, State, Volume, VolumeId} from '../../externs/ts/state.js';
import {VolumeInfo} from '../../externs/volume_info.js';
import {Slice} from '../../lib/base_store.js';
import {cacheEntries, getMyFiles, updateFileData, volumeNestingEntries} from '../ducks/all_entries.js';
import {getEntry} from '../store.js';

import {updateDeviceConnectionState} from './device.js';

/**
 * @fileoverview Volumes slice of the store.
 * @suppress {checkTypes}
 */

const slice = new Slice<State>('volumes');
export {slice as volumesSlice};

const VolumeType = VolumeManagerCommon.VolumeType;
export const myFilesEntryListKey =
    `entry-list://${VolumeManagerCommon.RootType.MY_FILES}`;
export const crostiniPlaceHolderKey =
    `fake-entry://${VolumeManagerCommon.RootType.CROSTINI}`;
export const drivePlaceHolderKey =
    `fake-entry://${VolumeManagerCommon.RootType.DRIVE_FAKE_ROOT}`;
export const recentRootKey =
    `fake-entry://${VolumeManagerCommon.RootType.RECENT}/all`;
export const trashRootKey =
    `fake-entry://${VolumeManagerCommon.RootType.TRASH}`;
export const driveRootEntryListKey =
    `entry-list://${VolumeManagerCommon.RootType.DRIVE_FAKE_ROOT}`;
export const makeRemovableParentKey =
    (volume: Volume|chrome.fileManagerPrivate.VolumeMetadata) =>
        `entry-list://${VolumeManagerCommon.RootType.REMOVABLE}/${
            volume.devicePath}`;
export const removableGroupKey =
    (volume: Volume|chrome.fileManagerPrivate.VolumeMetadata) =>
        `${volume.devicePath}/${volume.driveLabel}`;

export function getVolumeTypesNestedInMyFiles() {
  const myFilesNestedVolumeTypes = new Set<VolumeManagerCommon.VolumeType>([
    VolumeType.ANDROID_FILES,
    VolumeType.CROSTINI,
  ]);
  if (util.isGuestOsEnabled()) {
    myFilesNestedVolumeTypes.add(VolumeType.GUEST_OS);
  }
  return myFilesNestedVolumeTypes;
}

/**
 * Convert VolumeInfo and VolumeMetadata to its store representation: Volume.
 */
export function convertVolumeInfoAndMetadataToVolume(
    volumeInfo: VolumeInfo,
    volumeMetadata: chrome.fileManagerPrivate.VolumeMetadata): Volume {
  /**
   * FileKey for the volume root's Entry. Or how do we find the Entry for this
   * volume in the allEntries.
   */
  const volumeRootKey = volumeInfo.displayRoot.toURL();
  return {
    volumeId: volumeMetadata.volumeId,
    volumeType: volumeMetadata.volumeType,
    rootKey: volumeRootKey,
    status: PropStatus.SUCCESS,
    label: volumeInfo.label,
    error: volumeMetadata.mountCondition,
    deviceType: volumeMetadata.deviceType,
    devicePath: volumeMetadata.devicePath,
    isReadOnly: volumeMetadata.isReadOnly,
    isReadOnlyRemovableDevice: volumeMetadata.isReadOnlyRemovableDevice,
    providerId: volumeMetadata.providerId,
    configurable: volumeMetadata.configurable,
    watchable: volumeMetadata.watchable,
    source: volumeMetadata.source,
    diskFileSystemType: volumeMetadata.diskFileSystemType,
    iconSet: volumeMetadata.iconSet,
    driveLabel: volumeMetadata.driveLabel,
    vmType: volumeMetadata.vmType,

    isDisabled: false,
    // FileKey to volume's parent in the Tree.
    prefixKey: undefined,
    // A volume is by default interactive unless explicitly made
    // non-interactive.
    isInteractive: true,
  };
}

/**
 * Updates a volume from the store.
 */
export function updateVolume(
    state: State, volumeId: VolumeId, changes: Partial<Volume>): Volume|
    undefined {
  if (!state.volumes[volumeId]) {
    console.warn(`Volume not found in the store: ${volumeId}`);
    return;
  }
  return {
    ...state.volumes[volumeId],
    ...changes,
  };
}

/** Create action to add a volume. */
export const addVolume = slice.addReducer('add', addVolumeReducer);

function addVolumeReducer(currentState: State, payload: {
  volumeMetadata: chrome.fileManagerPrivate.VolumeMetadata,
  volumeInfo: VolumeInfo,
}): State {
  // Cache entries, so the reducers can use any entry from `allEntries`.
  cacheEntries(currentState, [new VolumeEntry(payload.volumeInfo)]);
  volumeNestingEntries(
      currentState, payload.volumeInfo, payload.volumeMetadata);
  const volumeMetadata = payload.volumeMetadata;
  const volumeInfo = payload.volumeInfo;
  if (!volumeInfo.fileSystem) {
    console.error(
        'Only add to the store volumes that have successfully resolved.');
    return currentState;
  }

  const volumes = {
    ...currentState.volumes,
  };
  const volume =
      convertVolumeInfoAndMetadataToVolume(volumeInfo, volumeMetadata);
  const volumeEntry = getEntry(currentState, volume.rootKey!);
  // Use volume entry's disabled property because that one is derived from
  // volume manager.
  if (volumeEntry) {
    volume.isDisabled = !!(volumeEntry as VolumeEntry).disabled;
  }

  // Nested in MyFiles.
  const myFilesNestedVolumeTypes = getVolumeTypesNestedInMyFiles();

  // When mounting MyFiles replace the temporary placeholder in nested volumes.
  if (volume.volumeType === VolumeType.DOWNLOADS) {
    for (const v of Object.values<Volume>(volumes)) {
      if (myFilesNestedVolumeTypes.has(v.volumeType)) {
        v.prefixKey = volume.rootKey;
      }
    }
  }

  // When mounting a nested volume, set the prefixKey.
  if (myFilesNestedVolumeTypes.has(volume.volumeType)) {
    const {myFilesEntry} = getMyFiles(currentState);
    volume.prefixKey = myFilesEntry.toURL();
  }

  // When mounting Drive.
  if (volume.volumeType === VolumeType.DRIVE) {
    const drive = getEntry(currentState, driveRootEntryListKey) as EntryList;
    assert(drive);
    volume.prefixKey = drive!.toURL();
  }

  // When mounting Removable.
  if (volume.volumeType === VolumeType.REMOVABLE) {
    // Should it it be nested or not?
    const groupingKey = removableGroupKey(volume);
    const parentKey = makeRemovableParentKey(volume);
    const groupParentEntry = getEntry(currentState, parentKey);
    if (groupParentEntry) {
      const volumesInSameGroup = Object.values<Volume>(volumes).filter(v => {
        if (v.volumeType === VolumeType.REMOVABLE &&
            removableGroupKey(v) === groupingKey) {
          v.prefixKey = parentKey;
          return true;
        }

        return false;
      });
      volume.prefixKey =
          volumesInSameGroup.length > 0 ? groupParentEntry?.toURL() : undefined;
    }
  }

  return {
    ...currentState,
    volumes: {
      ...volumes,
      [volume.volumeId]: volume,
    },
  };
}

/** Create action to remove a volume. */
export const removeVolume = slice.addReducer('remove', removeVolumeReducer);

function removeVolumeReducer(currentState: State, payload: {
  volumeId: VolumeId,
}): State {
  delete currentState.volumes[payload.volumeId];
  const volumes = {
    ...currentState.volumes,
  };

  return {
    ...currentState,
    volumes,
  };
}

/** Create action to update isInteractive for a volume. */
export const updateIsInteractiveVolume =
    slice.addReducer('set-is-interactive', updateIsInteractiveVolumeReducer);

function updateIsInteractiveVolumeReducer(currentState: State, payload: {
  volumeId: VolumeId,
  isInteractive: boolean,
}): State {
  const volumes = {
    ...currentState.volumes,
  };

  const updatedVolume = {
    ...volumes[payload.volumeId],
    isInteractive: payload.isInteractive,
  };

  return {
    ...currentState,
    volumes: {
      ...volumes,
      [payload.volumeId]: updatedVolume,
    },
  };
}

slice.addReducer(
    updateDeviceConnectionState.type, updateDeviceConnectionStateReducer);

function updateDeviceConnectionStateReducer(
    currentState: State,
    payload: typeof updateDeviceConnectionState.PAYLOAD): State {
  let volumes: State['volumes']|undefined;

  // Find ODFS volume(s) and disable it (or them) if offline.
  const disableODFS = payload.connection ===
      chrome.fileManagerPrivate.DeviceConnectionState.OFFLINE;
  for (const volume of Object.values<Volume>(currentState.volumes)) {
    if (!util.isOneDriveId(volume.providerId) ||
        volume.isDisabled === disableODFS) {
      continue;
    }
    const updatedVolume =
        updateVolume(currentState, volume.volumeId, {isDisabled: disableODFS});
    if (updatedVolume) {
      if (!volumes) {
        volumes = {
          ...currentState.volumes,
          [volume.volumeId]: updatedVolume,
        };
      } else {
        volumes[volume.volumeId] = updatedVolume;
      }
    }
    // Make the ODFS FileData/VolumeEntry consistent with its volume in the
    // store.
    updateFileData(currentState, volume.rootKey!, {disabled: disableODFS});
    const odfsVolumeEntry =
        getEntry(currentState, volume.rootKey!) as VolumeEntry;
    if (odfsVolumeEntry) {
      odfsVolumeEntry.disabled = disableODFS;
    }
  }

  return volumes ? {...currentState, volumes} : currentState;
}
