// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isVolumeEntry, sortEntries} from '../../common/js/entry_utils.js';
import {FakeEntryImpl} from '../../common/js/files_app_entry_types.js';
import {util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {FileKey, State} from '../../externs/ts/state.js';
import {Slice} from '../../lib/base_store.js';
import {cacheEntries, getMyFiles} from '../ducks/all_entries.js';
import {getEntry, getFileData} from '../store.js';

/**
 * @fileoverview UI entries slice of the store.
 * @suppress {checkTypes}
 *
 * UI entries represents entries shown on UI only (aka FakeEntry, e.g.
 * Recents/Trash/Google Drive wrapper), they don't have a real entry backup in
 * the file system.
 */


const slice = new Slice<State>('uiEntries');
export {slice as uiEntriesSlice};

const uiEntryRootTypesInMyFiles = new Set([
  VolumeManagerCommon.RootType.ANDROID_FILES,
  VolumeManagerCommon.RootType.CROSTINI,
  VolumeManagerCommon.RootType.GUEST_OS,
]);


/** Create action to add an UI entry to the store. */
export const addUiEntry = slice.addReducer('add', addUiEntryReducer);

export function addUiEntryReducer(currentState: State, payload: {
  entry: FakeEntryImpl,
}): State {
  // Cache entries, so the reducers can use any entry from `allEntries`.
  cacheEntries(currentState, [payload.entry]);

  const {entry} = payload;
  const key = entry.toURL();

  let isVolumeEntryExistedInMyFiles = false;
  if (uiEntryRootTypesInMyFiles.has(entry.rootType)) {
    const {myFilesEntry} = getMyFiles(currentState);
    const children = myFilesEntry.getUIChildren();
    // Check if the the ui entry already has a corresponding volume entry.
    isVolumeEntryExistedInMyFiles = !!children.find(
        childEntry =>
            isVolumeEntry(childEntry) && childEntry.name === entry.name);
    const isUiEntryExistedInMyFiles =
        !!children.find(childEntry => util.isSameEntry(childEntry, entry));
    // We only add the UI entry here if:
    // 1. it is not existed in MyFiles entry
    // 2. its corresponding volume (which ui entry is a placeholder for) is not
    // existed in MyFiles entry
    const shouldAddUiEntry =
        !isUiEntryExistedInMyFiles && !isVolumeEntryExistedInMyFiles;
    if (shouldAddUiEntry) {
      myFilesEntry.addEntry(entry);
      // Push the new entry to the children of FileData and sort them.
      const fileData = getFileData(currentState, myFilesEntry.toURL());
      if (fileData) {
        const newChildren = fileData.children.concat(entry.toURL());
        const childEntries =
            newChildren.map(childKey => getEntry(currentState, childKey)!);
        const sortedChildren =
            sortEntries(myFilesEntry, childEntries).map(entry => entry.toURL());
        currentState.allEntries[myFilesEntry.toURL()] = {
          ...fileData,
          children: sortedChildren,
        };
      }
    }
  }

  // If the corresponding volume entry exists, we don't add the ui entry here.
  if (!currentState.uiEntries.find(k => k === key) &&
      !isVolumeEntryExistedInMyFiles) {
    // Shallow copy.
    currentState.uiEntries = currentState.uiEntries.slice();
    currentState.uiEntries.push(key);
  }

  return {
    ...currentState,
  };
}

/** Create action to remove an UI entry from the store. */
export const removeUiEntry =
    slice.addReducer('remove', removeUiEntryReducer);

export function removeUiEntryReducer(currentState: State, payload: {
  key: FileKey,
}): State {
  const {key} = payload;
  const entry = getEntry(currentState, key) as FakeEntryImpl | null;
  if (currentState.uiEntries.find(k => k === key)) {
    // Shallow copy.
    currentState.uiEntries = currentState.uiEntries.filter(k => k !== key);
  }

  // We also need to remove it from the children of MyFiles if it's existed
  // there.
  if (entry && uiEntryRootTypesInMyFiles.has(entry.rootType)) {
    const {myFilesEntry} = getMyFiles(currentState);
    const children = myFilesEntry.getUIChildren();
    const isUiEntryExistedInMyFiles =
        !!children.find(childEntry => util.isSameEntry(childEntry, entry));
    if (isUiEntryExistedInMyFiles) {
      myFilesEntry.removeChildEntry(entry);
      const fileData = getFileData(currentState, myFilesEntry.toURL());
      if (fileData) {
        currentState.allEntries[myFilesEntry.toURL()] = {
          ...fileData,
          children: fileData.children.filter(child => child !== key),
        };
      }
    }
  }

  return {
    ...currentState,
  };
}
