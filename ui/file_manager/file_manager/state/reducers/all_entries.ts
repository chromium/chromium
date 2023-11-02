// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {util} from '../../common/js/util.js';
import {FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';
import {State} from '../../externs/ts/state.js';
import {Action, ActionType, ChangeDirectoryAction, ClearStaleCachedEntriesAction} from '../actions.js';
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

  for (const key of Object.keys(entries)) {
    if (entriesToKeep.has(key)) {
      continue;
    }

    delete entries[key];
  }

  return state;
}

/**
 * @param action Action being currently processed.
 * @returns The entry instance from the allEntries, if it exists in the cache.
 */
function getEntry(state: State, action: ChangeDirectoryAction): Entry|
    FilesAppEntry|null {
  const {newDirectory, key} = action.payload;
  if (newDirectory) {
    return newDirectory;
  }

  const entry = state.allEntries[key!] ? state.allEntries[key!]!.entry : null;
  if (!entry) {
    return null;
  }
  return entry;
}

/** Caches the Action's entry in the `allEntries` attribute. */
export function cacheEntries(currentState: State, action: Action): State {
  // Schedule to clear the cached entries from the state.
  scheduleClearCachedEntries();

  if (action.type === ActionType.CHANGE_DIRECTORY) {
    const {key} = action.payload;
    const allEntries = currentState.allEntries || {};

    const entry = getEntry(currentState, (action as ChangeDirectoryAction));
    if (!entry) {
      // Nothing to cache, just continue.
      return currentState;
    }

    const volumeManager = window.fileManager?.volumeManager;
    const volumeInfo = volumeManager?.getVolumeInfo(entry);
    const locationInfo = volumeManager?.getLocationInfo(entry);
    const label = locationInfo ? util.getEntryLabel(locationInfo, entry) : '';

    const volumeType = volumeInfo?.volumeType || null;

    const entryData = allEntries[key] || {};
    allEntries[key] = Object.assign(entryData, {
      entry,
      label,
      volumeType,
    });

    currentState.allEntries = allEntries;
  }

  return currentState;
}
