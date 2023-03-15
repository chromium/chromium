// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FileKey} from '../../externs/ts/state.js';
import {BaseAction} from '../../lib/base_store.js';
import {ActionType} from '../actions.js';

/**
 * Actions for Folder shortcuts.
 *
 * Folder shortcuts represent a shortcut for the folders inside Drive.
 */

/** Action to refresh all folder shortcuts in the store. */
export interface RefreshFolderShortcutAction extends BaseAction {
  type: ActionType.REFRESH_FOLDER_SHORTCUT;
  payload: {
    /** All folder shortcuts should be provided here. */
    entries: DirectoryEntry[],
  };
}

/** Action to add single folder shortcut in the store. */
export interface AddFolderShortcutAction extends BaseAction {
  type: ActionType.ADD_FOLDER_SHORTCUT;
  payload: {
    entry: DirectoryEntry,
  };
}

/** Action to remove single folder shortcut from the store. */
export interface RemoveFolderShortcutAction extends BaseAction {
  type: ActionType.REMOVE_FOLDER_SHORTCUT;
  payload: {
    key: FileKey,
  };
}

/**
 * Action factory to refresh all folder shortcuts in the store, all folder
 * shortcuts needs to be provided here because it will replace all existing ones
 * in the store.
 */
export function refreshFolderShortcut(
    payload: RefreshFolderShortcutAction['payload']):
    RefreshFolderShortcutAction {
  return {
    type: ActionType.REFRESH_FOLDER_SHORTCUT,
    payload,
  };
}

/** Action factory to add single folder shortcut in the store. */
export function addFolderShortcut(payload: AddFolderShortcutAction['payload']):
    AddFolderShortcutAction {
  return {
    type: ActionType.ADD_FOLDER_SHORTCUT,
    payload,
  };
}

/** Action factory to remove single folder shortcut in the store. */
export function removeFolderShortcut(
    payload: RemoveFolderShortcutAction['payload']):
    RemoveFolderShortcutAction {
  return {
    type: ActionType.REMOVE_FOLDER_SHORTCUT,
    payload,
  };
}
