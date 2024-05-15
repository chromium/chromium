// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {VolumeInfo} from '../../background/js/volume_info.js';
import {isOneDriveId, isSameEntry} from '../../common/js/entry_utils.js';
import type {FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import {EntryList, VolumeEntry} from '../../common/js/files_app_entry_types.js';
import {isGuestOsEnabled, isSinglePartitionFormatEnabled} from '../../common/js/flags.js';
import {str} from '../../common/js/translations.js';
import type {GetActionFactoryPayload} from '../../common/js/util.js';
import {RootType, Source, VolumeType} from '../../common/js/volume_manager_types.js';
import {ICON_TYPES, ODFS_EXTENSION_ID} from '../../foreground/js/constants.js';
import type {ActionsProducerGen} from '../../lib/actions_producer.js';
import {Slice} from '../../lib/base_store.js';
import {PropStatus, type State, type Volume, type VolumeId} from '../../state/state.js';
import type {FileKey} from '../file_key.js';
import {getEntry, getFileData, getStore} from '../store.js';

import {cacheEntries, getMyFiles, readSubDirectories, updateFileDataInPlace} from './all_entries.js';
import {updateDeviceConnectionState} from './device.js';
import {removeUiEntry} from './ui_entries.js';

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
export const oneDriveFakeRootKey =
    `fake-entry://${RootType.PROVIDED}/${ODFS_EXTENSION_ID}`;
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

function appendChildIfNotExisted(
    parentEntry: VolumeEntry|EntryList,
    childEntry: Entry|FilesAppEntry): boolean {
  if (!parentEntry.getUiChildren().find(
          (entry) => isSameEntry(entry, childEntry))) {
    parentEntry.addEntry(childEntry);
    return true;
  }

  return false;
}

/**
 * Given a volume info, check if we need to group it into a wrapper.
 *
 * When the "SinglePartitionFormat" flag is on, we always group removable volume
 * even there's only 1 partition, otherwise the group only happens when there
 * are more than 1 partition in the same device.
 */
function shouldGroupRemovable(
    volumes: State['volumes'], volumeInfo: VolumeInfo,
    volumeMetadata: chrome.fileManagerPrivate.VolumeMetadata): boolean {
  if (isSinglePartitionFormatEnabled()) {
    return true;
  }
  const groupingKey = removableGroupKey(volumeMetadata);
  return Object.values<Volume>(volumes).some(v => {
    return (
        v.volumeType === VolumeType.REMOVABLE &&
        removableGroupKey(v) === groupingKey &&
        v.volumeId !== volumeInfo.volumeId);
  });
}

/** Create action to add a volume. */
const addVolumeInternal = slice.addReducer('add', addVolumeReducer);

function addVolumeReducer(currentState: State, payload: {
  volumeMetadata: chrome.fileManagerPrivate.VolumeMetadata,
  volumeInfo: VolumeInfo,
}): State {
  const {volumeMetadata, volumeInfo} = payload;

  // Cache entries, so the reducers can use any entry from `allEntries`.
  const newVolumeEntry = new VolumeEntry(payload.volumeInfo);
  cacheEntries(currentState, [newVolumeEntry]);
  const volumeRootKey = newVolumeEntry.toURL();

  // Update isEjectable fields in the FileData.
  currentState.allEntries[volumeRootKey] = {
    ...currentState.allEntries[volumeRootKey]!,
    isEjectable: (volumeInfo.source === Source.DEVICE &&
                  volumeInfo.volumeType !== VolumeType.MTP) ||
        volumeInfo.source === Source.FILE,
  };

  const volume =
      convertVolumeInfoAndMetadataToVolume(volumeInfo, volumeMetadata);
  // Use volume entry's disabled property because that one is derived from
  // volume manager.
  volume.isDisabled = !!newVolumeEntry.disabled;

  // Handles volumes nested inside MyFiles, if local user files are allowed.
  // It creates a placeholder for MyFiles if MyFiles volume isn't mounted yet.
  const myFilesNestedVolumeTypes = getVolumeTypesNestedInMyFiles();
  const {myFilesEntry} = getMyFiles(currentState);
  // For volumes which are supposed to be nested inside MyFiles (e.g. Android,
  // Crostini, GuestOS), we need to nest them into MyFiles and remove the
  // placeholder fake entry if existed.
  if (myFilesEntry && myFilesNestedVolumeTypes.has(volume.volumeType)) {
    volume.prefixKey = myFilesEntry.toURL();

    // Nest the entry for the new volume info in MyFiles.
    const uiEntryPlaceholder = myFilesEntry.getUiChildren().find(
        childEntry => childEntry.name === newVolumeEntry.name);
    // Remove a placeholder for the currently mounting volume.
    if (uiEntryPlaceholder) {
      myFilesEntry.removeChildEntry(uiEntryPlaceholder);
      // Do not remove the placeholder ui entry from the store. Removing it from
      // the MyFiles is sufficient to prevent it from showing in the directory
      // tree. We keep it in the store (`currentState["uiEntries"]`) because
      // when the corresponding volume unmounts, we need to use its existence to
      // decide if we need to re-add the placeholder back to MyFiles.
    }
    appendChildIfNotExisted(myFilesEntry, newVolumeEntry);
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
      const uiChildren = [...myFilesEntryList.getUiChildren()];
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
    let driveFakeRoot: EntryList|null =
        getEntry(currentState, driveRootEntryListKey) as EntryList;
    if (!driveFakeRoot) {
      driveFakeRoot =
          new EntryList(str('DRIVE_DIRECTORY_LABEL'), RootType.DRIVE_FAKE_ROOT);
      cacheEntries(currentState, [driveFakeRoot]);
    }
    // When Drive is disabled via pref change, the root key in `uiEntries` will
    // be removed immediately but the corresponding entry in `allEntries` is
    // removed asynchronously. When Drive is enabled again, it's possible the
    // entry is still in `allEntries` but we don't have root key in `uiEntries`.
    if (!currentState.uiEntries.includes(driveFakeRoot.toURL())) {
      currentState.uiEntries =
          [...currentState.uiEntries, driveFakeRoot.toURL()];
    }
    // We want the order to be
    // - My Drive
    // - Shared Drives (if the user has any)
    // - Computers (if the user has any)
    // - Shared with me
    // - Offline
    //
    // Clear all existing UI children to make sure we can maintain the append
    // order. For example: when Drive is disconnected and then reconnected, if
    // we don't clear current children, all other children are still there and
    // only "My Drive" will be re-added at the end.
    driveFakeRoot.removeAllChildren();
    driveFakeRoot.addEntry(newVolumeEntry);

    const {sharedDriveDisplayRoot, computersDisplayRoot, fakeEntries} =
        volumeInfo;
    // Add "Shared drives" (team drives) grand root into Drive. It's guaranteed
    // to be resolved at this moment because ADD_VOLUME action will only be
    // triggered after resolving all roots.
    if (sharedDriveDisplayRoot) {
      cacheEntries(currentState, [sharedDriveDisplayRoot]);
      driveFakeRoot.addEntry(sharedDriveDisplayRoot);
    }

    // Add "Computer" grand root into Drive. It's guaranteed to be resolved at
    // this moment because ADD_VOLUME action will only be triggered after
    // resolving all roots.
    if (computersDisplayRoot) {
      cacheEntries(currentState, [computersDisplayRoot]);
      driveFakeRoot.addEntry(computersDisplayRoot);
    }

    // Add "Shared with me" into Drive.
    const fakeSharedWithMe = fakeEntries[RootType.DRIVE_SHARED_WITH_ME];
    if (fakeSharedWithMe) {
      cacheEntries(currentState, [fakeSharedWithMe]);
      currentState.uiEntries =
          [...currentState.uiEntries, fakeSharedWithMe.toURL()];
      driveFakeRoot.addEntry(fakeSharedWithMe);
    }

    // Add "Offline" into Drive.
    const fakeOffline = fakeEntries[RootType.DRIVE_OFFLINE];
    if (fakeOffline) {
      cacheEntries(currentState, [fakeOffline]);
      currentState.uiEntries = [...currentState.uiEntries, fakeOffline.toURL()];
      driveFakeRoot.addEntry(fakeOffline);
    }

    volume.prefixKey = driveFakeRoot.toURL();
  }

  // Handles Removable volume.
  // It may nest in a EntryList if one device has multiple partitions.
  if (volume.volumeType === VolumeType.REMOVABLE) {
    const groupingKey = removableGroupKey(volumeMetadata);
    const shouldGroup =
        shouldGroupRemovable(currentState.volumes, volumeInfo, volumeMetadata);

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
      // Update the siblings too.
      Object.values<Volume>(currentState.volumes)
          .filter(
              v => v.volumeType === VolumeType.REMOVABLE &&
                  removableGroupKey(v) === groupingKey,
              )
          .forEach(v => {
            const fileData = getFileData(currentState, v.rootKey!);
            if (!fileData || !fileData?.entry) {
              return;
            }
            if (!v.prefixKey) {
              v.prefixKey = parentEntry!.toURL();
              appendChildIfNotExisted(parentEntry!, fileData.entry);
              // For sub-partition from a removable volume, its children icon
              // should be UNKNOWN_REMOVABLE, and it shouldn't be ejectable.
              currentState.allEntries[v.rootKey!] = {
                ...fileData,
                icon: ICON_TYPES.UNKNOWN_REMOVABLE,
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
      const fileData = getFileData(currentState, volumeRootKey)!;
      currentState.allEntries[volumeRootKey] = {
        ...fileData,
        icon: ICON_TYPES.UNKNOWN_REMOVABLE,
        isEjectable: false,
      };
      currentState.allEntries[parentKey] = {
        ...getFileData(currentState, parentKey)!,
        // Removable devices with group, its parent should always be ejectable.
        isEjectable: true,
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

export async function*
    addVolume(
        volumeInfo: VolumeInfo,
        volumeMetadata: chrome.fileManagerPrivate.VolumeMetadata):
        ActionsProducerGen {
  if (!volumeInfo.fileSystem) {
    console.error(
        'Only add to the store volumes that have successfully resolved.');
    return;
  }

  yield addVolumeInternal({volumeInfo, volumeMetadata});

  // For volume changes which involves UI children change, we need to trigger a
  // re-scan for the parent entry to populate the FileData.children with its UI
  // children.
  let fileKeyToScan: FileKey|null = null;

  const store = getStore();
  const state = store.getState();
  const myFilesNestedVolumeTypes = getVolumeTypesNestedInMyFiles();
  const {myFilesEntry} = getMyFiles(state);

  if (myFilesNestedVolumeTypes.has(volumeInfo.volumeType) ||
      volumeInfo.volumeType === VolumeType.DOWNLOADS) {
    // Adding volumes which are supposed to be nested inside MyFiles (e.g.
    // Android, Crostini, GuestOS) will modify MyFiles's UI children, re-scan
    // required.
    // Adding MyFiles volume might inherit UI children from its placeholder,
    // which modifies MyFiles's UI children, re-scan required.
    if (myFilesEntry) {
      fileKeyToScan = myFilesEntry.toURL();
    }
  }

  if (volumeInfo.volumeType === VolumeType.DRIVE) {
    // Adding Drive volume updates UI children for Drive root entry list,
    // re-scan required.
    fileKeyToScan = driveRootEntryListKey;
  }

  if (volumeInfo.volumeType === VolumeType.REMOVABLE) {
    // Adding Removable volume which requires grouping updates UI children for
    // the wrapper entry list, re-scan required.
    const shouldGroup =
        shouldGroupRemovable(state.volumes, volumeInfo, volumeMetadata);
    if (shouldGroup) {
      fileKeyToScan = makeRemovableParentKey(volumeMetadata);
    }
  }

  if (!fileKeyToScan) {
    return;
  }
  store.dispatch(readSubDirectories(fileKeyToScan));
}

/** Create action to remove a volume. */
const removeVolumeInternal = slice.addReducer('remove', removeVolumeReducer);

function removeVolumeReducer(currentState: State, payload: {
  volumeId: VolumeId,
}): State {
  delete currentState.volumes[payload.volumeId];
  currentState.volumes = {
    ...currentState.volumes,
  };

  return {...currentState};
}

export async function* removeVolume(volumeId: VolumeId): ActionsProducerGen {
  const store = getStore();
  const state = store.getState();
  const volumeToRemove: Volume|undefined = state.volumes[volumeId];
  if (!volumeToRemove) {
    // Somehow the volume is already removed from the store, do nothing.
    return;
  }

  yield removeVolumeInternal({volumeId});

  const volumeEntry = getEntry(state, volumeToRemove.rootKey!);
  if (!volumeEntry) {
    return;
  }
  if (!volumeToRemove.prefixKey) {
    return;
  }

  // We also need to remove it from its prefix entry if there is one.
  const prefixEntryFileData = getFileData(state, volumeToRemove.prefixKey);
  if (!prefixEntryFileData) {
    return;
  }
  const prefixEntry = prefixEntryFileData.entry as EntryList | VolumeEntry;
  // Remove it from the prefix entry's UI children.
  prefixEntry.removeChildEntry(volumeEntry);

  // If the prefix entry is an entry list for removable partitions, and this
  // is the last child, remove the prefix entry.
  if (prefixEntry.rootType === RootType.REMOVABLE &&
      prefixEntry.getUiChildren().length === 0) {
    store.dispatch(removeUiEntry(volumeToRemove.prefixKey));
    // No scan is required because the prefix entry is removed.
    return;
  }
  // If the volume entry is under MyFiles, we need to add the placeholder
  // entry back after the corresponding volume is removed (e.g.
  // Crostini/Play files).
  const volumeTypesNestedInMyFiles = getVolumeTypesNestedInMyFiles();
  const uiEntryKey = state.uiEntries.find(entryKey => {
    const uiEntry = getEntry(state, entryKey)!;
    return uiEntry.name === volumeEntry.name;
  });
  if (volumeTypesNestedInMyFiles.has(volumeToRemove.volumeType) && uiEntryKey) {
    // Re-add the corresponding placeholder ui entry to the UI children.
    const uiEntry = getEntry(state, uiEntryKey)!;
    prefixEntry.addEntry(uiEntry);
  }
  // The UI children for the prefix entry has been changed, re-scan required.
  store.dispatch(readSubDirectories(volumeToRemove.prefixKey));
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
  } as Volume;

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
    payload: GetActionFactoryPayload<typeof updateDeviceConnectionState>):
    State {
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
    updateFileDataInPlace(
        currentState, volume.rootKey!, {disabled: disableODFS});
    const odfsVolumeEntry =
        getEntry(currentState, volume.rootKey!) as VolumeEntry;
    if (odfsVolumeEntry) {
      odfsVolumeEntry.disabled = disableODFS;
    }
  }

  return volumes ? {...currentState, volumes} : currentState;
}
