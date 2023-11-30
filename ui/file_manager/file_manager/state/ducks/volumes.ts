// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isOneDriveId, isSameEntry, sortEntries} from '../../common/js/entry_utils.js';
import {EntryList, VolumeEntry} from '../../common/js/files_app_entry_types.js';
import {isGuestOsEnabled, isSinglePartitionFormatEnabled} from '../../common/js/flags.js';
import {str} from '../../common/js/translations.js';
import {RootType, Source, VolumeType} from '../../common/js/volume_manager_types.js';
import {FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';
import {FileKey, PropStatus, State, Volume, VolumeId} from '../../externs/ts/state.js';
import type {VolumeInfo} from '../../externs/volume_info.js';
import {constants} from '../../foreground/js/constants.js';
import {Slice} from '../../lib/base_store.js';
import {getEntry, getFileData} from '../store.js';

import {cacheEntries, getMyFiles, updateFileData} from './all_entries.js';
import {updateDeviceConnectionState} from './device.js';

/**
 * @fileoverview Volumes slice of the store.
 */

const slice = new Slice<State, State['volumes']>('volumes');
export {slice as volumesSlice};

export const myFilesEntryListKey = `entry-list://${RootType.MY_FILES}`;
export const crostiniPlaceHolderKey = `fake-entry://${RootType.CROSTINI}`;
export const drivePlaceHolderKey = `fake-entry://${RootType.DRIVE_FAKE_ROOT}`;
export const recentRootKey = `fake-entry://${RootType.RECENT}/all`;
export const trashRootKey = `fake-entry://${RootType.TRASH}`;
export const driveRootEntryListKey = `entry-list://${RootType.DRIVE_FAKE_ROOT}`;
export const makeRemovableParentKey =
    (volume: Volume|chrome.fileManagerPrivate.VolumeMetadata) => {
      // Should be consistent with EntryList's toURL() method.
      if (volume.devicePath) {
        return `entry-list://${RootType.REMOVABLE}/${volume.devicePath}`;
      }
      return `entry-list://${RootType.REMOVABLE}`;
    };
export const removableGroupKey =
    (volume: Volume|chrome.fileManagerPrivate.VolumeMetadata) =>
        `${volume.devicePath}/${volume.driveLabel}`;

export function getVolumeTypesNestedInMyFiles() {
  const myFilesNestedVolumeTypes = new Set<VolumeType>([
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
    volumeType: volumeMetadata.volumeType as VolumeType,
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
  const volume = state.volumes[volumeId];
  if (!volume) {
    console.warn(`Volume not found in the store: ${volumeId}`);
    return;
  }
  return {
    ...volume,
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
    ...currentState.allEntries[volumeRootKey]!,
    isEjectable: (volumeInfo.source === Source.DEVICE &&
                  volumeInfo.volumeType !== VolumeType.MTP) ||
        volumeInfo.source === Source.FILE,
    shouldDelayLoadingChildren: volumeInfo.source === Source.NETWORK &&
        (volumeInfo.volumeType === VolumeType.PROVIDED ||
         volumeInfo.volumeType === VolumeType.SMB),
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

  // When we manipulate the children below, we need to update both
  // `entry.children_` (usually via `appendChildIfNotExisted/removeChildEntry`)
  // and also `FileData.children`. This is specific for the purpose of Directory
  // tree rendering, the tree item only fetch its sub directories on the first
  // render or when it's being expanded, if a volume mount introduces new
  // children (e.g. Drive volume and its children), the Tree UI doesn't know it
  // needs to be re-fetch sub directories because no file watcher event is
  // triggered for certain cases, hence updating the `FileData.children` here.

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
      // Also copy the FileData children of the entry list to the real volume
      // entry.
      const myFilesEntryListFileData =
          getFileData(currentState, myFilesEntryListKey)!;
      const myFilesVolumeEntryFileData =
          getFileData(currentState, volumeRootKey)!;
      currentState.allEntries[volumeRootKey] = {
        ...myFilesVolumeEntryFileData,
        children: [...myFilesEntryListFileData.children],
      };
      // Remove MyFiles entry list from the uiEntries.
      currentState.uiEntries = currentState.uiEntries.filter(
          uiEntryKey => uiEntryKey !== myFilesEntryListKey);
    }
  }

  // Handles Drive volume.
  // It nests the Drive root (aka MyDrive) inside a EntryList for "Google
  // Drive", and also the fake entries for "Offline" and "Shared with me".
  if (volume.volumeType === VolumeType.DRIVE) {
    let driveFakeRoot: EntryList|null =
        getEntry(currentState, driveRootEntryListKey) as EntryList;
    if (!driveFakeRoot) {
      driveFakeRoot =
          new EntryList(str('DRIVE_DIRECTORY_LABEL'), RootType.DRIVE_FAKE_ROOT);
      cacheEntries(currentState, [driveFakeRoot]);
      currentState.uiEntries =
          [...currentState.uiEntries, driveFakeRoot.toURL()];
    }
    const driveRootFileDataChildren: FileKey[] = [];

    appendChildIfNotExisted(driveFakeRoot, newVolumeEntry);
    driveRootFileDataChildren.push(volumeRootKey);

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
      cacheEntries(currentState, [sharedDriveDisplayRoot]);
      appendChildIfNotExisted(driveFakeRoot, sharedDriveDisplayRoot);
      // Do not add Shared drives to the FileData children, as we should only
      // show it in the navigation when it has children inside.
    }

    // Add "Computer" grand root into Drive. It's guaranteed to be resolved at
    // this moment because ADD_VOLUME action will only be triggered after
    // resolving all roots.
    if (computersDisplayRoot) {
      cacheEntries(currentState, [computersDisplayRoot]);
      appendChildIfNotExisted(driveFakeRoot, computersDisplayRoot);
      // Do not add Computers to the FileData children, as we should only show
      // it in the navigation when it has children inside.
    }

    // Add "Shared with me" into Drive.
    const fakeSharedWithMe = fakeEntries[RootType.DRIVE_SHARED_WITH_ME];
    if (fakeSharedWithMe) {
      cacheEntries(currentState, [fakeSharedWithMe]);
      currentState.uiEntries =
          [...currentState.uiEntries, fakeSharedWithMe.toURL()];
      appendChildIfNotExisted(driveFakeRoot, fakeSharedWithMe);
      driveRootFileDataChildren.push(fakeSharedWithMe.toURL());
    }

    // Add "Offline" into Drive.
    const fakeOffline = fakeEntries[RootType.DRIVE_OFFLINE];
    if (fakeOffline) {
      cacheEntries(currentState, [fakeOffline]);
      currentState.uiEntries = [...currentState.uiEntries, fakeOffline.toURL()];
      appendChildIfNotExisted(driveFakeRoot, fakeOffline);
      driveRootFileDataChildren.push(fakeOffline.toURL());
    }

    currentState.allEntries[driveRootEntryListKey] = {
      ...getFileData(currentState, driveRootEntryListKey)!,
      children: driveRootFileDataChildren,
    };
    volume.prefixKey = driveFakeRoot.toURL();
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
            volumeMetadata.driveLabel || '', RootType.REMOVABLE,
            volumeMetadata.devicePath);
        cacheEntries(currentState, [parentEntry]);
        currentState.uiEntries =
            [...currentState.uiEntries, parentEntry.toURL()];
      }
      const partitionChildEntries: Array<Entry|FilesAppEntry> = [];
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
              partitionChildEntries.push(fileData.entry);
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
      partitionChildEntries.push(newVolumeEntry);
      volume.prefixKey = parentEntry.toURL();
      // For sub-partition from a removable volume, its children icon should be
      // UNKNOWN_REMOVABLE, and it shouldn't be ejectable.
      const fileData = getFileData(currentState, volumeRootKey)!;
      currentState.allEntries[volumeRootKey] = {
        ...fileData,
        icon: constants.ICON_TYPES.UNKNOWN_REMOVABLE,
        isEjectable: false,
      };
      currentState.allEntries[parentKey] = {
        ...getFileData(currentState, parentKey)!,
        // Removable devices with group, its parent should always be ejectable.
        isEjectable: true,
        children: sortEntries(parentEntry, partitionChildEntries)
                      .map(entry => entry.toURL()),
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

  if (!volumeToRemove.prefixKey) {
    return {...currentState};
  }
  // We also need to remove it from its prefix entry if there is one.
  const prefixEntryFileData =
      getFileData(currentState, volumeToRemove.prefixKey);
  if (prefixEntryFileData) {
    const prefixEntry = prefixEntryFileData.entry as EntryList | VolumeEntry;
    // Remove it from the prefix entry's UI children.
    prefixEntry.removeChildEntry(volumeEntry);
    // Remove it from the prefix entry's file data.
    let newChildren = prefixEntryFileData.children.filter(
        child => child !== volumeEntry.toURL());

    // If the prefix entry is an entry list for removable partitions, and this
    // is the last child, remove the prefix entry.
    if (prefixEntry.rootType === RootType.REMOVABLE &&
        newChildren.length === 0) {
      currentState.uiEntries = currentState.uiEntries.filter(
          uiEntryKey => uiEntryKey !== volumeToRemove.prefixKey!);
    }
    // If the volume entry is under MyFiles, we need to add the placeholder
    // entry back after the corresponding volume is removed (e.g. Crostini/Play
    // files).
    const volumeTypesNestedInMyFiles = getVolumeTypesNestedInMyFiles();
    const uiEntryKey = currentState.uiEntries.find(entryKey => {
      const uiEntry = getEntry(currentState, entryKey)!;
      return uiEntry.name === volumeEntry.name;
    });
    if (volumeTypesNestedInMyFiles.has(volumeToRemove.volumeType) &&
        uiEntryKey) {
      // Re-add the corresponding placeholder ui entry to the UI children.
      const uiEntry = getEntry(currentState, uiEntryKey)!;
      prefixEntry.addEntry(uiEntry);
      // Re-add the corresponding placeholder ui entry to the file data.
      newChildren = newChildren.concat(uiEntryKey);
      const childEntries =
          newChildren.map(childKey => getEntry(currentState, childKey)!);
      newChildren =
          sortEntries(prefixEntry, childEntries).map(entry => entry.toURL());
    }
    currentState.allEntries[volumeToRemove.prefixKey] = {
      ...prefixEntryFileData,
      children: newChildren,
    };
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
  const volumes: typeof State['volumes'] = {
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
