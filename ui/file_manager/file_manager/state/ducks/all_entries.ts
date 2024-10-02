// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import type {EntryLocation} from '../../background/js/entry_location_impl.js';
import type {VolumeInfo} from '../../background/js/volume_info.js';
import {getParentEntry} from '../../common/js/api.js';
import {canHaveSubDirectories, isDirectoryEntry, isDriveRootEntryList, isEntryScannable, isEntrySupportUiChildren, isFakeEntryInDrive, isGrandRootEntryInDrive, isInsideDrive, isVolumeEntry, isVolumeFileData, readEntries, shouldSupportDriveSpecificIcons, sortEntries, supportsUiChildren, urlToEntry} from '../../common/js/entry_utils.js';
import {getIcon} from '../../common/js/file_type.js';
import type {FilesAppDirEntry, FilesAppEntry, VolumeEntry} from '../../common/js/files_app_entry_types.js';
import {EntryList} from '../../common/js/files_app_entry_types.js';
import {isSkyvaultV2Enabled} from '../../common/js/flags.js';
import {recordInterval, recordSmallCount, startInterval} from '../../common/js/metrics.js';
import {getEntryLabel, str} from '../../common/js/translations.js';
import {debug, iconSetToCSSBackgroundImageValue} from '../../common/js/util.js';
import {COMPUTERS_DIRECTORY_PATH, RootType, SHARED_DRIVES_DIRECTORY_PATH, shouldProvideIcons, Source, VolumeType} from '../../common/js/volume_manager_types.js';
import {ACTIONS_MODEL_METADATA_PREFETCH_PROPERTY_NAMES, DLP_METADATA_PREFETCH_PROPERTY_NAMES, FILE_SELECTION_METADATA_PREFETCH_PROPERTY_NAMES, ICON_TYPES, LIST_CONTAINER_METADATA_PREFETCH_PROPERTY_NAMES} from '../../foreground/js/constants.js';
import type {MetadataItem} from '../../foreground/js/metadata/metadata_item.js';
import type {ActionsProducerGen} from '../../lib/actions_producer.js';
import {isDebugStoreEnabled, Slice} from '../../lib/base_store.js';
import {keepLatest, keyedKeepLatest} from '../../lib/concurrency_models.js';
import {type CurrentDirectory, EntryType, type FileData, type MaterializedView, PropStatus, type State, type Volume, type VolumeMap} from '../../state/state.js';
import type {FileKey} from '../file_key.js';
import {getEntry, getFileData, getStore, getVolume} from '../store.js';

import {changeDirectory, hasDlpDisabledFiles} from './current_directory.js';
import {driveRootEntryListKey, myFilesEntryListKey, recentRootKey} from './volumes.js';

/**
 * @fileoverview Entries slice of the store.
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
  for (const fileData of Object.values(entries)) {
    if (fileData.expanded) {
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

  for (const view of state.materializedViews) {
    entriesToKeep.add(view.key);
  }

  const isDebugStore = isDebugStoreEnabled();
  for (const key of Object.keys(entries)) {
    if (entriesToKeep.has(key)) {
      continue;
    }

    delete entries[key];
    if (isDebugStore) {
      console.info(`Clear entry: ${key}`);
    }
  }

  return state;
}

/**
 * Schedules the routine to remove stale entries from `allEntries`.
 */
function scheduleClearCachedEntries() {
  if (clearCachedEntriesRequestId === 0) {
    // For unittest force to run at least at 50ms to avoid flakiness on slow
    // bots (msan).
    const options = window.IN_TEST ? {timeout: 50} : {};
    clearCachedEntriesRequestId = requestIdleCallback(startClearCache, options);
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
  ...LIST_CONTAINER_METADATA_PREFETCH_PROPERTY_NAMES,
  ...ACTIONS_MODEL_METADATA_PREFETCH_PROPERTY_NAMES,
  ...FILE_SELECTION_METADATA_PREFETCH_PROPERTY_NAMES,
  ...DLP_METADATA_PREFETCH_PROPERTY_NAMES,
]));

