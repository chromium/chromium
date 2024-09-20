// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {EntryLocation} from '../../background/js/entry_location_impl.js';
import type {VolumeInfo} from '../../background/js/volume_info.js';
import type {VolumeManager} from '../../background/js/volume_manager.js';
import type {FakeEntry, FilesAppDirEntry, FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import {ODFS_EXTENSION_ID} from '../../foreground/js/constants.js';
import {driveRootEntryListKey, myFilesEntryListKey, recentRootKey, trashRootKey} from '../../state/ducks/volumes.js';
import {type CurrentDirectory, EntryType, type FileData, type FileKey, type State, type Volume} from '../../state/state.js';
import {getEntry, getStore, getVolume} from '../../state/store.js';
import type {XfTreeItem} from '../../widgets/xf_tree_item.js';

import {createDOMError} from './dom_utils.js';
import type {VolumeEntry} from './files_app_entry_types.js';
import {EntryList, FakeEntryImpl} from './files_app_entry_types.js';
import {isArcVmEnabled, isPluginVmEnabled, isSkyvaultV2Enabled} from './flags.js';
import {collator, getEntryLabel} from './translations.js';
import type {TrashEntry} from './trash.js';
import {FileErrorToDomError} from './util.js';
import {COMPUTERS_DIRECTORY_NAME, COMPUTERS_DIRECTORY_PATH, RootType, SHARED_DRIVES_DIRECTORY_NAME, SHARED_DRIVES_DIRECTORY_PATH, VolumeType} from './volume_manager_types.js';

/**
 * Type guard used to identify if a generic Entry is actually a DirectoryEntry.
 */
export function isDirectoryEntry(entry: Entry|FilesAppEntry):
    entry is(DirectoryEntry | FilesAppDirEntry) {
  return entry.isDirectory;
}

/**
 * Type guard used to identify if a generic Entry is actually a FileEntry.
 */
export function isFileEntry(entry: Entry): entry is FileEntry {
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

export function isVolumeFileData(fileData: FileData): boolean {
  return fileData.type === EntryType.VOLUME_ROOT;
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
  if (isVolumeEntry(entry) && entry.volumeType === VolumeType.DOWNLOADS) {
    return true;
  }

  return false;
}

export function isMyFilesFileData(
    state: State, fileData: FileData|null): boolean {
  if (!fileData) {
    return false;
  }
  if (fileData.key === myFilesEntryListKey) {
    return true;
  }
  if (fileData.type === EntryType.VOLUME_ROOT) {
    const volume = getVolume(state, fileData);
    return volume?.volumeType === VolumeType.DOWNLOADS;
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
 * Given an entry, check if it's a grand root ("Shared drives" and  "Computers")
 * inside Drive. Note: if the return value is true, the input entry is
 * guaranteed to be DirectoryEntry type.
 */
export function isGrandRootEntryInDrive(entry: Entry|FilesAppEntry):
    entry is DirectoryEntry {
  const {fullPath} = entry;
  return fullPath === SHARED_DRIVES_DIRECTORY_PATH ||
      fullPath === COMPUTERS_DIRECTORY_PATH;
}

/**
 * Given an entry, check if it's a fake entry ("Shared with me" and "Offline")
 * inside Drive.
 */
export function isFakeEntryInDrive(entry: Entry|
                                   FilesAppEntry): entry is FakeEntry {
  if (!(entry instanceof FakeEntryImpl)) {
    return false;
  }
  const {rootType} = entry;

  return rootType === RootType.DRIVE_SHARED_WITH_ME ||
      rootType === RootType.DRIVE_OFFLINE;
}

/**
 * Returns true if fileData's entry is inside any part of Drive 'My Drive'.
 */
export function isEntryInsideMyDrive(fileData: FileData): boolean {
  const {rootType} = fileData;
  return !!rootType && rootType === RootType.DRIVE;
}

/**
 * Returns true if fileData's entry is inside any part of Drive 'Computers'.
 */
export function isEntryInsideComputers(fileData: FileData): boolean {
  const {rootType} = fileData;
  return !!rootType &&
      (rootType === RootType.COMPUTERS_GRAND_ROOT ||
       rootType === RootType.COMPUTER);
}

/**
 * Returns true if fileData's entry is inside any part of Drive.
 */
export function isInsideDrive(fileData: FileData|CurrentDirectory): boolean {
  const {rootType} = fileData;
  return isDriveRootType(rootType);
}

/**
 * Returns whether or not the root type is one of Google Drive root types.
 */
export function isDriveRootType(rootType: RootType|null|undefined): boolean {
  return !!rootType &&
      (rootType === RootType.DRIVE ||
       rootType === RootType.SHARED_DRIVES_GRAND_ROOT ||
       rootType === RootType.SHARED_DRIVE ||
       rootType === RootType.COMPUTERS_GRAND_ROOT ||
       rootType === RootType.COMPUTER || rootType === RootType.DRIVE_OFFLINE ||
       rootType === RootType.DRIVE_SHARED_WITH_ME ||
       rootType === RootType.DRIVE_FAKE_ROOT);
}

/** Sort the entries based on the filter and the names. */
export function sortEntries(
    parentEntry: Entry|FilesAppEntry,
    entries: Array<Entry|FilesAppEntry>): Array<Entry|FilesAppEntry> {
  if (entries.length === 0) {
    return [];
  }
  // TODO: proper way to get file filter and volume manager.
  const {fileFilter, volumeManager} = window.fileManager;
  // For entries under My Files we need to use a different sorting logic
  // because we need to make sure curtain files are always at the bottom.
  if (isMyFilesEntry(parentEntry)) {
    // Use locationInfo from first entry because it only compare within the
    // same volume.
    // TODO(b/271485133): Do not use getLocationInfo() for sorting.
    const locationInfo = volumeManager.getLocationInfo(entries[0]!);
    if (locationInfo) {
      const compareFunction = compareLabelAndGroupBottomEntries(
          locationInfo,
          // Only Linux/Play/GuestOS files are in the UI children.
          parentEntry.getUiChildren(),
      );
      return entries.filter(entry => fileFilter.filter(entry))
          .sort(compareFunction);
    }
  }
  return entries.filter(entry => fileFilter.filter(entry)).sort(compareName);
}

/**
 * Take an entry and extract the rootType.
 */
export function getRootType(entry: Entry|FilesAppEntry|DirectoryEntry|
                            FilesAppDirEntry|FakeEntry|EntryLocation): RootType|
    null {
  return 'rootType' in entry ? entry.rootType : null;
}

/**
 * Obtains whether an entry is fake or not.
 */
export function isFakeEntry(entry: Entry|FilesAppEntry): entry is FakeEntry {
  if (entry?.getParent === undefined) {
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
  return tree.length === 3 && isSharedDriveEntry(entry);
}

/**
 * Obtains whether an entry is the grand root directory of Shared Drives.
 */
export function isTeamDrivesGrandRoot(entry: Entry|FilesAppEntry) {
  if (!entry.fullPath) {
    return false;
  }
  const tree = entry.fullPath.split('/');
  return tree.length === 2 && isSharedDriveEntry(entry);
}

/**
 * Obtains whether an entry is descendant of the Shared Drives directory.
 */
export function isSharedDriveEntry(entry: Entry|FilesAppEntry) {
  if (!entry.fullPath) {
    return false;
  }
  const tree = entry.fullPath.split('/');
  return tree[0] === '' && tree[1] === SHARED_DRIVES_DIRECTORY_NAME;
}

/**
 * Extracts Shared Drive name from entry path.
 * @return The name of Shared Drive. Empty string if |entry| is not
 *     under Shared Drives.
 */
export function getTeamDriveName(entry: Entry|FakeEntry|FilesAppEntry): string {
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
export function isRecentRootType(rootType: RootType|null) {
  return rootType === RootType.RECENT;
}

/**
 * Returns true if the given entry is the root folder of recent files.
 */
export function isRecentRoot(entry: Entry|FilesAppEntry) {
  return isFakeEntry(entry) && isRecentRootType(getRootType(entry));
}

/**
 * Whether the `fileData` the is RECENT root.
 * NOTE: Drive shared with me and offline are marked as RECENT for its "type"
 * field, so we need to use "rootType" instead.
 */
export function isRecentFileData(fileData: FileData|null|undefined): boolean {
  return !!fileData && fileData.rootType === RootType.RECENT;
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
  return tree.length === 3 && isComputersEntry(entry);
}

/**
 * Obtains whether an entry is descendant of the My Computers directory.
 */
export function isComputersEntry(entry: Entry|FilesAppEntry) {
  if (!entry.fullPath) {
    return false;
  }
  const tree = entry.fullPath.split('/');
  return tree[0] === '' && tree[1] === COMPUTERS_DIRECTORY_NAME;
}

/**
 * Returns true if the given root type is Trash.
 */
export function isTrashRootType(rootType: RootType|null) {
  return rootType === RootType.TRASH;
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
export function isTrashEntry(entry: Entry|FilesAppEntry): entry is TrashEntry {
  return isTrashRootType(getRootType(entry));
}

export function isTrashFileData(fileData: FileData): boolean {
  return fileData.fullPath === '/' && fileData.type === EntryType.TRASH;
}

/**
 * Returns true if the given entry is a placeholder for OneDrive.
 */
export function isOneDrivePlaceholder(entry: Entry|FilesAppEntry) {
  return isFakeEntry(entry) && isOneDrivePlaceholderKey(entry.toURL());
}

/**
 * Compares two entries.
 * @return True if the both entry represents a same file or
 *     directory. Returns true if both entries are null.
 */
export function isSameEntry(
    entry1: Entry|FilesAppEntry|null|undefined,
    entry2: Entry|FilesAppEntry|null|undefined): boolean {
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
 * @return True if the both arrays contain same files or directories
 *     in the same order. Returns true if both arrays are null.
 */
export function isSameEntries(
    entries1: Array<Entry|FilesAppEntry>,
    entries2: Array<Entry|FilesAppEntry>): boolean {
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
 * @return True if the both file systems are equal. Also, returns true
 *     if both file systems are null.
 */
export function isSameFileSystem(
    fileSystem1: FileSystem|null, fileSystem2: FileSystem|null): boolean {
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
 * @return True if given entries are in the same directory.
 */
export function isSiblingEntry(
    entry1: Entry|FilesAppEntry, entry2: Entry|FilesAppEntry): boolean {
  const path1 = entry1.fullPath.split('/');
  const path2 = entry2.fullPath.split('/');
  if (path1.length !== path2.length) {
    return false;
  }
  for (let i = 0; i < path1.length - 1; i++) {
    if (path1[i] !== path2[i]) {
      return false;
    }
  }
  return true;
}

/**
 * Checks if the child entry is a descendant of another entry. If the entries
 * point to the same file or directory, then returns false.
 *
 * @param ancestorEntry The ancestor
 *     directory entry. Can be a fake.
 * @param childEntry The child entry. Can be a fake.
 * @return True if the child entry is contained in the ancestor path.
 */
export function isDescendantEntry(
    ancestorEntry: Entry|FilesAppEntry,
    childEntry: Entry|FilesAppEntry): boolean {
  if (!ancestorEntry.isDirectory) {
    return false;
  }

  // For EntryList and VolumeEntry they can contain entries from different
  // files systems, so we should check its getUiChildren.
  if (isEntrySupportUiChildren(ancestorEntry)) {
    // VolumeEntry has to check to root entry descendant entry.
    if ('getNativeEntry' in ancestorEntry) {
      const nativeEntry = ancestorEntry.getNativeEntry();
      if (nativeEntry &&
          isSameFileSystem(nativeEntry.filesystem, childEntry.filesystem)) {
        return isDescendantEntry(nativeEntry, childEntry);
      }
    }

    return ancestorEntry.getUiChildren().some(
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

          return isDirectoryEntry(volumeEntry) &&
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

/**
 * Compare by name. The 2 entries must be in same directory.
 */
export function compareName(
    entry1: Entry|FilesAppEntry, entry2: Entry|FilesAppEntry) {
  return collator.compare(entry1.name, entry2.name);
}

/**
 * Compare by label (i18n name). The 2 entries must be in same directory.
 */
export function compareLabel(
    locationInfo: EntryLocation, entry1: Entry|FilesAppEntry,
    entry2: Entry|FilesAppEntry) {
  return collator.compare(
      getEntryLabel(locationInfo, entry1), getEntryLabel(locationInfo, entry2));
}

/**
 * Compare by path.
 */
export function comparePath(
    entry1: Entry|FilesAppEntry, entry2: Entry|FilesAppEntry) {
  return collator.compare(entry1.fullPath, entry2.fullPath);
}

/**
 * @param bottomEntries entries that should be grouped in the bottom, used for
 *     sorting Linux and Play files entries after
 * other folders in MyFiles.
 */
export function compareLabelAndGroupBottomEntries(
    locationInfo: EntryLocation, bottomEntries: Array<Entry|FilesAppEntry>) {
  const childrenMap = new Map();
  bottomEntries.forEach((entry) => {
    childrenMap.set(entry.toURL(), entry);
  });

  /**
   * Compare entries putting entries from |bottomEntries| in the bottom and
   * sort by name within entries that are the same type in regards to
   * |bottomEntries|.
   */
  function compare(entry1: Entry|FilesAppEntry, entry2: Entry|FilesAppEntry) {
    // Bottom entry here means Linux or Play files, which should appear after
    // all native entries.
    const isBottomEntry1 = childrenMap.has(entry1.toURL()) ? 1 : 0;
    const isBottomEntry2 = childrenMap.has(entry2.toURL()) ? 1 : 0;

    // When there are the same type, just compare by label.
    if (isBottomEntry1 === isBottomEntry2) {
      return compareLabel(locationInfo, entry1, entry2);
    }

    return isBottomEntry1 - isBottomEntry2;
  }

  return compare;
}


/**
 * Converts array of entries to an array of corresponding URLs.
 */
export function entriesToURLs(entries: Array<Entry|FilesAppEntry>): string[] {
  return entries.map(entry => {
    // When building file_manager_base.js, cachedUrl is not referred other than
    // here. Thus closure compiler raises an error if we refer the property like
    // entry.cachedUrl.
    if ('cachedUrl' in entry) {
      return entry['cachedUrl'] as string || entry.toURL();
    }
    return entry.toURL();
  });
}

/**
 * Converts array of URLs to an array of corresponding Entries.
 *
 * @param callback Completion callback with array of success Entries and failure
 *     URLs.
 */
export function convertURLsToEntries(
    urls: string[], callback?: (entries: Entry[], urls: string[]) => void) {
  const promises = urls.map(url => {
    return new Promise(window.webkitResolveLocalFileSystemURL.bind(null, url))
        .then(
            entry => {
              return {entry: entry};
            },
            _ => {
              // Not an error. Possibly, the file is not accessible anymore.
              console.warn('Failed to resolve the file with url: ' + url + '.');
              return {failureUrl: url};
            });
  });
  const resultPromise = Promise.all(promises).then(results => {
    const entries = [];
    const failureUrls = [];
    for (let i = 0; i < results.length; i++) {
      const result = results[i]!;
      if ('entry' in result) {
        entries.push(result.entry);
      }
      if ('failureUrl' in result) {
        failureUrls.push(result.failureUrl);
      }
    }
    return {
      entries: entries,
      failureUrls: failureUrls,
    };
  });

  // Invoke the callback. If opt_callback is specified, resultPromise is still
  // returned and fulfilled with a result.
  if (callback) {
    resultPromise
        .then(result => {
          callback(result.entries, result.failureUrls);
        })
        .catch(error => {
          console.warn(
              'convertURLsToEntries has failed.',
              error.stack ? error.stack : error);
        });
  }

  return resultPromise;
}

/**
 * Converts a url into an {!Entry}, if possible.
 */
export function urlToEntry(url: string) {
  return new Promise(window.webkitResolveLocalFileSystemURL.bind(null, url));
}

/**
 * Returns true if the given |entry| matches any of the special entries:
 *
 *  - "My Files"/{Downloads,PvmDefault,Camera} directories, or
 *  - "Play Files"/{<any-directory>,DCIM/Camera} directories, or
 *  - "Linux Files" root "/" directory
 *  - "Guest OS" root "/" directory
 *
 * which cannot be modified such as deleted/cut or renamed.
 */
export function isNonModifiable(
    volumeManager: VolumeManager, entry: Entry|FilesAppEntry|undefined) {
  if (!entry) {
    return false;
  }

  if (isFakeEntry(entry)) {
    return true;
  }

  if (!volumeManager) {
    return false;
  }

  const volumeInfo = volumeManager.getVolumeInfo(entry);
  if (!volumeInfo) {
    return false;
  }

  const volumeType = volumeInfo.volumeType;

  if (volumeType === VolumeType.DOWNLOADS) {
    if (!entry.isDirectory) {
      return false;
    }

    const fullPath = entry.fullPath;

    if (fullPath === '/Downloads') {
      return true;
    }

    if (fullPath === '/PvmDefault' && isPluginVmEnabled()) {
      return true;
    }

    if (fullPath === '/Camera') {
      return true;
    }

    return false;
  }

  if (volumeType === VolumeType.ANDROID_FILES) {
    if (!entry.isDirectory) {
      return false;
    }

    const fullPath = entry.fullPath;

    if (fullPath === '/') {
      return true;
    }

    const isRootDirectory = fullPath === ('/' + entry.name);
    if (isRootDirectory) {
      return true;
    }

    if (fullPath === '/DCIM/Camera') {
      return true;
    }

    return false;
  }

  if (volumeType === VolumeType.CROSTINI) {
    return entry.fullPath === '/';
  }

  if (volumeType === VolumeType.GUEST_OS) {
    return entry.fullPath === '/';
  }

  return false;
}


/**
 * Retrieves all entries inside the given |rootEntry|.
 * @param entriesCallback Called when some chunk of entries are read. This can
 *     be called a couple of times until the completion.
 * @param successCallback Called when the read is completed.
 * @param errorCallback Called when an error occurs.
 * @param shouldStop Callback to check if the read process should stop or not.
 *     When this callback is called and it returns true, the remaining recursive
 *     reads will be aborted.
 * @param maxDepth Max depth to delve directories recursively. If 0 is
 *     specified, only the rootEntry will be read. If -1 is specified or
 *     maxDepth is unspecified, the depth of recursion is unlimited.
 */
export function readEntriesRecursively(
    rootEntry: DirectoryEntry|FilesAppDirEntry,
    entriesCallback: (entries: Array<Entry|FilesAppEntry>) => void,
    successCallback: VoidCallback, errorCallback: ErrorCallback,
    shouldStop: () => boolean, maxDepth?: number) {
  let numRunningTasks = 0;
  let error: Error|null = null;
  const maxDirDepth = maxDepth === undefined ? -1 : maxDepth;
  const maybeRunCallback = () => {
    if (numRunningTasks === 0) {
      if (shouldStop()) {
        errorCallback(createDOMError(FileErrorToDomError.ABORT_ERR));
      } else if (error) {
        errorCallback(error);
      } else {
        successCallback();
      }
    }
  };
  const processEntry =
      (entry: DirectoryEntry|FilesAppDirEntry, depth: number) => {
        const onError: ErrorCallback = (fileError: Error) => {
          if (!error) {
            error = fileError;
          }
          numRunningTasks--;
          maybeRunCallback();
        };
        const onSuccess = (entries: Array<Entry|FilesAppEntry>) => {
          if (shouldStop() || error || entries.length === 0) {
            numRunningTasks--;
            maybeRunCallback();
            return;
          }
          entriesCallback(entries);
          for (let i = 0; i < entries.length; i++) {
            const entry = entries[i];
            if (entry && isDirectoryEntry(entry) &&
                (maxDirDepth === -1 || depth < maxDirDepth)) {
              processEntry(entry, depth + 1);
            }
          }
          // Read remaining entries.
          reader.readEntries(onSuccess, onError);
        };

        numRunningTasks++;
        const reader = entry.createReader();
        reader.readEntries(onSuccess, onError);
      };

  processEntry(rootEntry, 0);
}


/**
 * Returns true if entry is FileSystemEntry or FileSystemDirectoryEntry, it
 * returns false if it's FakeEntry or any one of the FilesAppEntry types.
 */
export function isNativeEntry(entry: Entry|FilesAppEntry) {
  return !('typeName' in entry);
}

/**
 * For FilesAppEntry types that wraps a native entry, returns the native entry
 * to be able to send to fileManagerPrivate API.
 */
type AllEntryTypes = DirectoryEntry|FilesAppDirEntry|Entry|FilesAppEntry;
export function unwrapEntry<T extends DirectoryEntry|FilesAppDirEntry>(
    entry: T): DirectoryEntry|FilesAppDirEntry;
export function unwrapEntry<T extends AllEntryTypes>(entry: T): AllEntryTypes;
export function unwrapEntry<T extends AllEntryTypes>(entry: T): AllEntryTypes {
  if (!entry) {
    return entry;
  }

  const nativeEntry = 'getNativeEntry' in entry && entry.getNativeEntry();
  return nativeEntry || entry;
}

/**
 * Used for logs and debugging. It tries to tell what type is the entry, its
 * path and URL.
 */
export function entryDebugString(entry: Entry|FilesAppEntry) {
  if (entry === null) {
    return 'entry is null';
  }
  if (entry === undefined) {
    return 'entry is undefined';
  }
  let typeName = '';
  if (entry.constructor && entry.constructor.name) {
    typeName = entry.constructor.name;
  } else {
    typeName = Object.prototype.toString.call(entry);
  }
  let entryDescription = '(' + typeName + ') ';
  if (entry.fullPath) {
    entryDescription = entryDescription + entry.fullPath + ' ';
  }
  if (entry.toURL) {
    entryDescription = entryDescription + entry.toURL();
  }
  return entryDescription;
}

/**
 * Returns true if all entries belong to the same volume. If there are no
 * entries it also returns false.
 */
export function isSameVolume(
    entries: Array<Entry|FilesAppEntry>, volumeManager: VolumeManager) {
  if (!entries.length) {
    return false;
  }

  const firstEntry = entries[0];
  if (!firstEntry) {
    return false;
  }
  const volumeInfo = volumeManager.getVolumeInfo(firstEntry);

  for (let i = 1; i < entries.length; i++) {
    if (!entries[i]) {
      return false;
    }
    const volumeInfoToCompare = volumeManager.getVolumeInfo(entries[i]!);
    if (!volumeInfoToCompare ||
        volumeInfoToCompare.volumeId !== volumeInfo?.volumeId) {
      return false;
    }
  }

  return true;
}

/**
 * Returns the ODFS root as an Entry. Request the actions of this
 * Entry to get ODFS metadata.
 */
export function getODFSMetadataQueryEntry(odfsVolumeInfo: VolumeInfo) {
  return unwrapEntry(odfsVolumeInfo.displayRoot);
}

/**
 * Return true if the volume with |volumeInfo| is an
 * interactive volume.
 */
export function isInteractiveVolume(volumeInfo: VolumeInfo) {
  const state = getStore().getState();
  const volume = state.volumes[volumeInfo.volumeId];
  if (!volume) {
    console.warn('Expected volume to be in the store.');
    return true;
  }
  return volume.isInteractive;
}

export const isOneDriveId = (providerId: string|null|undefined) =>
    providerId === ODFS_EXTENSION_ID;

export function isOneDrive(volumeInfo: VolumeInfo|Volume) {
  return isOneDriveId(volumeInfo?.providerId);
}

export function isOneDrivePlaceholderKey(key: FileKey|undefined) {
  if (!key) {
    return false;
  }
  return isOneDriveId(key.substr(key.lastIndexOf('/') + 1));
}

/**
 * Returns a boolean indicating whether the volume is a GuestOs volume. And
 * ANDROID_FILES type volume can also be a GuestOs volume if ARCVM is enabled.
 */
export function isGuestOs(type: VolumeType) {
  return type === VolumeType.GUEST_OS ||
      (type === VolumeType.ANDROID_FILES && isArcVmEnabled());
}

/**
 * Returns true if fileData's entry supports the "shared" feature, as in,
 * displays a shared icon. It's only supported inside "My Drive" or
 * "Computers", even Shared Drive does not support it, the "My Drive" and
 * "Computers" itself don't support it either, only their children.
 *
 * Note: if the return value is true, fileData's entry is guaranteed to be
 * native Entry type.
 */
export function shouldSupportDriveSpecificIcons(fileData: FileData): boolean {
  return (isEntryInsideMyDrive(fileData) && !!fileData.entry &&
          !isVolumeEntry(fileData.entry)) ||
      (isEntryInsideComputers(fileData) && !!fileData.entry &&
       !isGrandRootEntryInDrive(fileData.entry));
}

/**
 * Extracts the `entry` from the supplied `treeItem` depending on if the new
 * directory tree is enabled or not.
 */
export function getTreeItemEntry(treeItem: XfTreeItem|null|undefined): Entry|
    FilesAppEntry|null {
  if (!treeItem) {
    return null;
  }

  const item = treeItem as XfTreeItem;
  const state = getStore().getState();
  return getEntry(state, item.dataset['navigationKey']!);
}

/**
 * Check if the entry support `getUiChildren()` method.
 */
export function isEntrySupportUiChildren(entry: FilesAppEntry|Entry):
    entry is EntryList|VolumeEntry {
  return 'getUiChildren' in entry;
}

export function supportsUiChildren(fileData: FileData): boolean {
  return fileData.type === EntryType.ENTRY_LIST ||
      fileData.type === EntryType.VOLUME_ROOT;
}

/**
 * A generator version of `entry.readEntries()`.
 *
 * Example usage:
 * ```
 * const childEntries = []
 * for await (const partialEntries of readEntries(...)) {
     childEntries.push(...partialEntries);
  }
 * ```
 */
export async function*
    readEntries(entry: DirectoryEntry|FilesAppDirEntry):
        AsyncGenerator<Entry[], void, void> {
  const ls = (reader: DirectoryReader): Promise<Entry[]> => {
    return new Promise((resolve, reject) => {
      reader.readEntries(results => resolve(results), error => reject(error));
    });
  };

  const reader = entry.createReader();
  while (true) {
    const entries = await ls(reader);
    if (entries.length === 0) {
      break;
    }
    yield entries;
  }

  // The final return here is void.
}

/**
 * Check if the given entry is scannable or not, e.g. can we call
 * `readEntries()` on it.
 * If the return value is true, its type is guaranteed to be a Directory like
 * entry.
 */
export function isEntryScannable(entry: Entry|FilesAppEntry|null):
    entry is DirectoryEntry|FilesAppDirEntry {
  if (!entry) {
    return false;
  }
  if (!entry.isDirectory) {
    return false;
  }
  if ('disabled' in entry && entry.disabled) {
    return false;
  }
  const entryKeysWithoutChildren = new Set([
    recentRootKey,
    trashRootKey,
  ]);
  if (entryKeysWithoutChildren.has(entry.toURL())) {
    return false;
  }
  return true;
}

/**
 * Check if the given fileData can display sub-directories.
 */
export function canHaveSubDirectories(fileData: FileData|null) {
  if (!fileData) {
    return false;
  }
  if (!fileData.isDirectory) {
    return false;
  }
  if (fileData.disabled) {
    return false;
  }

  const entryKeysWithoutChildren = new Set([
    recentRootKey,
    trashRootKey,
  ]);

  if (entryKeysWithoutChildren.has(fileData.key)) {
    return false;
  }

  return true;
}

/**
 * Determines if the given entry can be deleted, considering read-only status
 * and SkyVault.
 */
export function isReadOnlyForDelete(
    volumeManager: VolumeManager, entry: Entry|FilesAppEntry) {
  if (isNonModifiable(volumeManager, entry)) {
    return true;
  }

  const locationInfo = volumeManager.getLocationInfo(entry);
  const isReadOnly = locationInfo && locationInfo.isReadOnly;

  if (!isReadOnly || !isSkyvaultV2Enabled()) {
    // If not read-only, or if SkyVault is disabled, just return
    return isReadOnly;
  }
  // Else, further checks are needed
  const volumeInfo = locationInfo.volumeInfo;
  if (!volumeInfo) {
    return isReadOnly;
  }

  // Allow deletion even if read-only, when:
  //  - local storage is disabled
  //  - the volume is in MyFiles or Downloads
  const state = getStore().getState();
  const localUserFilesAllowed = state.preferences?.localUserFilesAllowed;
  if (!localUserFilesAllowed &&
      (volumeInfo.volumeType === VolumeType.DOWNLOADS ||
       volumeInfo.volumeType === VolumeType.MY_FILES)) {
    return false;
  }
  return isReadOnly;
}
