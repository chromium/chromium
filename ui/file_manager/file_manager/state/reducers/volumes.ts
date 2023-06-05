// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

import {EntryList, VolumeEntry} from '../../common/js/files_app_entry_types.js';
import {util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {PropStatus, State, Volume} from '../../externs/ts/state.js';
import {VolumeInfo} from '../../externs/volume_info.js';
import {AddVolumeAction, RemoveVolumeAction, UpdateIsInteractiveVolumeAction} from '../actions/volumes.js';
import {getEntry} from '../store.js';

import {getMyFiles} from './all_entries.js';

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

export function addVolume(currentState: State, action: AddVolumeAction): State {
  const volumeMetadata = action.payload.volumeMetadata;
  const volumeInfo = action.payload.volumeInfo;
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

export function removeVolume(
    currentState: State, action: RemoveVolumeAction): State {
  delete currentState.volumes[action.payload.volumeId];
  const volumes = {
    ...currentState.volumes,
  };

  return {
    ...currentState,
    volumes,
  };
}

export function updateIsInteractiveVolume(
    currentState: State, action: UpdateIsInteractiveVolumeAction): State {
  const volumes = {
    ...currentState.volumes,
  };

  const updatedVolume = {
    ...volumes[action.payload.volumeId],
    isInteractive: action.payload.isInteractive,
  };

  return {
    ...currentState,
    volumes: {
      ...volumes,
      [action.payload.volumeId]: updatedVolume,
    },
  };
}
