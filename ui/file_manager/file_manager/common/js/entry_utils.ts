// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';
import {EntryType, FileData} from '../../externs/ts/state.js';
import {driveRootEntryListKey, myFilesEntryListKey} from '../../state/reducers/volumes.js';

import {EntryList, FakeEntryImpl, VolumeEntry} from './files_app_entry_types.js';
import {util} from './util.js';
import {VolumeManagerCommon} from './volume_manager_types.js';

/**
 * Type guard used to identify if a generic FileSystemEntry is actually a
 * FileSystemDirectoryEntry.
 */
export function isFileSystemDirectoryEntry(entry: FileSystemEntry):
    entry is FileSystemDirectoryEntry {
  return entry.isDirectory;
}

/**
 * Type guard used to identify if a generic FileSystemEntry is actually a
 * FileSystemFileEntry.
 */
export function isFileSystemFileEntry(entry: FileSystemEntry):
    entry is FileSystemFileEntry {
  return entry.isFile;
}

/**
 * Returns the native entry (aka FileEntry) from the Store. It returns `null`
 * for entries that aren't native.
 */
export function getNativeEntry(fileData: FileData): Entry|null {
  if (fileData.type === EntryType.FS_API) {
    return fileData.entry as Entry;
  }
  if (fileData.type === EntryType.VOLUME_ROOT) {
    return (fileData.entry as VolumeEntry).getNativeEntry();
  }
  return null;
}

/**
 * Type guard used to identify if a given entry is actually a
 * VolumeEntry.
 */
export function isVolumeEntry(entry: Entry|
                              FilesAppEntry): entry is VolumeEntry {
  return 'volumeInfo' in entry;
}

/**
 * Check if the entry is MyFiles or not.
 * Note: if the return value is true, the input entry is guaranteed to be
 * EntryList or VolumeEntry type.
 */
export function isMyFilesEntry(entry: Entry|FilesAppEntry|
                               null): entry is VolumeEntry|EntryList {
  if (!entry) {
    return false;
  }
  if (entry instanceof EntryList && entry.toURL() === myFilesEntryListKey) {
    return true;
  }
  if (isVolumeEntry(entry) &&
      entry.volumeType === VolumeManagerCommon.VolumeType.DOWNLOADS) {
    return true;
  }

  return false;
}

/**
 * Check if the entry is the drive root entry list ("Google Drive" wrapper).
 * Note: if the return value is true, the input entry is guaranteed to be
 * EntryList type.
 */
export function isDriveRootEntryList(entry: Entry|FilesAppEntry|
                                     null): entry is EntryList {
  if (!entry) {
    return false;
  }
  return entry.toURL() === driveRootEntryListKey;
}

/**
 * Given an entry, check if it's a grand root ("Shared drives" and
 * "Computers") inside Drive.
 * Note: if the return value is true, the input entry is guaranteed to be
 * DirectoryEntry type.
 */
export function isGrandRootEntryInDrives(entry: Entry|FilesAppEntry):
    entry is DirectoryEntry {
  const {fullPath} = entry;
  return fullPath === VolumeManagerCommon.SHARED_DRIVES_DIRECTORY_PATH ||
      fullPath === VolumeManagerCommon.COMPUTERS_DIRECTORY_PATH;
}

/**
 * Given an entry, check if it's a fake entry ("Shared with me" and "Offline")
 * inside Drive.
 */
export function isFakeEntryInDrives(entry: Entry|
                                    FilesAppEntry): entry is FakeEntryImpl {
  if (!(entry instanceof FakeEntryImpl)) {
    return false;
  }
  const {rootType} = entry;

  return rootType === VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME ||
      rootType === VolumeManagerCommon.RootType.DRIVE_OFFLINE;
}

/** Sort the entries based on the filter and the names. */
export function sortEntries(
    parentEntry: Entry|FilesAppEntry,
    entries: Array<Entry|FilesAppEntry>): Array<Entry|FilesAppEntry> {
  if (entries.length === 0) {
    return [];
  }
  // TODO: proper way to get directory model and volume manager.
  const {directoryModel, volumeManager} = window.fileManager;
  const fileFilter = directoryModel.getFileFilter();
  // For entries under My Files we need to use a different sorting logic
  // because we need to make sure curtain files are always at the bottom.
  if (isMyFilesEntry(parentEntry)) {
    // Use locationInfo from first entry because it only compare within the
    // same volume.
    // TODO(b/271485133): Do not use getLocationInfo() for sorting.
    const locationInfo = volumeManager.getLocationInfo(entries[0]!);
    if (locationInfo) {
      const compareFunction = util.compareLabelAndGroupBottomEntries(
          locationInfo,
          // Only Linux/Play/GuestOS files are in the UI children.
          parentEntry.getUIChildren(),
      );
      return entries.filter(entry => fileFilter.filter(entry))
          .sort(compareFunction);
    }
  }
  return entries.filter(entry => fileFilter.filter(entry))
      .sort(util.compareName);
}
