// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DialogType} from '../../common/js/dialog_type.js';
import {isDriveRootEntryList, isFakeEntryInDrives, isGrandRootEntryInDrives, sortEntries} from '../../common/js/entry_utils.js';
import {EntryList} from '../../common/js/files_app_entry_types.js';
import {metrics} from '../../common/js/metrics.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {FilesAppDirEntry, FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';
import {ActionsProducerGen} from '../../lib/actions_producer.js';
import {addChildEntries, AddChildEntriesAction} from '../actions/all_entries.js';
import {getFileData, getStore} from '../store.js';

/**
 * @fileoverview Action producers related to entries.
 * @suppress {checkTypes} TS already checks this file.
 */

/**
 * Read sub directories for a given entry.
 * TODO(b/271485133): Remove successCallback/errorCallback.
 */
export async function*
    readSubDirectories(
        entry: Entry|FilesAppEntry|null, recursive: boolean = false,
        metricNameForTracking: string = ''):
        ActionsProducerGen<AddChildEntriesAction> {
  if (!entry || !entry.isDirectory || ('disabled' in entry && entry.disabled)) {
    return;
  }

  // Track time for reading sub directories if metric for tracking is passed.
  if (metricNameForTracking) {
    metrics.startInterval(metricNameForTracking);
  }

  // Type casting here because TS can't exclude the invalid entry types via the
  // above if checks.
  const validEntry = entry as DirectoryEntry | FilesAppDirEntry;
  const childEntriesToReadDeeper: Array<Entry|FilesAppEntry> = [];
  if (isDriveRootEntryList(validEntry)) {
    for await (
        const action of readSubDirectoriesForDriveRootEntryList(validEntry)) {
      yield action;
      if (action) {
        childEntriesToReadDeeper.push(...action.payload.entries);
      }
    }
  } else {
    const childEntries = await readChildEntriesForDirectoryEntry(validEntry);
    // Only dispatch directories.
    const subDirectories =
        childEntries.filter(childEntry => childEntry.isDirectory);
    yield addChildEntries({parentKey: entry.toURL(), entries: subDirectories});
    childEntriesToReadDeeper.push(...subDirectories);
  }

  // Track time for reading sub directories if metric for tracking is passed.
  if (metricNameForTracking) {
    metrics.recordInterval(metricNameForTracking);
  }

  // Read sub directories for children when recursive is true.
  if (recursive) {
    // We only read deeper if the parent entry is expanded in the tree.
    const fileData = getFileData(getStore().getState(), entry.toURL());
    if (fileData?.expanded) {
      for (const childEntry of childEntriesToReadDeeper) {
        for await (const action of readSubDirectories(
            childEntry, /* recursive */ true)) {
          yield action;
        }
      }
    }
  }
}

/**
 * Read entries for Drive root entry list (aka "Google Drive"), there are some
 * differences compared to the `readSubDirectoriesForDirectoryEntry`:
 * * We don't need to call readEntries to get its child entries. Instead, all
 * its children are from its entry.getUIChildren().
 * * For fake entries children (e.g. Shared with me and Offline), we only show
 * them based on the dialog type.
 * * For curtain children (e.g. team drives and computers grand root), we only
 * show them when there's at least one child entries inside. So we need to read
 * their children (grand children of drive fake root) first before we can decide
 * if we need to show them or not.
 */
async function*
    readSubDirectoriesForDriveRootEntryList(entry: EntryList):
        ActionsProducerGen<AddChildEntriesAction> {
  const metricNameMap = {
    [VolumeManagerCommon.SHARED_DRIVES_DIRECTORY_PATH]: 'TeamDrivesCount',
    [VolumeManagerCommon.COMPUTERS_DIRECTORY_PATH]: 'ComputerCount',
  };

  const driveChildren = entry.getUIChildren();
  /**
   * Store the filtered children, for fake entries or grand roots we might need
   * to hide them based on curtain conditions.
   */
  const filteredChildren: Array<Entry|FilesAppEntry> = [];

  const isFakeEntryVisible =
      window.fileManager.dialogType !== DialogType.SELECT_SAVEAS_FILE;

  for (const childEntry of driveChildren) {
    // For fake entries ("Shared with me" and)
    if (isFakeEntryInDrives(childEntry)) {
      if (isFakeEntryVisible) {
        filteredChildren.push(childEntry);
      }
      continue;
    }
    // For non grand roots (also not fake entries), we put them in the children
    // directly and dispatch an action to read the it later.
    if (!isGrandRootEntryInDrives(childEntry)) {
      filteredChildren.push(childEntry);
      continue;
    }
    // For grand roots ("Shared drives" and "Computers") inside Drive, we only
    // show them when there's at least one child entries inside.
    const grandChildEntries =
        await readChildEntriesForDirectoryEntry(childEntry);
    metrics.recordSmallCount(
        metricNameMap[childEntry.fullPath]!, grandChildEntries.length);
    if (grandChildEntries.length > 0) {
      filteredChildren.push(childEntry);
    }
  }
  yield addChildEntries({parentKey: entry.toURL(), entries: filteredChildren});
}

/**
 * Read child entries for a given directory entry.
 */
async function readChildEntriesForDirectoryEntry(
    entry: DirectoryEntry|
    FilesAppDirEntry): Promise<Array<Entry|FilesAppEntry>> {
  return new Promise<Array<Entry|FilesAppEntry>>(resolve => {
    const reader = entry.createReader();
    const subEntries: Array<Entry|FilesAppEntry> = [];
    const readEntry = () => {
      reader.readEntries((entries) => {
        if (entries.length === 0) {
          resolve(sortEntries(entry, subEntries));
          return;
        }
        for (const subEntry of entries) {
          subEntries.push(subEntry);
        }
        readEntry();
      });
    };
    readEntry();
  });
}
