// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FileType} from '../../common/js/file_type.js';
import {util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';
import {EntryType, State} from '../../externs/ts/state.js';
import {Action, ActionType} from '../actions.js';
import {ClearStaleCachedEntriesAction} from '../actions/all_entries.js';
import {getStore} from '../store.js';

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
      metadataModel.getCache([entry as FileEntry], [])[0] :
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

  if (action.type === ActionType.CHANGE_SELECTION) {
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
