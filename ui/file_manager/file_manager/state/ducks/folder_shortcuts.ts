// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {util} from '../../common/js/util.js';
import {FileKey, State} from '../../externs/ts/state.js';
import {addReducer, BaseAction, Reducer, ReducersMap} from '../../lib/base_store.js';
import {Action, ActionType} from '../actions.js';
import {getEntry} from '../store.js';

/**
 * Actions and reducers for folder shortcuts.
 *
 * Folder shortcuts represent a shortcut for the folders inside Drive.
 */

/** Map of actions to reducers for the folder shortcuts slice. */
export const folderShortcutsReducersMap: ReducersMap<State, Action> = new Map();

/** Action to refresh all folder shortcuts in the store. */
export interface RefreshFolderShortcutAction extends BaseAction {
  type: ActionType.REFRESH_FOLDER_SHORTCUT;
  payload: {
    /** All folder shortcuts should be provided here. */
    entries: DirectoryEntry[],
  };
}

const refreshFolderShortcutReducer =
    (currentState: State, payload: RefreshFolderShortcutAction['payload']):
        State => ({
          ...currentState,
          folderShortcuts: payload.entries.map(entry => entry.toURL()),
        });

/**
 * Action factory to refresh all folder shortcuts in the store, all folder
 * shortcuts needs to be provided here because it will replace all existing ones
 * in the store.
 */
export const refreshFolderShortcut = addReducer(
    ActionType.REFRESH_FOLDER_SHORTCUT,
    refreshFolderShortcutReducer as Reducer<State, Action>,
    folderShortcutsReducersMap);


/** Action to add single folder shortcut in the store. */
export interface AddFolderShortcutAction extends BaseAction {
  type: ActionType.ADD_FOLDER_SHORTCUT;
  payload: {
    entry: DirectoryEntry,
  };
}

function addFolderShortcutReducer(
    currentState: State, payload: AddFolderShortcutAction['payload']): State {
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

/** Action factory to add single folder shortcut in the store. */
export const addFolderShortcut = addReducer(
    ActionType.ADD_FOLDER_SHORTCUT,
    addFolderShortcutReducer as Reducer<State, Action>,
    folderShortcutsReducersMap);


/** Action to remove single folder shortcut from the store. */
export interface RemoveFolderShortcutAction extends BaseAction {
  type: ActionType.REMOVE_FOLDER_SHORTCUT;
  payload: {
    key: FileKey,
  };
}

function removeFolderShortcutReducer(
    currentState: State,
    payload: RemoveFolderShortcutAction['payload']): State {
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

/** Action factory to remove single folder shortcut in the store. */
export const removeFolderShortcut = addReducer(
    ActionType.REMOVE_FOLDER_SHORTCUT,
    removeFolderShortcutReducer as Reducer<State, Action>,
    folderShortcutsReducersMap);
