// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FilesAppDirEntry, FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';
import {FileTasks, PropStatus} from '../../externs/ts/state.js';
import {BaseAction} from '../../lib/base_store.js';
import {ActionType} from '../actions.js';
import {FileKey} from '../file_key.js';

/** Action to request to change the Current Directory. */
export interface ChangeDirectoryAction extends BaseAction {
  type: ActionType.CHANGE_DIRECTORY;
  payload: {
    newDirectory?: DirectoryEntry|FilesAppDirEntry,
                key: FileKey,
                status: PropStatus,
  };
}

/** Action to update the currently selected files/folders. */
export interface ChangeSelectionAction extends BaseAction {
  type: ActionType.CHANGE_SELECTION;
  payload: {
    selectedKeys: FileKey[],
    entries: Array<Entry|FilesAppEntry>,
  };
}

/** Action to update the FileTasks in the selection. */
export interface ChangeFileTasksAction extends BaseAction {
  type: ActionType.CHANGE_FILE_TASKS;
  payload: FileTasks;
}

/** Action to update the current directory's content. */
export interface UpdateDirectoryContentAction extends BaseAction {
  type: ActionType.UPDATE_DIRECTORY_CONTENT;
  payload: {
    entries: Array<Entry|FilesAppEntry>,
  };
}

/** Factory for the ChangeDirectoryAction. */
export function changeDirectory({to, toKey, status}: {
  to?: DirectoryEntry|FilesAppDirEntry, toKey: FileKey,
  status?: PropStatus,
}): ChangeDirectoryAction {
  return {
    type: ActionType.CHANGE_DIRECTORY,
    payload: {
      newDirectory: to,
      key: toKey ? toKey : to!.toURL(),
      status: status ? status : PropStatus.STARTED,
    },
  };
}

/** Factory for the ChangeSelectionAction. */
export function updateSelection(payload: ChangeSelectionAction['payload']):
    ChangeSelectionAction {
  return {
    type: ActionType.CHANGE_SELECTION,
    payload,
  };
}

/** Factory for the ChangeFileTasksAction. */
export function updateFileTasks(payload: FileTasks): ChangeFileTasksAction {
  return {
    type: ActionType.CHANGE_FILE_TASKS,
    payload,
  };
}

/** Factory for the UpdateDirectoryContentAction. */
export function updateDirectoryContent(
    payload: UpdateDirectoryContentAction['payload']):
    UpdateDirectoryContentAction {
  return {
    type: ActionType.UPDATE_DIRECTORY_CONTENT,
    payload,
  };
}