/** Get the icon for an entry. */
function getEntryIcon(
    entry: Entry|FilesAppEntry, locationInfo: EntryLocation|null,
    volumeType: VolumeType|null): FileData['icon'] {
  const url = entry.toURL();

  // Pre-defined icons based on the URL.
  const urlToIconPath: Record<FileKey, string> = {
    [recentRootKey]: ICON_TYPES.RECENT,
    [myFilesEntryListKey]: ICON_TYPES.MY_FILES,
    [driveRootEntryListKey]: ICON_TYPES.SERVICE_DRIVE,
  };

  if (urlToIconPath[url]) {
    return urlToIconPath[url]!;
  }

  // Handle icons for grand roots ("Shared drives" and "Computers") in Drive.
  // Here we can't just use `fullPath` to check if an entry is a grand root or
  // not, because normal directory can also have the same full path. We also
  // need to check if the entry is a direct child of the drive root entry list.
  const grandRootPathToIconMap = {
    [COMPUTERS_DIRECTORY_PATH]: ICON_TYPES.COMPUTERS_GRAND_ROOT,
    [SHARED_DRIVES_DIRECTORY_PATH]: ICON_TYPES.SHARED_DRIVES_GRAND_ROOT,
  };
  if (volumeType === VolumeType.DRIVE &&
      grandRootPathToIconMap[entry.fullPath]) {
    return grandRootPathToIconMap[entry.fullPath]!;
  }

  // For grouped removable devices, its parent folder is an entry list, we
  // should use USB icon for it.
  if ('rootType' in entry && entry.rootType === RootType.REMOVABLE) {
    return ICON_TYPES.USB;
  }

  if (isVolumeEntry(entry) && entry.volumeInfo) {
    switch (entry.volumeInfo.volumeType) {
      case VolumeType.DOWNLOADS:
        return ICON_TYPES.MY_FILES;
      case VolumeType.SMB:
        return ICON_TYPES.SMB;
      case VolumeType.PROVIDED:
      // Fallthrough
      case VolumeType.DOCUMENTS_PROVIDER: {
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
        if (volumeType && shouldProvideIcons(volumeType)) {
          return ICON_TYPES.GENERIC;
        }
        return '';
      }
      case VolumeType.MTP:
        return ICON_TYPES.MTP;
      case VolumeType.ARCHIVE:
        return ICON_TYPES.ARCHIVE;
      case VolumeType.REMOVABLE:
        // For sub-partition from a removable volume, its children icon should
        // be UNKNOWN_REMOVABLE.
        return entry.volumeInfo.prefixEntry ? ICON_TYPES.UNKNOWN_REMOVABLE :
                                              ICON_TYPES.USB;
      case VolumeType.DRIVE:
        return ICON_TYPES.DRIVE;
    }
  }

  return getIcon(entry as Entry, undefined, locationInfo?.rootType);
}

/**
 * Given fileData, check if its loading children should be delayed.
 * We are doing this for SMB to avoid potentially hanging whilst scanning a
 * large SMB file share and causing performance issues.
 */
export function shouldDelayLoadingChildren(
    fileData: FileData, state: State): boolean {
  // When this function is triggered when mounting new volumes, volumeInfo is
  // not available in the VolumeManager yet, we need to get volumeInfo from the
  // entry itself.
  const volume: Volume|VolumeInfo|undefined = isVolumeFileData(fileData) ?
      // TODO: Confirm how to remove the usage of entry here.
      (fileData.entry as VolumeEntry).volumeInfo :
      state!.volumes[fileData.volumeId!];
  return isVolumeSlowToScan(volume);
}

function isVolumeSlowToScan(volume?: Volume|VolumeInfo|null): boolean {
  return volume?.source === Source.NETWORK &&
      volume.volumeType === VolumeType.SMB;
}

