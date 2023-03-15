// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {util} from '../../common/js/util.js';
import {State} from '../../externs/ts/state.js';
import {AddFolderShortcutAction, RefreshFolderShortcutAction, RemoveFolderShortcutAction} from '../actions/folder_shortcuts.js';
import {getEntry} from '../store.js';

export function refreshFolderShortcut(
    currentState: State, action: RefreshFolderShortcutAction): State {
  const {entries} = action.payload;

  return {
    ...currentState,
    folderShortcuts: entries.map(entry => entry.toURL()),
  };
}

export function addFolderShortcut(
    currentState: State, action: AddFolderShortcutAction): State {
  const {entry} = action.payload;
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
    if (util.comparePath(shortcutEntry!, entry) > 0) {
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

export function removeFolderShortcut(
    currentState: State, action: RemoveFolderShortcutAction): State {
  const {key} = action.payload;
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
