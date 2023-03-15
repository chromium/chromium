// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';
import {MetadataItem} from '../../foreground/js/metadata/metadata_item.js';
import {BaseAction} from '../../lib/base_store.js';
import {ActionType} from '../actions.js';
import {FileKey} from '../file_key.js';

/**
 * Processes the allEntries and removes any entry that isn't in use any more.
 */
export interface ClearStaleCachedEntriesAction extends BaseAction {
  type: ActionType.CLEAR_STALE_CACHED_ENTRIES;
  payload?: undefined;
}

export interface EntryMetadata {
  entry: Entry|FilesAppEntry;
  metadata: MetadataItem;
}

/**
 * Action to update the allEntries metadata.
 */
export interface UpdateMetadataAction extends BaseAction {
  type: ActionType.UPDATE_METADATA;
  payload: {
    metadata: EntryMetadata[],
  };
}

/** Action to add child entries to a given parent entry. */
export interface AddChildEntriesAction extends BaseAction {
  type: ActionType.ADD_CHILD_ENTRIES;
  payload: {
    parentKey: FileKey,
    entries: Array<Entry|FilesAppEntry>,
  };
}


/** Factory for the UpdateMetadataAction. */
export function updateMetadata(payload: UpdateMetadataAction['payload']):
    UpdateMetadataAction {
  return {
    type: ActionType.UPDATE_METADATA,
    payload,
  };
}

/** Action factory to add child entries to a given parent entry. */
export function addChildEntries(payload: AddChildEntriesAction['payload']):
    AddChildEntriesAction {
  return {
    type: ActionType.ADD_CHILD_ENTRIES,
    payload,
  };
}