function convertViewToFileData(view: MaterializedView): FileData {
  const metadata: MetadataItem = {};
  const fileData: FileData = {
    key: view.key,
    fullPath: new URL(view.key).pathname,
    icon: view.icon,
    type: EntryType.MATERIALIZED_VIEW,
    isDirectory: true,
    label: view.label,
    volumeId: null,
    rootType: null,
    metadata,
    expanded: false,
    disabled: false,
    isRootEntry: view.isRoot,
    canExpand: true,
    isEjectable: false,
    children: [],
  };
  return fileData;
}

/**
 * Converts the entry to the Store representation of an Entry: FileData.
 */
export function convertEntryToFileData(entry: Entry|FilesAppEntry): FileData {
  const {volumeManager, metadataModel} = window.fileManager;
  // When this function is triggered when mounting new volumes, volumeInfo is
  // not available in the VolumeManager yet, we need to get volumeInfo from the
  // entry itself.
  const volumeInfo = isVolumeEntry(entry) ? entry.volumeInfo :
                                            volumeManager.getVolumeInfo(entry);
  const locationInfo = volumeManager.getLocationInfo(entry);
  const label = getEntryLabel(locationInfo, entry);
  // For FakeEntry, we need to read from entry.volumeType because it doesn't
  // have volumeInfo in the volume manager.
  const volumeType = 'volumeType' in entry && entry.volumeType ?
      entry.volumeType as VolumeType :
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

  const fileData: FileData = {
    key: entry.toURL(),
    fullPath: entry.fullPath,
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
    canExpand: false,
    // `isEjectable` is determined by its corresponding volume, will be updated
    // when volume is added.
    isEjectable: false,
    children: [],
  };
  // For slow volumes, we always mark the root and directories as canExpand, to
  // avoid scanning to determine if it has sub-directories.
  fileData.canExpand = isVolumeSlowToScan(volumeInfo);
  return fileData;
}

function appendView(state: State, view: MaterializedView) {
  const allEntries = state.allEntries || {};
  const key = view.key;
  const fileData = convertViewToFileData(view)!;
  const existingFileData: Partial<FileData> = allEntries[key] || {};

  allEntries[key] = {
    ...fileData,
    expanded: existingFileData.expanded ?? fileData.expanded,
    isEjectable: existingFileData.isEjectable ?? fileData.isEjectable,
    canExpand: existingFileData.canExpand ?? fileData.canExpand,
    // Keep children to prevent sudden removal of the children items on the UI.
    children: existingFileData.children ?? fileData.children,
    key,
  };

  state.allEntries = allEntries;
}

/**
 * Converts an EntryData object from FileManagerPrivate API to the store
 * representation of an Entry: FileData.
 */
export async function convertEntryDataToFileData(
    entryData: chrome.fileManagerPrivate.EntryData): Promise<FileData> {
  const nativeEntry = await urlToEntry(entryData.entryUrl);
  // TODO(b/328564447): This function should only rely on `entryData`, so
  // gradually update the returned FileData to only use fields from `entryData`.
  const nativeEntryFileData = convertEntryToFileData(nativeEntry);
  return {
    key: entryData.entryUrl,
    fullPath: nativeEntryFileData.fullPath,
    entry: nativeEntry,
    icon: nativeEntryFileData.icon,
    label: nativeEntryFileData.label,
    volumeId: nativeEntryFileData.volumeId,
    rootType: nativeEntryFileData.rootType,
    metadata: nativeEntryFileData.metadata,
    isDirectory: nativeEntryFileData.isDirectory,
    type: EntryType.FS_API,
    isRootEntry: nativeEntryFileData.isRootEntry,
    isEjectable: false,
    canExpand: nativeEntryFileData.canExpand,
    children: [],
    expanded: false,
    disabled: nativeEntryFileData.disabled,
  };
}

/**
 * Appends the entry to the Store.
 */
