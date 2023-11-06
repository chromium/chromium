// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isOneDriveId, isSameEntry, isVolumeEntry, sortEntries} from '../../common/js/entry_utils.js';
import {EntryList, VolumeEntry} from '../../common/js/files_app_entry_types.js';
import {isGuestOsEnabled, isSinglePartitionFormatEnabled} from '../../common/js/flags.js';
import {str} from '../../common/js/translations.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {FakeEntry, FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';
import {PropStatus, State, Volume, VolumeId} from '../../externs/ts/state.js';
import type {VolumeInfo} from '../../externs/volume_info.js';
import {constants} from '../../foreground/js/constants.js';
import {Slice} from '../../lib/base_store.js';
import {getEntry, getFileData} from '../store.js';

import {cacheEntries, getMyFiles, updateFileData} from './all_entries.js';
import {updateDeviceConnectionState} from './device.js';

/**
 * @fileoverview Volumes slice of the store.
 * @suppress {checkTypes}
 */

const slice = new Slice<State, State['volumes']>('volumes');
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
    (volume: Volume|chrome.fileManagerPrivate.VolumeMetadata) => {
      // Should be consistent with EntryList's toURL() method.
      if (volume.devicePath) {
        return `entry-list://${VolumeManagerCommon.RootType.REMOVABLE}/${
            volume.devicePath}`;
      }
      return `entry-list://${VolumeManagerCommon.RootType.REMOVABLE}`;
    };
export const removableGroupKey =
    (volume: Volume|chrome.fileManagerPrivate.VolumeMetadata) =>
        `${volume.devicePath}/${volume.driveLabel}`;

