// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getParentEntry} from '../../common/js/api.js';
import {DialogType} from '../../common/js/dialog_type.js';
import {isDriveRootEntryList, isFakeEntryInDrives, isGrandRootEntryInDrives, isVolumeEntry, sortEntries} from '../../common/js/entry_utils.js';
import {FileType} from '../../common/js/file_type.js';
import {EntryList, VolumeEntry} from '../../common/js/files_app_entry_types.js';
import {recordInterval, recordSmallCount, startInterval} from '../../common/js/metrics.js';
import {getEntryLabel, str} from '../../common/js/translations.js';
import {iconSetToCSSBackgroundImageValue} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {EntryLocation} from '../../externs/entry_location.js';
import {FilesAppDirEntry, FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';
import {CurrentDirectory, EntryType, FileData, State, Volume, VolumeMap} from '../../externs/ts/state.js';
import type {VolumeInfo} from '../../externs/volume_info.js';
import {constants} from '../../foreground/js/constants.js';
import {MetadataItem} from '../../foreground/js/metadata/metadata_item.js';
import type {ActionsProducerGen} from '../../lib/actions_producer.js';
import {Slice} from '../../lib/base_store.js';
import type {FileKey} from '../file_key.js';
import {getEntry, getFileData, getStore} from '../store.js';

import {hasDlpDisabledFiles} from './current_directory.js';
import {driveRootEntryListKey, myFilesEntryListKey, recentRootKey} from './volumes.js';

/**
 * @fileoverview Entries slice of the store.
 * @suppress {checkTypes} TS already checks this file.
 */

const slice = new Slice<State, State['allEntries']>('allEntries');
export {slice as allEntriesSlice};

/**
 * Create action to scan `allEntries` and remove its stale entries.
 */
export const clearCachedEntries =
    slice.addReducer('clear-stale-cache', clearCachedEntriesReducer);

function clearCachedEntriesReducer(state: State): State {
  const entries = state.allEntries;
  const currentDirectoryKey = state.currentDirectory?.key;
  const entriesToKeep = new Set<string>();

  if (currentDirectoryKey) {
    entriesToKeep.add(currentDirectoryKey);

    for (const component of state.currentDirectory!.pathComponents) {
      entriesToKeep.add(component.key);
    }

    for (const key of state.currentDirectory!.content.keys) {
      entriesToKeep.add(key);
    }
  }
  const selectionKeys = state.currentDirectory?.selection.keys ?? [];
  if (selectionKeys) {
    for (const key of selectionKeys) {
      entriesToKeep.add(key);
    }
  }

  for (const volume of Object.values<Volume>(state.volumes)) {
    if (!volume.rootKey) {
      continue;
    }
    entriesToKeep.add(volume.rootKey);
    if (volume.prefixKey) {
      entriesToKeep.add(volume.prefixKey);
    }
  }

  for (const key of state.uiEntries) {
    entriesToKeep.add(key);
  }

  for (const key of state.folderShortcuts) {
    entriesToKeep.add(key);
  }

  for (const root of state.navigation.roots) {
    entriesToKeep.add(root.key);
  }

  // For all expanded entries, we need to keep them and all their direct
  // children.
  for (const key of Object.keys(entries)) {
    const fileData = entries[key];
    if (fileData.expanded) {
      entriesToKeep.add(key);
      if (fileData.children) {
        for (const child of fileData.children) {
          entriesToKeep.add(child);
        }
      }
    }
  }

  // For all kept entries, we also need to keep their children so we can decide
  // if we need to show the expand icon or not.
  for (const key of entriesToKeep) {
    const fileData = entries[key];
    if (fileData?.children) {
      for (const child of fileData.children) {
        entriesToKeep.add(child);
      }
    }
  }

  for (const key of Object.keys(entries)) {
    if (entriesToKeep.has(key)) {
      continue;
    }

    delete entries[key];
  }

  return state;
}

/**
 * Schedules the routine to remove stale entries from `allEntries`.
 */
function scheduleClearCachedEntries() {
  if (clearCachedEntriesRequestId === 0) {
    clearCachedEntriesRequestId = requestIdleCallback(startClearCache);
  }
}

/** ID for the current scheduled `clearCachedEntries`. */
let clearCachedEntriesRequestId = 0;

/** Starts the action CLEAR_STALE_CACHED_ENTRIES.  */
function startClearCache() {
  const store = getStore();
  store.dispatch(clearCachedEntries());
  clearCachedEntriesRequestId = 0;
}

const prefetchPropertyNames = Array.from(new Set([
  ...constants.LIST_CONTAINER_METADATA_PREFETCH_PROPERTY_NAMES,
  ...constants.ACTIONS_MODEL_METADATA_PREFETCH_PROPERTY_NAMES,
  ...constants.FILE_SELECTION_METADATA_PREFETCH_PROPERTY_NAMES,
  ...constants.DLP_METADATA_PREFETCH_PROPERTY_NAMES,
]));

/** Get the icon for an entry. */
function getEntryIcon(
    entry: Entry|FilesAppEntry, locationInfo: EntryLocation|null,
    volumeType: VolumeManagerCommon.VolumeType|null): FileData['icon'] {
  const url = entry.toURL();

  // Pre-defined icons based on the URL.
  const urlToIconPath = {
    [recentRootKey]: constants.ICON_TYPES.RECENT,
    [myFilesEntryListKey]: constants.ICON_TYPES.MY_FILES,
    [driveRootEntryListKey]: constants.ICON_TYPES.SERVICE_DRIVE,
  };

  if (urlToIconPath[url]) {
    return urlToIconPath[url]!;
  }

  // Handle icons for grand roots ("Shared drives" and "Computers") in Drive.
  // Here we can't just use `fullPath` to check if an entry is a grand root or
  // not, because normal directory can also have the same full path. We also
  // need to check if the entry is a direct child of the drive root entry list.
  const grandRootPathToIconMap = {
    [VolumeManagerCommon.COMPUTERS_DIRECTORY_PATH]:
        constants.ICON_TYPES.COMPUTERS_GRAND_ROOT,
    [VolumeManagerCommon.SHARED_DRIVES_DIRECTORY_PATH]:
        constants.ICON_TYPES.SHARED_DRIVES_GRAND_ROOT,
  };
  if (volumeType === VolumeManagerCommon.VolumeType.DRIVE &&
      grandRootPathToIconMap[entry.fullPath]) {
    return grandRootPathToIconMap[entry.fullPath]!;
  }

  // For grouped removable devices, its parent folder is an entry list, we
  // should use USB icon for it.
  if ('rootType' in entry &&
      entry.rootType === VolumeManagerCommon.VolumeType.REMOVABLE) {
    return constants.ICON_TYPES.USB;
  }

  if (isVolumeEntry(entry) && entry.volumeInfo) {
    switch (entry.volumeInfo.volumeType) {
      case VolumeManagerCommon.VolumeType.DOWNLOADS:
        return constants.ICON_TYPES.MY_FILES;
      case VolumeManagerCommon.VolumeType.SMB:
        return constants.ICON_TYPES.SMB;
      case VolumeManagerCommon.VolumeType.PROVIDED:
      // Fallthrough
      case VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER: {
        // Only return IconSet if there's valid background image generated.
        const iconSet = entry.volumeInfo.iconSet;
        if (iconSet) {
          const backgroundImage =
              iconSetToCSSBackgroundImageValue(entry.volumeInfo.iconSet);
          if (backgroundImage !== 'none') {
            return iconSet;
          }
        }
        // If no background is generated from IconSet, set the icon to the
        // generic one for certain volume type.
        if (volumeType && VolumeManagerCommon.shouldProvideIcons(volumeType)) {
          return constants.ICON_TYPES.GENERIC;
        }
        return '';
      }
      case VolumeManagerCommon.VolumeType.MTP:
        return constants.ICON_TYPES.MTP;
      case VolumeManagerCommon.VolumeType.ARCHIVE:
        return constants.ICON_TYPES.ARCHIVE;
      case VolumeManagerCommon.VolumeType.REMOVABLE:
        // For sub-partition from a removable volume, its children icon should
        // be UNKNOWN_REMOVABLE.
        return entry.volumeInfo.prefixEntry ?
            constants.ICON_TYPES.UNKNOWN_REMOVABLE :
            constants.ICON_TYPES.USB;
      case VolumeManagerCommon.VolumeType.DRIVE:
        return constants.ICON_TYPES.DRIVE;
    }
  }

  return FileType.getIcon(entry as Entry, undefined, locationInfo?.rootType);
}

/**
 * Converts the entry to the Store representation of an Entry: FileData.
 */
export function convertEntryToFileData(entry: Entry|FilesAppEntry): FileData {
  const {volumeManager, metadataModel} = window.fileManager;
  // When this function is triggered when mounting new volumes, volumeInfo is
  // not available in the VolumeManager yet, we need to get volumeInfo from the
  // entry itself.
  const volumeInfo = 'volumeInfo' in entry ? entry.volumeInfo as VolumeInfo :
                                             volumeManager.getVolumeInfo(entry);
  const locationInfo = volumeManager.getLocationInfo(entry);
  const label = getEntryLabel(locationInfo, entry);
  // For FakeEntry, we need to read from entry.volumeType because it doesn't
  // have volumeInfo in the volume manager.
  const volumeType = 'volumeType' in entry && entry.volumeType ?
      entry.volumeType as VolumeManagerCommon.VolumeType :
      (volumeInfo?.volumeType || null);
  const volumeId = volumeInfo?.volumeId || null;
  const icon = getEntryIcon(entry, locationInfo, volumeType);

  /**
   * Update disabled attribute if entry supports disabled attribute and has a
   * non-null volumeType.
   */
  if ('disabled' in entry && volumeType) {
    entry.disabled = volumeManager.isDisabled(volumeType);
  }

  const metadata = metadataModel ?
      metadataModel.getCache([entry as FileEntry], prefetchPropertyNames)[0]! :
      {} as MetadataItem;

  return {
    entry,
    icon,
    type: getEntryType(entry),
    isDirectory: entry.isDirectory,
    label,
    volumeId,
    rootType: locationInfo?.rootType ?? null,
    metadata,
    expanded: false,
    disabled: 'disabled' in entry ? entry.disabled as boolean : false,
    isRootEntry: !!locationInfo?.isRootEntry,
    // `isEjectable/shouldDelayLoadingChildren` is determined by its
    // corresponding volume, will be updated when volume is added.
    isEjectable: false,
    shouldDelayLoadingChildren: false,
    children: [],
  };
}

/**
 * Appends the entry to the Store.
 */
function appendEntry(state: State, entry: Entry|FilesAppEntry) {
  const allEntries = state.allEntries || {};
  const key = entry.toURL();
  const existingFileData = allEntries[key] || {};

  // Some client code might dispatch actions based on
  // `volume.resolveDisplayRoot()` which is a DirectoryEntry instead of a
  // VolumeEntry. It's safe to ignore this entry because the data will be the
  // same as `existingFileData` and we don't want to convert from VolumeEntry to
  // DirectoryEntry.
  if (existingFileData.type === EntryType.VOLUME_ROOT &&
      getEntryType(entry) !== EntryType.VOLUME_ROOT) {
    return;
  }

  const fileData = convertEntryToFileData(entry);

  allEntries[key] = {
    ...fileData,
    // For existing entries already in the store, we want to keep the existing
    // value for the following fields. For example, for "expanded" entries with
    // expanded=true, we don't want to override it with expanded=false derived
    // from `convertEntryToFileData` function above.
    expanded: existingFileData.expanded || fileData.expanded,
    isEjectable: existingFileData.isEjectable || fileData.isEjectable,
    shouldDelayLoadingChildren: existingFileData.shouldDelayLoadingChildren ||
        fileData.shouldDelayLoadingChildren,
    // Keep children to prevent sudden removal of the children items on the UI.
    children: existingFileData.children || fileData.children,
  };

  state.allEntries = allEntries;
}

/**
 * Updates `FileData` from a `FileKey`.
 */
export function updateFileData(
    state: State, key: FileKey, changes: Partial<FileData>): FileData|
    undefined {
  if (!state.allEntries[key]) {
    console.warn(`Entry FileData not found in the store: ${key}`);
    return;
  }
  const newFileData = {
    ...state.allEntries[key],
    ...changes,
  };
  state.allEntries[key] = newFileData;
  return newFileData;
}

/** Caches the Action's entry in the `allEntries` attribute. */
export function cacheEntries(
    currentState: State, entries: Array<Entry|FilesAppEntry>) {
  scheduleClearCachedEntries();
  for (const entry of entries) {
    appendEntry(currentState, entry);
  }
}

function getEntryType(entry: Entry|FilesAppEntry): EntryType {
  // Entries from FilesAppEntry have the `type_name` property.
  if (!('type_name' in entry)) {
    return EntryType.FS_API;
  }

  switch (entry.type_name) {
    case 'EntryList':
      return EntryType.ENTRY_LIST;
    case 'VolumeEntry':
      return EntryType.VOLUME_ROOT;
    case 'FakeEntry':
      switch (entry.rootType) {
        case VolumeManagerCommon.RootType.RECENT:
          return EntryType.RECENT;
        case VolumeManagerCommon.RootType.TRASH:
          return EntryType.TRASH;
        case VolumeManagerCommon.RootType.DRIVE_FAKE_ROOT:
          return EntryType.ENTRY_LIST;
        case VolumeManagerCommon.RootType.CROSTINI:
        case VolumeManagerCommon.RootType.ANDROID_FILES:
          return EntryType.PLACEHOLDER;
        case VolumeManagerCommon.RootType.DRIVE_OFFLINE:
        case VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME:
          // TODO(lucmult): This isn't really Recent but it's the closest.
          return EntryType.RECENT;
      }
      console.warn(`Invalid fakeEntry.rootType='${entry.rootType} rootType`);
      return EntryType.PLACEHOLDER;
    case 'GuestOsPlaceholder':
      return EntryType.PLACEHOLDER;
    case 'TrashEntry':
      return EntryType.TRASH;
    default:
      console.warn(`Invalid entry.type_name='${entry.type_name}`);
      return EntryType.FS_API;
  }
}

export interface EntryMetadata {
  entry: Entry|FilesAppEntry;
  metadata: MetadataItem;
}

/** Create action to update entries metadata. */
export const updateMetadata =
    slice.addReducer('update-metadata', updateMetadataReducer);

function updateMetadataReducer(currentState: State, payload: {
  metadata: EntryMetadata[],
}): State {
  // Cache entries, so the reducers can use any entry from `allEntries`.
  cacheEntries(currentState, payload.metadata.map(m => m.entry));

  for (const entryMetadata of payload.metadata) {
    const key = entryMetadata.entry.toURL();
    const fileData = currentState.allEntries[key];
    const metadata = {...fileData.metadata, ...entryMetadata.metadata};
    currentState.allEntries[key] = {
      ...fileData,
      metadata,
    };
  }
  if (!currentState.currentDirectory) {
    console.warn('Missing `currentDirectory`');
    return currentState;
  }
  const currentDirectory: CurrentDirectory = {
    ...currentState.currentDirectory,
    hasDlpDisabledFiles: hasDlpDisabledFiles(currentState),
  };

  return {
    ...currentState,
    currentDirectory,
  };
}

function findVolumeByType(
    volumes: VolumeMap, volumeType: VolumeManagerCommon.VolumeType): Volume|
    null {
  return Object.values<Volume>(volumes).find(v => {
    // If the volume isn't resolved yet, we just ignore here.
    return v.rootKey && v.volumeType === volumeType;
  }) ??
      null;
}

/**
 * Returns the MyFiles entry and volume, the entry can either be a fake one
 * (EntryList) or a real one (VolumeEntry) depends on if the MyFiles volume is
 * mounted or not.
 * Note: it will create a fake EntryList in the store if there's no
 * MyFiles entry in the store (e.g. no EntryList and no VolumeEntry).
 */
export function getMyFiles(state: State):
    {myFilesVolume: null|Volume, myFilesEntry: VolumeEntry|EntryList} {
  const {volumes} = state;
  const myFilesVolume =
      findVolumeByType(volumes, VolumeManagerCommon.VolumeType.DOWNLOADS);
  const myFilesVolumeEntry = myFilesVolume ?
      getEntry(state, myFilesVolume.rootKey!) as VolumeEntry | null :
      null;
  let myFilesEntryList =
      getEntry(state, myFilesEntryListKey) as EntryList | null;
  if (!myFilesVolumeEntry && !myFilesEntryList) {
    myFilesEntryList = new EntryList(
        str('MY_FILES_ROOT_LABEL'), VolumeManagerCommon.RootType.MY_FILES);
    appendEntry(state, myFilesEntryList);
    state.uiEntries = [...state.uiEntries, myFilesEntryList.toURL()];
  }

  return {
    myFilesEntry: myFilesVolumeEntry || myFilesEntryList!,
    myFilesVolume,
  };
}


/**  Create action to add child entries to a parent entry. */
export const addChildEntries =
    slice.addReducer('add-children', addChildEntriesReducer);

function addChildEntriesReducer(currentState: State, payload: {
  parentKey: FileKey,
  entries: Array<Entry|FilesAppEntry>,
}): State {
  // Cache entries, so the reducers can use any entry from `allEntries`.
  cacheEntries(currentState, payload.entries);

  const {parentKey, entries} = payload;
  const {allEntries} = currentState;
  // The corresponding parent entry item has been removed somehow, do nothing.
  if (!allEntries[parentKey]) {
    return currentState;
  }

  const newEntryKeys = entries.map(entry => entry.toURL());
  // Add children to the parent entry item.
  const parentFileData: FileData = {
    ...allEntries[parentKey],
    children: newEntryKeys,
  };
  // We mark all the children's shouldDelayLoadingChildren if the parent entry
  // has been delayed.
  if (parentFileData.shouldDelayLoadingChildren) {
    for (const entryKey of newEntryKeys) {
      allEntries[entryKey] = {
        ...allEntries[entryKey],
        shouldDelayLoadingChildren: true,
      };
    }
  }

  return {
    ...currentState,
    allEntries: {
      ...allEntries,
      [parentKey]: parentFileData,
    },
  };
}

/**
 * Read sub directories for a given entry.
 * TODO(b/271485133): Remove successCallback/errorCallback.
 */
export async function*
    readSubDirectories(
        entry: Entry|FilesAppEntry|null, recursive: boolean = false,
        metricNameForTracking: string = ''): ActionsProducerGen {
  if (!entry || !entry.isDirectory || ('disabled' in entry && entry.disabled)) {
    return;
  }

  // Track time for reading sub directories if metric for tracking is passed.
  if (metricNameForTracking) {
    startInterval(metricNameForTracking);
  }

  // Type casting here because TS can't exclude the invalid entry types via the
  // above if checks.
  const validEntry = entry as DirectoryEntry | FilesAppDirEntry;
  const childEntriesToReadDeeper: Array<Entry|FilesAppEntry> = [];
  if (isDriveRootEntryList(validEntry)) {
    for await (
        const action of readSubDirectoriesForDriveRootEntryList(validEntry)) {
      yield action;
      if (action) {
        childEntriesToReadDeeper.push(...action.payload.entries);
      }
    }
  } else {
    const childEntries = await readChildEntriesForDirectoryEntry(validEntry);
    // Only dispatch directories.
    const subDirectories =
        childEntries.filter(childEntry => childEntry.isDirectory);
    yield addChildEntries({parentKey: entry.toURL(), entries: subDirectories});
    childEntriesToReadDeeper.push(...subDirectories);
  }

  // Track time for reading sub directories if metric for tracking is passed.
  if (metricNameForTracking) {
    recordInterval(metricNameForTracking);
  }

  // Read sub directories for children when recursive is true.
  if (recursive) {
    // We only read deeper if the parent entry is expanded in the tree.
    const fileData = getFileData(getStore().getState(), entry.toURL());
    if (fileData?.expanded) {
      for (const childEntry of childEntriesToReadDeeper) {
        for await (const action of readSubDirectories(
            childEntry, /* recursive */ true)) {
          yield action;
        }
      }
    }
  }
}

/**
 * Read entries for Drive root entry list (aka "Google Drive"), there are some
 * differences compared to the `readSubDirectoriesForDirectoryEntry`:
 * * We don't need to call readEntries to get its child entries. Instead, all
 * its children are from its entry.getUIChildren().
 * * For fake entries children (e.g. Shared with me and Offline), we only show
 * them based on the dialog type.
 * * For curtain children (e.g. team drives and computers grand root), we only
 * show them when there's at least one child entries inside. So we need to read
 * their children (grand children of drive fake root) first before we can decide
 * if we need to show them or not.
 */
async function*
    readSubDirectoriesForDriveRootEntryList(entry: EntryList):
        ActionsProducerGen {
  const metricNameMap = {
    [VolumeManagerCommon.SHARED_DRIVES_DIRECTORY_PATH]: 'TeamDrivesCount',
    [VolumeManagerCommon.COMPUTERS_DIRECTORY_PATH]: 'ComputerCount',
  };

  const driveChildren = entry.getUIChildren();
  /**
   * Store the filtered children, for fake entries or grand roots we might need
   * to hide them based on curtain conditions.
   */
  const filteredChildren: Array<Entry|FilesAppEntry> = [];

  const isFakeEntryVisible =
      window.fileManager.dialogType !== DialogType.SELECT_SAVEAS_FILE;

  for (const childEntry of driveChildren) {
    // For fake entries ("Shared with me" and)
    if (isFakeEntryInDrives(childEntry)) {
      if (isFakeEntryVisible) {
        filteredChildren.push(childEntry);
      }
      continue;
    }
    // For non grand roots (also not fake entries), we put them in the children
    // directly and dispatch an action to read the it later.
    if (!isGrandRootEntryInDrives(childEntry)) {
      filteredChildren.push(childEntry);
      continue;
    }
    // For grand roots ("Shared drives" and "Computers") inside Drive, we only
    // show them when there's at least one child entries inside.
    const grandChildEntries =
        await readChildEntriesForDirectoryEntry(childEntry);
    recordSmallCount(
        metricNameMap[childEntry.fullPath]!, grandChildEntries.length);
    if (grandChildEntries.length > 0) {
      filteredChildren.push(childEntry);
    }
  }
  yield addChildEntries({parentKey: entry.toURL(), entries: filteredChildren});
}

/**
 * Read child entries for a given directory entry.
 */
async function readChildEntriesForDirectoryEntry(
    entry: DirectoryEntry|
    FilesAppDirEntry): Promise<Array<Entry|FilesAppEntry>> {
  return new Promise<Array<Entry|FilesAppEntry>>(resolve => {
    const reader = entry.createReader();
    const subEntries: Array<Entry|FilesAppEntry> = [];
    const readEntry = () => {
      reader.readEntries((entries) => {
        if (entries.length === 0) {
          resolve(sortEntries(entry, subEntries));
          return;
        }
        for (const subEntry of entries) {
          subEntries.push(subEntry);
        }
        readEntry();
      });
    };
    readEntry();
  });
}

/**
 * Read child entries for the newly renamed directory entry.
 * We need to read its parent's children first before reading its own children,
 * because the newly renamed entry might not be in the store yet after renaming.
 */
export async function*
    readSubDirectoriesForRenamedEntry(newEntry: Entry): ActionsProducerGen {
  const parentDirectory = await getParentEntry(newEntry);
  // Read the children of the parent first to make sure the newly added entry
  // appears in the store.
  for await (const action of readSubDirectories(parentDirectory)) {
    yield action;
  }
  // Read the children of the newly renamed entry.
  for await (
      const action of readSubDirectories(newEntry, /* recursive= */ true)) {
    yield action;
  }
}
