// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BaseAction} from '../../lib/base_store.js';
import {ActionType} from '../actions.js';
import {FileKey} from '../file_key.js';


/** Action to refresh all navigation roots. */
export interface RefreshNavigationRootsAction extends BaseAction {
  type: ActionType.REFRESH_NAVIGATION_ROOTS;
  payload: {};
}

/** Action to update the navigation data in FileData for a given entry. */
export interface UpdateNavigationEntryAction extends BaseAction {
  type: ActionType.UPDATE_NAVIGATION_ENTRY;
  payload: {
    key: FileKey,
    expanded: boolean,
  };
}

/**
 * Action factory to refresh all navigation roots. This will clear all existing
 * navigation roots in the store and regenerate them with the current state
 * data.
 */
export function refreshNavigationRoots(): RefreshNavigationRootsAction {
  return {
    type: ActionType.REFRESH_NAVIGATION_ROOTS,
    payload: {},
  };
}

/**
 * Action factory to update the navigation data in FileData for a given entry.
 */
export function updateNavigationEntry(
    payload: UpdateNavigationEntryAction['payload']):
    UpdateNavigationEntryAction {
  return {
    type: ActionType.UPDATE_NAVIGATION_ENTRY,
    payload,
  };
}