export function getVolumeTypesNestedInMyFiles() {
  const myFilesNestedVolumeTypes = new Set<VolumeManagerCommon.VolumeType>([
    VolumeType.ANDROID_FILES,
    VolumeType.CROSTINI,
  ]);
  if (isGuestOsEnabled()) {
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
  const {volumeMetadata, volumeInfo} = payload;
  if (!volumeInfo.fileSystem) {
    console.error(
        'Only add to the store volumes that have successfully resolved.');
    return currentState;
  }

  // Cache entries, so the reducers can use any entry from `allEntries`.
  const newVolumeEntry = new VolumeEntry(payload.volumeInfo);
  cacheEntries(currentState, [newVolumeEntry]);
  const volumeRootKey = newVolumeEntry.toURL();

  // Update isEjectable/shouldDelayLoadingChildren fields in the FileData.
  currentState.allEntries[volumeRootKey] = {
    ...currentState.allEntries[volumeRootKey],
    isEjectable:
        (volumeInfo.source === VolumeManagerCommon.Source.DEVICE &&
         volumeInfo.volumeType !== VolumeManagerCommon.VolumeType.MTP) ||
        volumeInfo.source === VolumeManagerCommon.Source.FILE,
    shouldDelayLoadingChildren:
        volumeInfo.source === VolumeManagerCommon.Source.NETWORK &&
        (volumeInfo.volumeType === VolumeManagerCommon.VolumeType.PROVIDED ||
         volumeInfo.volumeType === VolumeManagerCommon.VolumeType.SMB),
  };

  const volume =
      convertVolumeInfoAndMetadataToVolume(volumeInfo, volumeMetadata);
  // Use volume entry's disabled property because that one is derived from
  // volume manager.
  volume.isDisabled = !!newVolumeEntry.disabled;

  // Handles volumes nested inside MyFiles.
  // It creates a placeholder for MyFiles if MyFiles volume isn't mounted yet.
  const myFilesNestedVolumeTypes = getVolumeTypesNestedInMyFiles();
  const {myFilesEntry} = getMyFiles(currentState);
  // For volumes which are supposed to be nested inside MyFiles (e.g. Android,
  // Crostini, GuestOS), we need to nest them into MyFiles and remove the
  // placeholder fake entry if existed.
  if (myFilesNestedVolumeTypes.has(volume.volumeType)) {
    volume.prefixKey = myFilesEntry.toURL();

    const myFilesEntryKey = myFilesEntry.toURL();
    // Shallow copy here because we will update this object directly below, and
    // the same object might be referenced in the UI.
    const myFilesFileData = {...getFileData(currentState, myFilesEntryKey)!};
    // Nest the entry for the new volume info in MyFiles.
    const uiEntryPlaceholder = myFilesEntry.getUIChildren().find(
        childEntry => childEntry.name === newVolumeEntry.name);
    // Remove a placeholder for the currently mounting volume.
    if (uiEntryPlaceholder) {
      myFilesEntry.removeChildEntry(uiEntryPlaceholder);
      // Also remove it from the children field.
      myFilesFileData.children = myFilesFileData.children.filter(
          childKey => childKey !== uiEntryPlaceholder.toURL());
      // Do not remove the placeholder ui entry from the store. Removing it from
      // the MyFiles is sufficient to prevent it from showing in the directory
      // tree. We keep it in the store (`currentState["uiEntries"]`) because
      // when the corresponding volume unmounts, we need to use its existence to
      // decide if we need to re-add the placeholder back to MyFiles.
    }
    appendChildIfNotExisted(myFilesEntry, newVolumeEntry);
    // Push the new entry to the children of FileData and sort them.
    if (!myFilesFileData.children.find(
            childKey => childKey === volumeRootKey)) {
      const newChildren = [...myFilesFileData.children, volumeRootKey];
      const childEntries =
          newChildren.map(childKey => getEntry(currentState, childKey)!);
      myFilesFileData.children =
          sortEntries(myFilesEntry, childEntries).map(entry => entry.toURL());
    }
    currentState.allEntries[myFilesEntryKey] = myFilesFileData;
  }

  // Handles MyFiles volume.
  // It nests the Android, Crostini & GuestOSes inside MyFiles.
  if (volume.volumeType === VolumeType.DOWNLOADS) {
    for (const v of Object.values<Volume>(currentState.volumes)) {
      if (myFilesNestedVolumeTypes.has(v.volumeType)) {
        v.prefixKey = volumeRootKey;
      }
    }

    // Do not use myFilesEntry above, because at this moment both fake MyFiles
    // and real MyFiles are in the store.
    const myFilesEntryList =
        getEntry(currentState, myFilesEntryListKey) as EntryList;
    if (myFilesEntryList) {
      // We need to copy the children of the entry list to the real volume
      // entry.
      const uiChildren = [...myFilesEntryList.getUIChildren()];
      for (const childEntry of uiChildren) {
        appendChildIfNotExisted(newVolumeEntry, childEntry);
        myFilesEntryList.removeChildEntry(childEntry);
      }
      // Remove MyFiles entry list from the uiEntries.
      currentState.uiEntries = currentState.uiEntries.filter(
          uiEntryKey => uiEntryKey !== myFilesEntryListKey);
    }
  }

  // Handles Drive volume.
  // It nests the Drive root (aka MyDrive) inside a EntryList for "Google
  // Drive", and also the fake entries for "Offline" and "Shared with me".
  if (volume.volumeType === VolumeType.DRIVE) {
    let googleDrive: EntryList|null =
        getEntry(currentState, driveRootEntryListKey) as EntryList;
    const entriesToCache: Array<Entry|FilesAppEntry> = [];
    if (!googleDrive) {
      googleDrive = new EntryList(
          str('DRIVE_DIRECTORY_LABEL'),
          VolumeManagerCommon.RootType.DRIVE_FAKE_ROOT);
      entriesToCache.push(googleDrive);
      currentState.uiEntries = [...currentState.uiEntries, googleDrive.toURL()];
    }
    appendChildIfNotExisted(googleDrive, newVolumeEntry);

    // We want the order to be
    // - My Drive
    // - Shared Drives (if the user has any)
    // - Computers (if the user has any)
    // - Shared with me
    // - Offline
    const {sharedDriveDisplayRoot, computersDisplayRoot, fakeEntries} =
        volumeInfo;
    // Add "Shared drives" (team drives) grand root into Drive. It's guaranteed
    // to be resolved at this moment because ADD_VOLUME action will only be
    // triggered after resolving all roots.
    if (sharedDriveDisplayRoot) {
      entriesToCache.push(sharedDriveDisplayRoot);
      appendChildIfNotExisted(googleDrive, sharedDriveDisplayRoot);
    }

    // Add "Computer" grand root into Drive. It's guaranteed to be resolved at
    // this moment because ADD_VOLUME action will only be triggered after
    // resolving all roots.
    if (computersDisplayRoot) {
      entriesToCache.push(computersDisplayRoot);
      appendChildIfNotExisted(googleDrive, computersDisplayRoot);
    }

    // Add "Shared with me" into Drive.
    const fakeSharedWithMe =
        fakeEntries[VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME];
    if (fakeSharedWithMe) {
      entriesToCache.push(fakeSharedWithMe);
      currentState.uiEntries =
          [...currentState.uiEntries, fakeSharedWithMe.toURL()];
      appendChildIfNotExisted(googleDrive, fakeSharedWithMe);
    }

    // Add "Offline" into Drive.
    const fakeOffline = fakeEntries[VolumeManagerCommon.RootType.DRIVE_OFFLINE];
    if (fakeOffline) {
      entriesToCache.push(fakeOffline);
      currentState.uiEntries = [...currentState.uiEntries, fakeOffline.toURL()];
      appendChildIfNotExisted(googleDrive, fakeOffline);
    }

    cacheEntries(currentState, entriesToCache);
    volume.prefixKey = googleDrive.toURL();
  }

  // Handles Removable volume.
  // It may nest in a EntryList if one device has multiple partitions.
  if (volume.volumeType === VolumeType.REMOVABLE) {
    const groupingKey = removableGroupKey(volumeMetadata);
    // When the flag is on, we always group removable volume even there's only 1
    // partition, otherwise the group only happens when there are more than 1
    // partition in the same device.
    const shouldGroup = isSinglePartitionFormatEnabled() ?
        true :
        Object.values<Volume>(currentState.volumes).some(v => {
          return (
              v.volumeType === VolumeType.REMOVABLE &&
              removableGroupKey(v) === groupingKey &&
              v.volumeId != volumeInfo.volumeId);
        });

    if (shouldGroup) {
      const parentKey = makeRemovableParentKey(volumeMetadata);
      let parentEntry = getEntry(currentState, parentKey) as EntryList | null;
      if (!parentEntry) {
        parentEntry = new EntryList(
            volumeMetadata.driveLabel || '',
            VolumeManagerCommon.RootType.REMOVABLE, volumeMetadata.devicePath);
        cacheEntries(currentState, [parentEntry]);
        currentState.uiEntries =
            [...currentState.uiEntries, parentEntry.toURL()];
        // Removable devices with group, its parent should always be ejectable.
        currentState.allEntries[parentKey].isEjectable = true;
      }
      // Update the siblings too.
      Object.values<Volume>(currentState.volumes)
          .filter(
              // volume with `prefixKey` has already been processed.
              v => !v.prefixKey && v.volumeType === VolumeType.REMOVABLE &&
                  removableGroupKey(v) === groupingKey,
              )
          .forEach(v => {
            v.prefixKey = parentEntry!.toURL();
            const fileData = getFileData(currentState, v.rootKey!);
            if (fileData?.entry) {
              appendChildIfNotExisted(parentEntry!, fileData.entry);
              // For sub-partition from a removable volume, its children icon
              // should be UNKNOWN_REMOVABLE, and it shouldn't be ejectable.
              currentState.allEntries[v.rootKey!] = {
                ...fileData,
                icon: constants.ICON_TYPES.UNKNOWN_REMOVABLE,
                isEjectable: false,
              };
            }
          });
      // At this point the current `newVolumeEntry` is not in `parentEntry`, we
      // need to add that to that group.
      appendChildIfNotExisted(parentEntry, newVolumeEntry);
      volume.prefixKey = parentEntry.toURL();
      // For sub-partition from a removable volume, its children icon should be
      // UNKNOWN_REMOVABLE, and it shouldn't be ejectable.
      const fileData = getFileData(currentState, volumeRootKey);
      currentState.allEntries[volumeRootKey] = {
        ...fileData,
        icon: constants.ICON_TYPES.UNKNOWN_REMOVABLE,
        isEjectable: false,
      };
    }
  }

  return {
    ...currentState,
    volumes: {
      ...currentState.volumes,
      [volume.volumeId]: volume,
    },
  };
}

function appendChildIfNotExisted(
    parentEntry: VolumeEntry|EntryList,
    childEntry: Entry|FilesAppEntry): boolean {
  if (!parentEntry.getUIChildren().find(
          (entry) => isSameEntry(entry, childEntry))) {
    parentEntry.addEntry(childEntry);
    return true;
  }

  return false;
}

/** Create action to remove a volume. */
export const removeVolume = slice.addReducer('remove', removeVolumeReducer);

function removeVolumeReducer(currentState: State, payload: {
  volumeId: VolumeId,
}): State {
  const volumeToRemove: Volume|undefined =
      currentState.volumes[payload.volumeId];
  if (!volumeToRemove) {
    // Somehow the volume is already removed from the store, do nothing.
    return currentState;
  }
  const volumeEntry = getEntry(currentState, volumeToRemove.rootKey!)!;
  delete currentState.volumes[payload.volumeId];
  currentState.volumes = {
    ...currentState.volumes,
  };

  // We also need to check if the removed volume is a child of My files and if
  // the volume is a grouped removable device.
  const volumeTypesNestedInMyFiles = getVolumeTypesNestedInMyFiles();
  const isGroupedRemovable =
      volumeToRemove.volumeType === VolumeManagerCommon.VolumeType.REMOVABLE &&
      volumeToRemove.prefixKey;
  if (volumeTypesNestedInMyFiles.has(volumeToRemove.volumeType)) {
    const {myFilesEntry} = getMyFiles(currentState);
    const children = myFilesEntry.getUIChildren();
    const volumeEntryExistsInMyFiles = !!children.find(
        childEntry =>
            isVolumeEntry(childEntry) && isSameEntry(childEntry, volumeEntry));
    if (volumeEntryExistsInMyFiles) {
      // Remove it from the MyFiles UI children.
      myFilesEntry.removeChildEntry(volumeEntry);
      // Re-add the corresponding placeholder ui entry to the UI children.
      const uiEntryKey = currentState.uiEntries.find(entryKey => {
        const uiEntry = getEntry(currentState, entryKey)! as FakeEntry;
        return uiEntry.name === volumeEntry.name;
      });
      if (uiEntryKey) {
        const uiEntry = getEntry(currentState, uiEntryKey)!;
        myFilesEntry.addEntry(uiEntry);
      }
      // Remove it from the MyFiles file data.
      const fileData = getFileData(currentState, myFilesEntry.toURL());
      if (fileData) {
        let newChildren =
            fileData.children.filter(child => child !== volumeEntry.toURL());
        // Re-add the corresponding placeholder ui entry to the file data.
        if (uiEntryKey) {
          newChildren = newChildren.concat(uiEntryKey);
          const childEntries =
              newChildren.map(childKey => getEntry(currentState, childKey)!);
          newChildren = sortEntries(myFilesEntry, childEntries)
                            .map(entry => entry.toURL());
        }
        currentState.allEntries[myFilesEntry.toURL()] = {
          ...fileData,
          children: newChildren,
        };
      }
    }
  } else if (isGroupedRemovable) {
    const fileData = getFileData(currentState, volumeToRemove.prefixKey!);
    if (fileData) {
      // Remove it from the parent UI entry's UI children.
      (fileData.entry as EntryList).removeChildEntry(volumeEntry);
      // Remove it from the parent UI entry's file data.
      const newChildren =
          fileData.children.filter(child => child !== volumeEntry.toURL());
      currentState.allEntries[volumeToRemove.prefixKey!] = {
        ...fileData,
        children: newChildren,
      };
      // If this is the last child, remove the parent UI entry.
      if (newChildren.length === 0) {
        currentState.uiEntries = currentState.uiEntries.filter(
            uiEntryKey => uiEntryKey !== volumeToRemove.prefixKey!);
      }
    }
  }

  return {
    ...currentState,
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
    if (!isOneDriveId(volume.providerId) || volume.isDisabled === disableODFS) {
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
