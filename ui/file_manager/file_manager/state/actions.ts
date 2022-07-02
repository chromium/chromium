// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PropStatus} from '../externs/ts/state.js';
import {BaseAction} from '../lib/base_store.js';

import {FileKey} from './file_key.js';

/**
 * Base class for all Actions in Files app.
 *
 * It enforces the type/enum for the `type` attribute.
 */
export interface Action extends BaseAction {
  type: Actions;
}

/** Enum to identify every Action in Files app. */
export const enum Actions {
  CHANGE_DIRECTORY = 'change-directory',
}

/** Action to request to change the Current Directory. */
export interface ChangeDirectoryAction extends Action {
  newDirectory?: Entry;
  key: FileKey;
  status: PropStatus;
}

/** Factory for the ChangeDirectoryAction. */
export function changeDirectory(
    {to, toKey, status}: {to?: Entry, toKey: FileKey, status?: PropStatus}):
    ChangeDirectoryAction {
  return {
    type: Actions.CHANGE_DIRECTORY,
    newDirectory: to,
    key: toKey ? toKey : to!.toURL(),
    status: status ? status : PropStatus.STARTED,
  };
}
