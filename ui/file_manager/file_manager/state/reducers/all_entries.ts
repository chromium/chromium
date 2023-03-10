// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FileType} from '../../common/js/file_type.js';
import {util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';
import {CurrentDirectory, EntryType, State} from '../../externs/ts/state.js';
import {constants} from '../../foreground/js/constants.js';
import {Action, ActionType} from '../actions.js';
import {ClearStaleCachedEntriesAction, UpdateMetadataAction} from '../actions/all_entries.js';
import {getStore} from '../store.js';

import {hasDlpDisabledFiles} from './current_directory.js';

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
  store.dispatch({type: ActionType.CLEAR_STALE_CACHED_ENTRIES});
  clearCachedEntriesRequestId = 0;
}

/**
 * Scans the current state for entries still in use to be able to remove the
 * stale entries from the `allEntries`.
 */
export function clearCachedEntries(
    state: State, _action: ClearStaleCachedEntriesAction): State {
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

  for (const key of Object.keys(entries)) {
    if (entriesToKeep.has(key)) {
      continue;
    }

    delete entries[key];
  }

  return state;
}

const prefetchPropertyNames = Array.from(new Set([
  ...constants.LIST_CONTAINER_METADATA_PREFETCH_PROPERTY_NAMES,
  ...constants.ACTIONS_MODEL_METADATA_PREFETCH_PROPERTY_NAMES,
  ...constants.FILE_SELECTION_METADATA_PREFETCH_PROPERTY_NAMES,
  ...constants.DLP_METADATA_PREFETCH_PROPERTY_NAMES,
]));

/**
 * Converts the entry to the Store representation of an Entry and appends the
 * entry to the Store.
 */
function appendEntry(state: State, entry: Entry|FilesAppEntry) {
  const allEntries = state.allEntries || {};
  const key = entry.toURL();
  const volumeManager = window.fileManager?.volumeManager;
  const metadataModel = window.fileManager?.metadataModel;
  const volumeInfo = volumeManager?.getVolumeInfo(entry);
  const locationInfo = volumeManager?.getLocationInfo(entry);
  const label = locationInfo ? util.getEntryLabel(locationInfo, entry) : '';

  const volumeType = volumeInfo?.volumeType || null;

  const entryData = allEntries[key] || {};
  const metadata = metadataModel ?
      metadataModel.getCache([entry as FileEntry], prefetchPropertyNames)[0] :
      undefined;

  allEntries[key] = {
    ...entryData,
    entry,
    iconName:
        FileType.getIcon(entry as Entry, undefined, locationInfo?.rootType),
    type: getEntryType(entry),
    isDirectory: entry.isDirectory,
    label,
    volumeType,
    metadata,
  };

  state.allEntries = allEntries;
}

/** Caches the Action's entry in the `allEntries` attribute. */
export function cacheEntries(currentState: State, action: Action): State {
  // Schedule to clear the cached entries from the state.
  scheduleClearCachedEntries();

  if (action.type === ActionType.CHANGE_SELECTION ||
      action.type === ActionType.UPDATE_DIRECTORY_CONTENT) {
    for (const entry of action.payload.entries) {
      appendEntry(currentState, entry);
    }
  }
  if (action.type === ActionType.CHANGE_DIRECTORY) {
    const entry = action.payload.newDirectory;
    if (!entry) {
      // Nothing to cache, just continue.
      return currentState;
    }

    appendEntry(currentState, entry);
  }
  if (action.type === ActionType.UPDATE_METADATA) {
    for (const entryMetadata of action.payload.metadata) {
      appendEntry(currentState, entryMetadata.entry);
    }
  }

  return currentState;
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
    default:
      console.warn(`Invalid entry.type_name='${entry.type_name}`);
      return EntryType.FS_API;
  }
}

/**
 * Reducer that updates the metadata of the entries and returns the new state.
 */
export function updateMetadata(
    currentState: State, action: UpdateMetadataAction): State {
  for (const entryMetadata of action.payload.metadata) {
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