function appendEntry(state: State, entry: Entry|FilesAppEntry) {
  const allEntries = state.allEntries || {};
  const key = entry.toURL();
  const existingFileData: Partial<FileData> = allEntries[key] || {};

  // Some client code might dispatch actions based on
  // `volume.resolveDisplayRoot()` which is a DirectoryEntry instead of a
  // VolumeEntry. It's safe to ignore this entry because the data will be the
  // same as `existingFileData` and we don't want to convert from VolumeEntry to
  // DirectoryEntry.
  if (existingFileData.type === EntryType.VOLUME_ROOT &&
      getEntryType(entry) !== EntryType.VOLUME_ROOT) {
    return;
  }

  const fileData = convertEntryToFileData(entry)!;

  allEntries[key] = {
    ...fileData,
    // For existing entries already in the store, we want to keep the existing
    // value for the following fields. For example, for "expanded" entries with
    // expanded=true, we don't want to override it with expanded=false derived
    // from `convertEntryToFileData` function above.
    expanded: existingFileData.expanded ?? fileData.expanded,
    isEjectable: existingFileData.isEjectable ?? fileData.isEjectable,
    canExpand: existingFileData.canExpand ?? fileData.canExpand,
    // Keep children to prevent sudden removal of the children items on the UI.
    children: existingFileData.children ?? fileData.children,
    key,
  };

  state.allEntries = allEntries;
}

/**
 * Updates `FileData` from a `FileKey`.
 *
 * Note: the state will be updated in place.
 */
