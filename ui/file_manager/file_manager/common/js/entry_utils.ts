// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EntryLocation} from '../../externs/entry_location.js';
import {FakeEntry, FilesAppDirEntry, FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';
import {EntryType, FileData} from '../../externs/ts/state.js';
import {driveRootEntryListKey, myFilesEntryListKey} from '../../state/ducks/volumes.js';

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

/**
 * Returns true if fileData's entry is inside any part of Drive 'My Drive'.
 */
export function isEntryInsideMyDrive(fileData: FileData): boolean {
  const {rootType} = fileData;
  return !!rootType && rootType === VolumeManagerCommon.RootType.DRIVE;
}

/**
 * Returns true if fileData's entry is inside any part of Drive 'Computers'.
 */
export function isEntryInsideComputers(fileData: FileData): boolean {
  const {rootType} = fileData;
  return !!rootType &&
      (rootType === VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT ||
       rootType === VolumeManagerCommon.RootType.COMPUTER);
}

/**
 * Returns true if fileData's entry is inside any part of Drive.
 */
export function isEntryInsideDrive(fileData: FileData): boolean {
  const {rootType} = fileData;
  return !!rootType &&
      (rootType === VolumeManagerCommon.RootType.DRIVE ||
       rootType === VolumeManagerCommon.RootType.SHARED_DRIVES_GRAND_ROOT ||
       rootType === VolumeManagerCommon.RootType.SHARED_DRIVE ||
       rootType === VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT ||
       rootType === VolumeManagerCommon.RootType.COMPUTER ||
       rootType === VolumeManagerCommon.RootType.DRIVE_OFFLINE ||
       rootType === VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME ||
       rootType === VolumeManagerCommon.RootType.DRIVE_FAKE_ROOT);
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

/**
 * Take an entry and extract the rootType.
 */
export function getRootType(entry: Entry|FilesAppEntry|DirectoryEntry|
                            FilesAppDirEntry|FakeEntry|
                            EntryLocation): VolumeManagerCommon.RootType|null {
  return 'rootType' in entry ? entry.rootType : null;
}

/**
 * Obtains whether an entry is fake or not.
 */
export function isFakeEntry(entry: Entry|FilesAppEntry) {
  if (entry.getParent === undefined) {
    return true;
  }
  return 'isNativeType' in entry ? !entry.isNativeType : false;
}


/**
 * Obtains whether an entry is the root directory of a Shared Drive.
 */
export function isTeamDriveRoot(entry: Entry|FilesAppEntry) {
  if (entry === null) {
    return false;
  }
  if (!entry.fullPath) {
    return false;
  }
  const tree = entry.fullPath.split('/');
  return tree.length == 3 && isSharedDriveEntry(entry);
}

/**
 * Obtains whether an entry is the grand root directory of Shared Drives.
 */
export function isTeamDrivesGrandRoot(entry: Entry|FakeEntry) {
  if (!entry.fullPath) {
    return false;
  }
  const tree = entry.fullPath.split('/');
  return tree.length == 2 && isSharedDriveEntry(entry);
}

/**
 * Obtains whether an entry is descendant of the Shared Drives directory.
 */
export function isSharedDriveEntry(entry: Entry|FilesAppEntry) {
  if (!entry.fullPath) {
    return false;
  }
  const tree = entry.fullPath.split('/');
  return tree[0] == '' &&
      tree[1] == VolumeManagerCommon.SHARED_DRIVES_DIRECTORY_NAME;
}

/**
 * Extracts Shared Drive name from entry path.
 * @return {string} The name of Shared Drive. Empty string if |entry| is not
 *     under Shared Drives.
 */
export function getTeamDriveName(entry: Entry|FakeEntry|FilesAppEntry) {
  if (!entry.fullPath || !isSharedDriveEntry(entry)) {
    return '';
  }
  const tree = entry.fullPath.split('/');
  if (tree.length < 3) {
    return '';
  }
  return tree[2] || '';
}

/**
 * Returns true if the given root type is for a container of recent files.
 */
export function isRecentRootType(rootType: VolumeManagerCommon.RootType|null) {
  return rootType == VolumeManagerCommon.RootType.RECENT;
}

/**
 * Returns true if the given entry is the root folder of recent files.
 */
export function isRecentRoot(entry: Entry|FilesAppEntry) {
  return isFakeEntry(entry) && isRecentRootType(getRootType(entry));
}

/**
 * Obtains whether an entry is the root directory of a Computer.
 */
export function isComputersRoot(entry: Entry|FilesAppEntry) {
  if (entry === null) {
    return false;
  }
  if (!entry.fullPath) {
    return false;
  }
  const tree = entry.fullPath.split('/');
  return tree.length == 3 && isComputersEntry(entry);
}

/**
 * Obtains whether an entry is descendant of the My Computers directory.
 */
export function isComputersEntry(entry: Entry|FilesAppEntry) {
  if (!entry.fullPath) {
    return false;
  }
  const tree = entry.fullPath.split('/');
  return tree[0] == '' &&
      tree[1] == VolumeManagerCommon.COMPUTERS_DIRECTORY_NAME;
}

/**
 * Returns true if the given root type is Trash.
 */
export function isTrashRootType(rootType: VolumeManagerCommon.RootType|null) {
  return rootType == VolumeManagerCommon.RootType.TRASH;
}

/**
 * Returns true if the given entry is the root folder of Trash.
 */
export function isTrashRoot(entry: Entry|FilesAppEntry) {
  return entry.fullPath === '/' && isTrashRootType(getRootType(entry));
}

/**
 * Returns true if the given entry is a descendent of Trash.
 */
export function isTrashEntry(entry: Entry|FilesAppEntry) {
  return isTrashRootType(getRootType(entry));
}


/**
 * Compares two entries.
 * @return {boolean} True if the both entry represents a same file or
 *     directory. Returns true if both entries are null.
 */
export function isSameEntry(
    entry1: Entry|FilesAppEntry|undefined,
    entry2: Entry|FilesAppEntry|undefined) {
  if (!entry1 && !entry2) {
    return true;
  }
  if (!entry1 || !entry2) {
    return false;
  }
  return entry1.toURL() === entry2.toURL();
}

/**
 * Compares two entry arrays.
 * @return {boolean} True if the both arrays contain same files or directories
 *     in the same order. Returns true if both arrays are null.
 */
export function isSameEntries(entries1: Entry[], entries2: Entry[]) {
  if (!entries1 && !entries2) {
    return true;
  }
  if (!entries1 || !entries2) {
    return false;
  }
  if (entries1.length !== entries2.length) {
    return false;
  }
  for (let i = 0; i < entries1.length; i++) {
    if (!isSameEntry(entries1[i], entries2[i])) {
      return false;
    }
  }
  return true;
}

/**
 * Compares two file systems.
 * @return {boolean} True if the both file systems are equal. Also, returns true
 *     if both file systems are null.
 */
export function isSameFileSystem(
    fileSystem1: FileSystem|null, fileSystem2: FileSystem|null) {
  if (!fileSystem1 && !fileSystem2) {
    return true;
  }
  if (!fileSystem1 || !fileSystem2) {
    return false;
  }
  return isSameEntry(fileSystem1.root, fileSystem2.root);
}

/**
 * Checks if given two entries are in the same directory.
 * @return {boolean} True if given entries are in the same directory.
 */
export function isSiblingEntry(entry1: Entry, entry2: Entry) {
  const path1 = entry1.fullPath.split('/');
  const path2 = entry2.fullPath.split('/');
  if (path1.length != path2.length) {
    return false;
  }
  for (let i = 0; i < path1.length - 1; i++) {
    if (path1[i] != path2[i]) {
      return false;
    }
  }
  return true;
}

/**
 * Checks if the child entry is a descendant of another entry. If the entries
 * point to the same file or directory, then returns false.
 *
 * @param {!DirectoryEntry|!FilesAppEntry} ancestorEntry The ancestor
 *     directory entry. Can be a fake.
 * @param {!Entry|!FilesAppEntry} childEntry The child entry. Can be a fake.
 * @return {boolean} True if the child entry is contained in the ancestor path.
 */
export function isDescendantEntry(
    ancestorEntry: Entry|FilesAppEntry,
    childEntry: Entry|FilesAppEntry): boolean {
  if (!ancestorEntry.isDirectory) {
    return false;
  }

  // For EntryList and VolumeEntry they can contain entries from different
  // files systems, so we should check its getUIChildren.
  if ('getUIChildren' in ancestorEntry) {
    const volumeOrEntryList = ancestorEntry as VolumeEntry | EntryList;
    // VolumeEntry has to check to root entry descendant entry.
    if ('getNativeEntry' in volumeOrEntryList) {
      const nativeEntry = volumeOrEntryList.getNativeEntry();
      if (nativeEntry &&
          isSameFileSystem(nativeEntry.filesystem, childEntry.filesystem)) {
        return isDescendantEntry(nativeEntry, childEntry);
      }
    }

    return volumeOrEntryList.getUIChildren().some(
        (ancestorChild: Entry|FilesAppEntry) => {
          if (isSameEntry(ancestorChild, childEntry)) {
            return true;
          }

          // root entry might not be resolved yet.
          const volumeEntry = 'getNativeEntry' in ancestorChild ?
              ancestorChild.getNativeEntry() :
              null;
          if (!volumeEntry) {
            return false;
          }

          if (isSameEntry(volumeEntry, childEntry)) {
            return true;
          }

          return isFileSystemDirectoryEntry(volumeEntry) &&
              isDescendantEntry(volumeEntry, childEntry);
        });
  }

  if (!isSameFileSystem(ancestorEntry.filesystem, childEntry.filesystem)) {
    return false;
  }
  if (isSameEntry(ancestorEntry, childEntry)) {
    return false;
  }
  if (isFakeEntry(ancestorEntry) || isFakeEntry(childEntry)) {
    return false;
  }

  // Check if the ancestor's path with trailing slash is a prefix of child's
  // path.
  let ancestorPath = ancestorEntry.fullPath;
  if (ancestorPath.slice(-1) !== '/') {
    ancestorPath += '/';
  }
  return childEntry.fullPath.indexOf(ancestorPath) === 0;
}
