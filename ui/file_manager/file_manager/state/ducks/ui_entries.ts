// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isSameEntry, isVolumeEntry} from '../../common/js/entry_utils.js';
import type {EntryList, FakeEntry} from '../../common/js/files_app_entry_types.js';
import {RootType} from '../../common/js/volume_manager_types.js';
import type {ActionsProducerGen} from '../../lib/actions_producer.js';
import {Slice} from '../../lib/base_store.js';
import type {FileKey, State} from '../../state/state.js';
import {cacheEntries, getMyFiles, readSubDirectories} from '../ducks/all_entries.js';
import {getEntry, getStore} from '../store.js';

/**
 * @fileoverview UI entries slice of the store.
 *
 * UI entries represents entries shown on UI only (aka FakeEntry, e.g.
 * Recents/Trash/Google Drive wrapper), they don't have a real entry backup in
 * the file system.
 */


const slice = new Slice<State, State['uiEntries']>('uiEntries');
export {slice as uiEntriesSlice};

const uiEntryRootTypesInMyFiles = new Set([
  RootType.ANDROID_FILES,
  RootType.CROSTINI,
  RootType.GUEST_OS,
]);


/** Create action to add an UI entry to the store. */
export const addUiEntryInternal = slice.addReducer('add', addUiEntryReducer);

function addUiEntryReducer(currentState: State, payload: {
  entry: FakeEntry|EntryList,
}): State {
  // Cache entries, so the reducers can use any entry from `allEntries`.
  cacheEntries(currentState, [payload.entry]);

  const {entry} = payload;
  const key = entry.toURL();
  const uiEntries = [...currentState.uiEntries, key];

  return {
    ...currentState,
    uiEntries,
  };
}

/**
 * Add UI entry to the store and re-scan MyFiles if the newly added UI entry is
 * under MyFiles.
 */
export async function*
    addUiEntry(entry: FakeEntry|EntryList): ActionsProducerGen {
  const state = getStore().getState();
  const exists = state.uiEntries.find(key => key === entry.toURL());
  if (exists) {
    return;
  }

  // If the UI entry to be added is under MyFiles, we also need to update
  // MyFiles's UI children.
  let isVolumeEntryInMyFiles = false;
  if (entry.rootType && uiEntryRootTypesInMyFiles.has(entry.rootType)) {
    const {myFilesEntry} = getMyFiles(state);
    if (!myFilesEntry) {
      // TODO(aidazolic): Add separately.
      return;
    }
    const children = myFilesEntry.getUiChildren();
    // Check if the the ui entry already has a corresponding volume entry.
    isVolumeEntryInMyFiles = !!children.find(
        childEntry =>
            isVolumeEntry(childEntry) && childEntry.name === entry.name);
    const isUiEntryInMyFiles =
        !!children.find(childEntry => isSameEntry(childEntry, entry));
    // We only add the UI entry here if:
    // 1. it does not exist in MyFiles entry's UI children.
    // 2. its corresponding volume (which ui entry is a placeholder for) does
    // not exist in MyFiles entry's UI children.
    const shouldAddUiEntry = !isUiEntryInMyFiles && !isVolumeEntryInMyFiles;
    if (shouldAddUiEntry) {
      myFilesEntry.addEntry(entry);
      yield addUiEntryInternal({entry});
      // Get MyFiles again from the latest state after yield because yield pause
      // the execution of this function and between the pause MyFiles might
      // change from EntryList to Volume (e.g. MyFiles volume mounts during the
      // pause).
      const {myFilesEntry: updatedMyFiles} = getMyFiles(getStore().getState());
      // Trigger a re-scan for MyFiles to make FileData.children in the store
      // has this newly added children.
      if (!updatedMyFiles) {
        return;
      }
      for await (const action of readSubDirectories(updatedMyFiles.toURL())) {
        yield action;
      }
      return;
    }
  }
  if (!isVolumeEntryInMyFiles) {
    yield addUiEntryInternal({entry});
  }
}

/** Create action to remove an UI entry from the store. */
const removeUiEntryInternal = slice.addReducer('remove', removeUiEntryReducer);

function removeUiEntryReducer(currentState: State, payload: {
  key: FileKey,
}): State {
  const {key} = payload;
  const uiEntries = currentState.uiEntries.filter(k => k !== key);

  return {
    ...currentState,
    uiEntries,
  };
}

/**
 * Remove UI entry from the store and re-scan MyFiles if the removed UI entry is
 * under MyFiles.
 */
export async function* removeUiEntry(key: FileKey): ActionsProducerGen {
  const state = getStore().getState();
  const exists = state.uiEntries.find(uiEntryKey => uiEntryKey === key);
  if (!exists) {
    return;
  }

  yield removeUiEntryInternal({key});

  const entry = getEntry(state, key) as FakeEntry | EntryList | null;
  // We also need to remove it from the children of MyFiles if it's existed
  // there.
  if (entry?.rootType && uiEntryRootTypesInMyFiles.has(entry.rootType)) {
    // Get MyFiles from the latest state after yield because yield pause
    // the execution of this function and between the pause MyFiles might
    // change.
    const {myFilesEntry} = getMyFiles(getStore().getState());
    if (!myFilesEntry) {
      return;
    }
    const children = myFilesEntry.getUiChildren();
    const isUiEntryInMyFiles =
        !!children.find(childEntry => isSameEntry(childEntry, entry));
    if (isUiEntryInMyFiles) {
      myFilesEntry.removeChildEntry(entry);
      // Trigger a re-scan for MyFiles to make FileData.children in the store
      // removes this children.
      for await (const action of readSubDirectories(myFilesEntry.toURL())) {
        yield action;
      }
    }
  }
}