export function updateFileDataInPlace(
    state: State, key: FileKey, changes: Partial<FileData>): FileData|
    undefined {
  if (!state.allEntries[key]) {
    console.warn(`Entry FileData not found in the store: ${key}`);
    return;
  }
  const newFileData = {
    ...state.allEntries[key]!,
    ...changes,
    key,
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

export function cacheMaterializedViews(
    currentState: State, views: MaterializedView[]) {
  scheduleClearCachedEntries();
  for (const entry of views) {
    appendView(currentState, entry);
  }
}


function getEntryType(entry: Entry|FilesAppEntry): EntryType {
  // Entries from FilesAppEntry have the `typeName` property.
  if (!('typeName' in entry)) {
    return EntryType.FS_API;
  }

  switch (entry.typeName) {
    case 'EntryList':
      return EntryType.ENTRY_LIST;
    case 'VolumeEntry':
      return EntryType.VOLUME_ROOT;
    case 'FakeEntry':
      switch (entry.rootType) {
        case RootType.RECENT:
          return EntryType.RECENT;
        case RootType.TRASH:
          return EntryType.TRASH;
        case RootType.DRIVE_FAKE_ROOT:
          return EntryType.ENTRY_LIST;
        case RootType.CROSTINI:
        case RootType.ANDROID_FILES:
          return EntryType.PLACEHOLDER;
        case RootType.DRIVE_OFFLINE:
        case RootType.DRIVE_SHARED_WITH_ME:
          // TODO(lucmult): This isn't really Recent but it's the closest.
          return EntryType.RECENT;
        case RootType.PROVIDED:
          return EntryType.PLACEHOLDER;
      }
      console.warn(`Invalid fakeEntry.rootType='${entry.rootType} rootType`);
      return EntryType.PLACEHOLDER;
    case 'GuestOsPlaceholder':
      return EntryType.PLACEHOLDER;
    case 'TrashEntry':
      return EntryType.TRASH;
    case 'OneDrivePlaceholder':
      return EntryType.PLACEHOLDER;
    default:
      console.warn(`Invalid entry.typeName='${entry.typeName}`);
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
    const fileData = currentState.allEntries[key]!;
    const metadata = {...fileData.metadata, ...entryMetadata.metadata};
    currentState.allEntries[key] = {
      ...fileData,
      metadata,
      key,
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

function findVolumeByType(volumes: VolumeMap, volumeType: VolumeType): Volume|
    null {
  return Object.values<Volume>(volumes).find(v => {
    // If the volume isn't resolved yet, we just ignore here.
    return v.rootKey && v.volumeType === volumeType;
  }) ??
      null;
}

/**
 * Returns the MyFiles entry and volume, the entry can either be a fake one
 * (EntryList) or a real one (VolumeEntry) depending on if the MyFiles volume is
 * mounted or not, and returns null if local files are disabled by policy.
 * Note: it will create a fake EntryList in the store if there's no
 * MyFiles entry in the store (e.g. no EntryList and no VolumeEntry), but local
 * files are enabled.
 */
export function getMyFiles(state: State):
    {myFilesVolume: null|Volume, myFilesEntry: null|VolumeEntry|EntryList} {
  const localFilesAllowed = state.preferences?.localUserFilesAllowed !== false;
  if (!isSkyvaultV2Enabled()) {
    // Return null for TT version.
    // For GA version we show local files in read-only mode, if present.
    if (!localFilesAllowed) {
      return {
        myFilesEntry: null,
        myFilesVolume: null,
      };
    }
  }

  const {volumes} = state;
  const myFilesVolume = findVolumeByType(volumes, VolumeType.DOWNLOADS);
  const myFilesVolumeEntry = myFilesVolume ?
      getEntry(state, myFilesVolume.rootKey!) as VolumeEntry | null :
      null;
  let myFilesEntryList =
      getEntry(state, myFilesEntryListKey) as EntryList | null;
  if (localFilesAllowed && !myFilesVolumeEntry && !myFilesEntryList) {
    myFilesEntryList =
        new EntryList(str('MY_FILES_ROOT_LABEL'), RootType.MY_FILES);
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
    ...allEntries[parentKey]!,
    children: newEntryKeys,
    // Update canExpand according to the children length.
    canExpand: newEntryKeys.length > 0,
  };

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
 */
export async function*
    readSubDirectoriesInternal(
        fileKey: FileKey, recursive: boolean = false,
        metricNameForTracking: string = ''): ActionsProducerGen {
  let state = getStore().getState();
  let fileData = getFileData(state, fileKey);
  if (!fileData) {
    debug(`failed to find FileData for ${fileKey}`);
    console.warn(`readSubDirectoriesInternal: failed to find FileData`);
    return;
  }
  if (!canHaveSubDirectories(fileData)) {
    return;
  }

  // Track time for reading sub directories if metric for tracking is passed.
  if (metricNameForTracking) {
    startInterval(metricNameForTracking);
  }

  const childEntriesToReadDeeper: Array<Entry|FilesAppEntry> = [];
  const entry = fileData.entry;
  if (fileKey === driveRootEntryListKey) {
    assert(entry);
    if (!isDriveRootEntryList(entry)) {
      console.warn(
          `ERROR: ${fileKey} didn't return a EntryList from the Store`);
      return;
    }
    for await (const action of readSubDirectoriesForDriveRootEntryList(entry)) {
      yield action;
      if (action) {
        const childEntries = action.payload.entries;
        childEntriesToReadDeeper.push(...childEntries);
        // After populating the children of Google Drive, if Google Drive is the
        // current directory, we need to navigate to its first child - My Drive.
        const state = getStore().getState();
        if (action.payload.entries.length > 0 &&
            state.currentDirectory?.key === entry.toURL()) {
          yield changeDirectory({
            toKey: childEntries[0]!.toURL(),
            status: PropStatus.STARTED,
          });
        }
      }
    }
  } else if (entry && isEntryScannable(entry)) {
    const childEntries = await readChildEntriesByFullScan(entry);
    // Only dispatch directories.
    const subDirectories =
        childEntries.filter(childEntry => childEntry.isDirectory);
    yield addChildEntries({parentKey: fileKey, entries: subDirectories});
    childEntriesToReadDeeper.push(...subDirectories);
    // Fetch metadata if the entry supports Drive specific share icon.
    state = getStore().getState();
    const parentFileData = getFileData(state, fileKey);
    if (parentFileData && isInsideDrive(parentFileData)) {
      const entriesNeedMetadata = subDirectories.filter(subDirectory => {
        const subDirFileData = getFileData(state, subDirectory.toURL());
        return subDirFileData &&
            shouldSupportDriveSpecificIcons(subDirFileData);
      });
      if (entriesNeedMetadata.length > 0) {
        window.fileManager.metadataModel.get(entriesNeedMetadata, [
          ...LIST_CONTAINER_METADATA_PREFETCH_PROPERTY_NAMES,
          ...DLP_METADATA_PREFETCH_PROPERTY_NAMES,
        ]);
      }
    }
  } else {
    // TODO(b/327534506): Add support for Materialize Views.
    console.warn(`readSubDirectories not supported for ${fileKey}`);
  }

  // Track time for reading sub directories if metric for tracking is passed.
  if (metricNameForTracking) {
    recordInterval(metricNameForTracking);
  }

  // Read sub directories for children when recursive is true.
  if (!recursive) {
    return;
  }

  // Refresh the fileData from the store because it might have changed during
  // the async operations above.
  state = getStore().getState();
  fileData = getFileData(state, fileKey);

  // We only read deeper if the parent entry is expanded in the tree.
  if (!fileData?.expanded) {
    return;
  }

  // Recursive scan.
  for (const childEntry of childEntriesToReadDeeper) {
    const state = getStore().getState();
    const childFileData = getFileData(state, childEntry.toURL());
    if (!childFileData) {
      continue;
    }

    if (childFileData.expanded) {
      // If child item is expanded, we need to do a full scan for it.
      for await (const action of readSubDirectories(
          childEntry.toURL(), /* recursive= */ true)) {
        yield action;
      }
    } else if (childFileData?.canExpand) {
      // If we already know the child item can be expanded, no partial scan is
      // required.
      continue;
    } else {
      // If the child item is not expanded, we do a partial scan to check if
      // it has children or not (so we know if we need to show expand icon
      // or not).
      for await (const action of readSubDirectoriesToCheckDirectoryChildren(
          childEntry.toURL())) {
        yield action;
      }
    }
  }
}

/**
 * When there are multiple `readSubDirectories` actions with the same key being
 * dispatched at the same time, we only keep the latest one.
 */
export const readSubDirectories = keyedKeepLatest(
    readSubDirectoriesInternal,
    (fileKey, recursive?, _metricNameForTracking?) =>
        `${fileKey}${recursive ? '-recursive' : ''}`);

/**
 * Read entries for Drive root entry list (aka "Google Drive"), there are some
 * differences compared to the `readSubDirectoriesForDirectoryEntry()`:
 * * We don't need to call readEntries() to get its child entries. Instead, all
 * its children are from its entry.getUiChildren().
 * * For fake entries children (e.g. Shared with me and Offline), we only show
 * them based on the dialog type.
 * * For certain children (e.g. team drives and computers grand root), we only
 * show them when there's at least one child entry inside. So we need to read
 * their children (grand children of drive fake root) first before we can decide
 * if we need to show them or not.
 */
async function*
    readSubDirectoriesForDriveRootEntryList(entry: EntryList):
        ActionsProducerGen {
  const metricNameMap = {
    [SHARED_DRIVES_DIRECTORY_PATH]: 'TeamDrivesCount',
    [COMPUTERS_DIRECTORY_PATH]: 'ComputerCount',
  };

  const driveChildren = entry.getUiChildren();
  /**
   * Store the filtered children, for fake entries or grand roots we might need
   * to hide them based on curtain conditions.
   */
  const filteredChildren: Array<Entry|FilesAppEntry> = [];

  for (const childEntry of driveChildren) {
    // For fake entries ("Shared with me" and "Offline").
    if (isFakeEntryInDrive(childEntry)) {
      filteredChildren.push(childEntry);
      continue;
    }
    // For non grand roots (also not fake entries), we put them in the children
    // directly and dispatch an action to read the it later.
    if (!isGrandRootEntryInDrive(childEntry)) {
      filteredChildren.push(childEntry);
      continue;
    }
    // For grand roots ("Shared drives" and "Computers") inside Drive, we only
    // show them when there's at least one child entries inside.
    const grandChildEntries = await readChildEntriesByFullScan(childEntry);
    recordSmallCount(
        metricNameMap[childEntry.fullPath]!, grandChildEntries.length);
    if (grandChildEntries.length > 0) {
      filteredChildren.push(childEntry);
    }
  }
  yield addChildEntries({parentKey: entry.toURL(), entries: filteredChildren});
}

/**
 * Read a given directory entry to get all child entries.
 *
 * @param entry The parent directory entry to read.
 */
async function readChildEntriesByFullScan(
    entry: DirectoryEntry|
    FilesAppDirEntry): Promise<Array<Entry|FilesAppEntry>> {
  const childEntries = [];
  for await (const partialEntries of readEntries(entry)) {
    childEntries.push(...partialEntries);
  }
  return sortEntries(entry, childEntries);
}

/**
 * Read a given directory entry to check if it has directory child entries or
 * not. It won't do full scanning, the scan stops immediately after finding a
 * child directory entry.
 *
 * @param entry The parent directory entry to read.
 */
async function checkDirectoryChildByPartialScan(
    entry: DirectoryEntry|FilesAppDirEntry): Promise<boolean> {
  const {directoryModel} = window.fileManager;
  const fileFilter = directoryModel.getFileFilter();
  const isDirectoryChild = (childEntry: Entry|FilesAppEntry): boolean =>
      childEntry.isDirectory && fileFilter.filter(childEntry);
  for await (const partialEntries of readEntries(entry)) {
    if (partialEntries.some(isDirectoryChild)) {
      return true;
    }
  }
  return false;
}

/**
 * Read sub directories for a given entry to check if it has directory children
 * or not.
 */
async function*
    readSubDirectoriesToCheckDirectoryChildrenInternal(fileKey: FileKey|null):
        ActionsProducerGen {
  if (!fileKey) {
    return;
  }

  const state = getStore().getState();
  const fileData = getFileData(state, fileKey);
  if (!fileData) {
    console.warn(`No file data: ${fileKey}`);
    return;
  }
  if (!canHaveSubDirectories(fileData)) {
    return;
  }

  // Do nothing because we already know it has children.
  if (fileData.children.length > 0 || fileData.canExpand) {
    return;
  }

  const {directoryModel} = window.fileManager;
  const fileFilter = directoryModel.getFileFilter();
  const isDirectoryChild = (childEntry: Entry|FilesAppEntry): boolean =>
      childEntry.isDirectory && fileFilter.filter(childEntry);
  // The entry has UIChildren but has no FileData.children, we know it can be
  // expanded.
  if (supportsUiChildren(fileData) && fileData.entry) {
    const entry = fileData.entry;
    assert(isEntrySupportUiChildren(entry));
    const uiChildrenDirectories =
        entry.getUiChildren().filter(isDirectoryChild);
    if (uiChildrenDirectories.length > 0) {
      yield updateFileData({key: fileKey, partialFileData: {canExpand: true}});
    }
  }

  // TODO(lucmult): Add support to Materialize Views.
  const entry = fileData.entry;
  if (!entry) {
    console.warn(`No entry: ${fileKey}`);
    return;
  }
  if (isDirectoryEntry(entry)) {
    const hasDirectoryChild = await checkDirectoryChildByPartialScan(entry);
    if (hasDirectoryChild) {
      yield updateFileData({key: fileKey, partialFileData: {canExpand: true}});
    }
  }
}

/**
 * When there are multiple `readSubDirectoriesToCheckDirectoryChildren`
 * actions with the same key being dispatched at the same time, we only keep
 * the latest one.
 */
export const readSubDirectoriesToCheckDirectoryChildren = keyedKeepLatest(
    readSubDirectoriesToCheckDirectoryChildrenInternal,
    (fileKey) => fileKey ?? '');

/**
 * Read child entries for the newly renamed directory entry.
 * We need to read its parent's children first before reading its own
 * children, because the newly renamed entry might not be in the store yet
 * after renaming.
 */
export async function*
    readSubDirectoriesForRenamedEntry(newEntry: Entry|FilesAppEntry):
        ActionsProducerGen {
  const parentDirectory = await getParentEntry(newEntry);
  // Read the children of the parent first to make sure the newly added entry
  // appears in the store.
  for await (const action of readSubDirectories(parentDirectory.toURL())) {
    yield action;
  }
  // Read the children of the newly renamed entry.
  for await (const action of readSubDirectories(
      newEntry.toURL(), /* recursive= */ true)) {
    yield action;
  }
}

/**
 * Traverse each entry in the `pathEntryKeys`: if the entry doesn't exist in
 * the store, read sub directories for its parent. After all entries exist in
 * the store, expand all parent entries.
 *
 * @param pathEntryKeys An array of FileKey starts from ancestor to child,
 *     e.g. [A, B, C] A is the parent entry of B, B is the parent entry of C.
 */
export async function*
    traverseAndExpandPathEntriesInternal(pathEntryKeys: FileKey[]):
        ActionsProducerGen {
  if (pathEntryKeys.length === 0) {
    return;
  }
  const childEntryKey = pathEntryKeys[pathEntryKeys.length - 1]!;
  const state = getStore().getState();
  const childEntryFileData = getFileData(state, childEntryKey);
  if (!childEntryFileData) {
    console.warn(`Can not find the child entry: ${childEntryKey}`);
    return;
  }
  const volume = getVolume(state, childEntryFileData);
  if (!volume) {
    console.warn(
        `Can not find the volume root for the child entry: ${childEntryKey}`);
    return;
  }
  const volumeEntry = getEntry(state, volume.rootKey!);
  if (!volumeEntry) {
    console.warn(`Can not find the volume root entry: ${volume.rootKey}`);
    return;
  }

  for (let i = 1; i < pathEntryKeys.length; i++) {
    // We need to getStore() for each loop because the below `yield action`
    // will add new entries to the store.
    const state = getStore().getState();
    const currentEntryKey = pathEntryKeys[i]!;
    const parentEntryKey = pathEntryKeys[i - 1]!;
    const parentFileData = getFileData(state, parentEntryKey)!;
    const fileData = getFileData(state, currentEntryKey);
    // Read sub directories if the child entry doesn't exist or it's not in
    // parent entry's children.
    if (!fileData || !parentFileData.children.includes(currentEntryKey)) {
      let foundCurrentEntry = false;
      for await (const action of readSubDirectories(parentEntryKey)) {
        yield action;
        const childEntries: Array<Entry|FilesAppEntry> =
            action?.payload.entries || [];
        foundCurrentEntry =
            !!childEntries.find(entry => entry.toURL() === currentEntryKey);
        if (foundCurrentEntry) {
          break;
        }
      }
      if (!foundCurrentEntry) {
        console.warn(`Failed to find entry "${
            currentEntryKey}" from its parent "${parentEntryKey}"`);
        return;
      }
    }
  }
  // Now all entries on `pathEntryKeys` are found, we can expand all of them
  // now. Note: if any entry on the path can't be found, we don't expand
  // anything because we don't want to expand half-way, e.g. if `pathEntryKeys
  // = [entryA, entryB, entryC]` but somehow entryB doesn't exist, we don't
  // want to expand `entryA`.
  for (let i = 0; i < pathEntryKeys.length - 1; i++) {
    yield updateFileData(
        {key: pathEntryKeys[i]!, partialFileData: {expanded: true}});
  }
}

/**
 * `traverseAndExpandPathEntries` is mainly used to traverse and expand the
 * `pathComponent` for current directory, if concurrent requests happen (e.g.
 * current directory changes too quickly while we are still resolving the
 * previous one), we just ditch the previous request and only keep the latest.
 */
export const traverseAndExpandPathEntries =
    keepLatest(traverseAndExpandPathEntriesInternal);

/** Create action to update FileData for a given entry. */
export const updateFileData =
    slice.addReducer('update-file-data', updateFileDataReducer);

function updateFileDataReducer(currentState: State, payload: {
  key: FileKey,
  partialFileData: Partial<FileData>,
}): State {
  const {key, partialFileData} = payload;
  const fileData = getFileData(currentState, key);
  if (!fileData) {
    return currentState;
  }

  currentState.allEntries[key] = {
    ...fileData,
    ...partialFileData,
    key,
  };
  return {...currentState};
}
