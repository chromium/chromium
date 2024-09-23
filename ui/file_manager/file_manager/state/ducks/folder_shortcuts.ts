// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {comparePath} from '../../common/js/entry_utils.js';
import type {FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import {Slice} from '../../lib/base_store.js';
import type {FileKey, State} from '../../state/state.js';
import {getEntry} from '../store.js';

import {cacheEntries} from './all_entries.js';

const slice = new Slice<State, State['folderShortcuts']>('folderShortcuts');
export {slice as folderShortcutsSlice};

/** Create action to refresh all folder shortcuts with provided ones. */
export const refreshFolderShortcut =
    slice.addReducer('refresh', refreshFolderShortcutReducer);

function refreshFolderShortcutReducer(currentState: State, payload: {
  entries: Array<Entry|FilesAppEntry>,
}): State {
  // Cache entries, so the reducers can use any entry from `allEntries`.
  cacheEntries(currentState, payload.entries);

  return {
    ...currentState,
    folderShortcuts: payload.entries.map(entry => entry.toURL()),
  };
}

/** Create action to add a folder shortcut. */
export const addFolderShortcut =
    slice.addReducer('add', addFolderShortcutReducer);

function addFolderShortcutReducer(currentState: State, payload: {
  entry: Entry|FilesAppEntry,
}): State {
  // Cache entries, so the reducers can use any entry from `allEntries`.
  cacheEntries(currentState, [payload.entry]);

  const {entry} = payload;
  const key = entry.toURL();
  const {folderShortcuts} = currentState;

  for (let i = 0; i < folderShortcuts.length; i++) {
    // Do nothing if the key is already existed.
    if (key === folderShortcuts[i]) {
      return currentState;
    }

    const shortcutEntry = getEntry(currentState, folderShortcuts[i]!);
    // The folder shortcut array is sorted, the new item will be added just
    // before the first larger item.
    if (comparePath(shortcutEntry!, entry) > 0) {
      return {
        ...currentState,
        folderShortcuts: [
          ...folderShortcuts.slice(0, i),
          key,
          ...folderShortcuts.slice(i),
        ],
      };
    }
  }

  // If for loop is not returned, the key is not added yet, add it at the last.
  return {
    ...currentState,
    folderShortcuts: folderShortcuts.concat(key),
  };
}

/** Create action to remove a folder shortcut. */
export const removeFolderShortcut =
    slice.addReducer('remove', removeFolderShortcutReducer);

function removeFolderShortcutReducer(currentState: State, payload: {
  key: FileKey,
}): State {
  const {key} = payload;
  const {folderShortcuts} = currentState;
  const isExisted = folderShortcuts.find(k => k === key);
  // Do nothing if the key is not existed.
  if (!isExisted) {
    return currentState;
  }

  return {
    ...currentState,
    folderShortcuts: folderShortcuts.filter(k => k !== key),
  };
}
