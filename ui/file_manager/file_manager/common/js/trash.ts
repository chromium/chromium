// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Trash implementation is based on
 * https://specifications.freedesktop.org/trash-spec/trashspec-1.0.html.
 *
 * When you move /dir/hello.txt to trash, you get:
 *  .Trash/files/hello.txt
 *  .Trash/info/hello.trashinfo
 *
 * .Trash/files/hello.txt is the original file.  .Trash/files.hello.trashinfo is
 * a text file which looks like:
 *  [Trash Info]
 *  Path=/dir/hello.txt
 *  DeletionDate=2020-11-02T07:35:38.964Z
 *
 * TrashEntry combines both files for display.
 */

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';

import type {VolumeManager} from '../../background/js/volume_manager.js';
import type {FilesAppEntry} from '../../common/js/files_app_entry_types.js';

import {parseTrashInfoFiles, startIOTask} from './api.js';
import {isDirectoryEntry, isFileEntry} from './entry_utils.js';
import {FakeEntryImpl} from './files_app_entry_types.js';
import {recordMediumCount} from './metrics.js';
import {str} from './translations.js';
import {RootType, VolumeType} from './volume_manager_types.js';

/**
 * Configuration for where Trash is stored in a volume.
 */
export class TrashConfig {
  /**
   * The id representing this specific TrashConfig.
   */
  readonly id: string;

  constructor(
      readonly volumeType: VolumeType, readonly topDir: string,
      readonly trashDir: string, readonly deleteIsForever: boolean) {
    this.id = `${volumeType}-${topDir}`;
  }
}

/**
 * Volumes supported for Trash, and location of Trash dir. Items will be
 * searched in order.
 */
export const TRASH_CONFIG = [
  // MyFiles/Downloads is a separate volume on a physical device, and doing a
  // move from MyFiles/Downloads/<path> to MyFiles/.Trash actually does a
  // copy across volumes, so we have a dedicated MyFiles/Downloads/.Trash.
  new TrashConfig(
      VolumeType.DOWNLOADS, '/Downloads/', '/Downloads/.Trash/',
      /*deleteIsForever=*/ true),
  new TrashConfig(
      VolumeType.DOWNLOADS, '/', '/.Trash/',
      /*deleteIsForever=*/ true),
];

if (loadTimeData.getBoolean('FILES_TRASH_DRIVE_ENABLED')) {
  TRASH_CONFIG.push(new TrashConfig(
      VolumeType.DRIVE, '/', '/.Trash-1000/',
      /*deleteIsForever=*/ false));
}

/**
 * Interval (ms) until items in trash are permanently deleted. 30 days.
 */
export const AUTO_DELETE_INTERVAL_MS = 30 * 24 * 60 * 60 * 1000;

/**
 * Interval (ms) when .trashinfo files with no related files entry can be
 * considered stale and should be removed. 1 hour.
 */
const STALE_TRASHINFO_INTERVAL_MS = 60 * 60 * 1000;

/**
 * Returns a list of strings that represent volumes that are enabled for Trash.
 * Used to validate drag drop data without resolving the URLs to Entry's.
 */
export function getEnabledTrashVolumeURLs(
    volumeManager: VolumeManager, includeTrashPath = false,
    deleteIsForeverOnly = false) {
  const urls: string[] = [];
  for (let i = 0; i < volumeManager.volumeInfoList.length; i++) {
    const volumeInfo = volumeManager.volumeInfoList.item(i);
    for (const config of TRASH_CONFIG) {
      if (deleteIsForeverOnly && !config.deleteIsForever) {
        continue;
      }
      if (volumeInfo.volumeType === config.volumeType) {
        if (!includeTrashPath) {
          urls.push(volumeInfo.fileSystem.root.toURL() as string);
          continue;
        }
        let fileSystemRootURL = volumeInfo.fileSystem.root.toURL() as string;
        if (fileSystemRootURL.endsWith('/')) {
          fileSystemRootURL =
              fileSystemRootURL.substring(0, fileSystemRootURL.length - 1);
        }
        urls.push(fileSystemRootURL + config.trashDir);
      }
    }
  }
  return urls;
}

/**
 * Returns true if all supplied entries reside at a known trash location.
 */
export function isAllTrashEntries(
    entries: FileSystemEntry[], volumeManager: VolumeManager): boolean {
  const enabledTrashVolumeURLs =
      getEnabledTrashVolumeURLs(volumeManager, /*includeTrashPath=*/ true);
  return entries.every((e: FileSystemEntry) => {
    for (const volumeURL of enabledTrashVolumeURLs) {
      if (e.toURL().startsWith(volumeURL)) {
        return true;
      }
    }
    return false;
  });
}

/**
 * Returns true if all supplied entries are on a volume where delete or empty
 * from trash will delete forever.
 */
export function deleteIsForever(
    entries: Array<Entry|FilesAppEntry>,
    volumeManager: VolumeManager): boolean {
  const enabledTrashVolumeURLs = getEnabledTrashVolumeURLs(
      volumeManager, /*includeTrashPath=*/ false,
      /*deleteIsForeverOnly=*/ true);
  return entries.every((e: Entry|FilesAppEntry) => {
    for (const volumeURL of enabledTrashVolumeURLs) {
      if (e.toURL().startsWith(volumeURL)) {
        return true;
      }
    }
    return false;
  });
}

/**
 * Returns true if all entries are on a trashable volume and they aren't already
 * trashed.
 */
export function shouldMoveToTrash(
    entries: Array<Entry|FilesAppEntry>,
    volumeManager: VolumeManager): boolean {
  const urls: Array<{volume: string, volumeAndTrashPath: string}> = [];
  for (let i = 0; i < volumeManager.volumeInfoList.length; i++) {
    const volumeInfo = volumeManager.volumeInfoList.item(i);
    for (const config of TRASH_CONFIG) {
      if (volumeInfo.volumeType === config.volumeType) {
        let fileSystemRootURL = volumeInfo.fileSystem.root.toURL();
        if (fileSystemRootURL.endsWith('/')) {
          fileSystemRootURL =
              fileSystemRootURL.substring(0, fileSystemRootURL.length - 1);
        }
        const trashURLs = {
          volume: volumeInfo.fileSystem.root.toURL(),
          volumeAndTrashPath: fileSystemRootURL + config.trashDir,
        };
        urls.push(trashURLs);
      }
    }
  }
  return entries.every(e => {
    let onAllowedVolume = false;
    for (const {volume, volumeAndTrashPath} of urls) {
      // All trash directories in configuration have a trailing slash, so if the
      // entry URL is a directory and doesn't have a trailing slash, add one to
      // ensure a .Trash directory doesn't show a "Move to trash" button.
      let entryURL = e.toURL();
      if (e.isDirectory && !entryURL.endsWith('/')) {
        entryURL = entryURL + '/';
      }
      if (entryURL.startsWith(volumeAndTrashPath)) {
        return false;
      }
      if (entryURL.startsWith(volume)) {
        onAllowedVolume = true;
      }
    }
    return onAllowedVolume;
  });
}

/**
 * Wrapper for /.Trash/files and /.Trash/info directories.
 */
export class TrashDirs {
  constructor(
      readonly files: FileSystemDirectoryEntry,
      readonly info: FileSystemDirectoryEntry) {}

  /**
   * Promise wrapper for FileSystemDirectoryEntry.getDirectory().
   */
  static getDirectory(
      dirEntry: FileSystemDirectoryEntry, path: string,
      create: boolean): Promise<FileSystemDirectoryEntry|null> {
    return new Promise((resolve) => {
      dirEntry.getDirectory(path, {create}, (entry: FileSystemEntry) => {
        resolve(entry as FileSystemDirectoryEntry);
      }, () => resolve(null));
    });
  }

  /**
   * Get trash dirs from file system as specified in config.
   */
  static async getTrashDirs(
      fileSystem: FileSystem, config: TrashConfig, create: boolean) {
    let trashRoot: FileSystemDirectoryEntry|null = fileSystem.root;
    const parts = config.trashDir.split('/');
    for (const part of parts) {
      if (part) {
        trashRoot = await TrashDirs.getDirectory(trashRoot, part, create);
        if (!trashRoot) {
          return null;
        }
      }
    }
    const files = await TrashDirs.getDirectory(trashRoot, 'files', create);
    const info = await TrashDirs.getDirectory(trashRoot, 'info', create);
    return files && info ? new TrashDirs(files, info) : null;
  }
}

/**
 * Represents a file moved to trash. Combines the info from both .Trash/info and
 * ./Trash/files.
 */
export class TrashEntry implements Entry {
  /**
   * The underlying filesystem for the volume the .Trash folder resides on.
   */
  readonly filesystem: FileSystem;

  /**
   * The trash root type.
   */
  readonly rootType = RootType.TRASH;

  /**
   * The type name of TrashEntry.
   */
  readonly typeName = 'TrashEntry';

  /**
   * True if the trashed item is a file, false otherwise.
   */
  readonly isFile: boolean;

  /**
   * True if the trashed item is a directory, false otherwise.
   */
  readonly isDirectory: boolean;

  /**
   * The full path of the trashed item.
   */
  readonly fullPath: string;

  constructor(
      readonly name: string, private deletionDate_: Date,
      readonly filesEntry: FileSystemEntry, readonly infoEntry: FileSystemEntry,
      readonly restoreEntry: FileSystemEntry) {
    this.filesystem = filesEntry.filesystem;
    this.fullPath = filesEntry.fullPath;
    this.isDirectory = filesEntry.isDirectory;
    this.isFile = filesEntry.isFile;
  }

  /**
   * Use filesEntry toURL() so this entry can be used as that file to view,
   * copy, etc.
   */
  // Adding suppression since this class implements FileSystemEntry from
  // https://developer.mozilla.org/en-US/docs/Web/API/FileSystemEntry
  // eslint-disable-next-line @typescript-eslint/naming-convention
  toURL(): string {
    return this.filesEntry.toURL();
  }

  /**
   * Pass through to getMetadata() of filesEntry, keep size, but use
   * DeletionDate from infoEntry for modificationTime.
   *
   * @override Entry
   */
  getMetadata(success: MetadataCallback, error: ErrorCallback) {
    this.filesEntry.getMetadata(m => {
      success({modificationTime: this.deletionDate_, size: m.size});
    }, error);
  }

  /**
   * Remove filesEntry first, then remove infoEntry. Overrides Entry.
   */
  remove(success: VoidCallback, error: ErrorCallback) {
    this.filesEntry.remove(() => this.infoEntry.remove(success, error), error);
  }

  /**
   * Pass through to filesEntry. Overrides FileEntry.
   */
  file(success: FileCallback, error: ErrorCallback) {
    if (isFileEntry(this.filesEntry)) {
      this.filesEntry.file(success, error);
      return;
    }
    console.error('file attempted on FileSystemDirectoryEntry');
  }

  /**
   * Pass through to filesEntry. Overrides DirectoryEntry.
   */
  getFile(
      path: string, options: FileSystemFlags, success: FileSystemEntryCallback,
      error: ErrorCallback) {
    if (isDirectoryEntry(this.filesEntry)) {
      this.filesEntry.getFile(path, options, success, error);
      return;
    }
    console.error('getFile attempted on FileSystemFileEntry');
  }

  /**
   * Remove filesEntry first, then remove infoEntry. Overrides DirectoryEntry.
   */
  removeRecursively(success: VoidCallback, error: ErrorCallback) {
    if (isDirectoryEntry(this.filesEntry)) {
      this.filesEntry.removeRecursively(
          () => this.infoEntry.remove(success, error), error);
      return;
    }
    console.error('removeRecursively attempted on FileSystemFileEntry');
  }

  /**
   * Trash entries should not allow the following methods, specifically `moveTo`
   * and `copyTo` should be handled by the restore IO task.
   */
  getParent() {}
  moveTo() {}
  copyTo() {}

  /**
   * We must set entry.isNativeType to true, so that this is not considered a
   * FakeEntry, and we are allowed to delete the item.
   */
  get isNativeType() {
    return true;
  }

  getNativeEntry() {
    return this.filesEntry;
  }
}

/**
 * Reads all entries in each of .Trash/info and .Trash/files and produces a
 * single stream of TrashEntry.
 */
class TrashDirectoryReader implements FileSystemDirectoryReader {
  /**
   * The entries that exist in this .Trash directory.
   */
  private filesEntries_: {[key: string]: FileSystemEntry} = {};

  /**
   * A directory reader used to read the items out of the .Trash/info directory.
   */
  private infoReader_: FileSystemDirectoryReader|null = null;

  constructor(private fileSystem_: FileSystem, private config_: TrashConfig) {}

  /**
   * Create a trash entry if infoEntry and matching files entry are valid, else
   * return null.
   */
  private createTrashEntry_(
      parsedEntry: chrome.fileManagerPrivate.ParsedTrashInfoFile,
      infoEntry: FileSystemEntry) {
    const filesEntry = this.getFilesEntry(parsedEntry.trashInfoFileName);

    // Ignore any .trashinfo file with no matching file entry.
    if (!filesEntry) {
      console.warn('Ignoring trash info file with no matching files entry');
      return null;
    }

    return new TrashEntry(
        parsedEntry.restoreEntry.name, new Date(parsedEntry.deletionDate),
        filesEntry, infoEntry, parsedEntry.restoreEntry);
  }

  /**
   * Returns the Entry from the cached files entries.
   */
  getFilesEntry(trashInfoFileName: string) {
    const filesEntry = this.filesEntries_[trashInfoFileName];
    delete this.filesEntries_[trashInfoFileName];
    return filesEntry;
  }

  /**
   * Async version of readEntries(). This function may be called multiple times
   * and returns an empty result to indicate end of stream.
   *
   * Reads all items in .Trash/files on first call and caches them. Then reads
   * 1 or more batches of infoReader until we have at least 1 valid result to
   * send, or reader is exhausted.
   */
  private async readEntriesAsync_(
      success: FileSystemEntriesCallback, error: ErrorCallback) {
    const ls = (reader:
                    FileSystemDirectoryReader): Promise<FileSystemEntry[]> => {
      return new Promise((resolve, reject) => {
        reader.readEntries(results => resolve(results), error => reject(error));
      });
    };

    // Read all of .Trash/files on first call.
    if (!this.infoReader_) {
      const trashDirs = await TrashDirs.getTrashDirs(
          this.fileSystem_, this.config_, /*create=*/ false);
      // If trash dirs do not yet exist, then return successful empty read.
      if (!trashDirs) {
        return success([]);
      }

      // Get all entries in trash/files.
      const filesReader = trashDirs.files.createReader();
      try {
        while (true) {
          const entries = await ls(filesReader);
          if (!entries.length) {
            break;
          }
          entries.forEach(
              entry => this.filesEntries_[entry.name + '.trashinfo'] = entry);
        }
      } catch (e: any) {
        console.warn('Error reading trash files entries', e);
        error(e);
        return;
      }

      this.infoReader_ = trashDirs.info.createReader();
    }

    // Consume infoReader which is initialized in the first call. Read from
    // .Trash/info until we have at least 1 result, or end of stream.
    const result: TrashEntry[] = [];
    const entriesToDelete: FileSystemEntry[] = [];
    const dateNow = Date.now();
    while (true) {
      let entries: FileSystemEntry[] = [];
      try {
        entries = await ls(this.infoReader_);
      } catch (e: any) {
        console.warn('Error reading trash info entries', e);
        error(e);
        return;
      }
      if (!entries.length) {
        break;
      }
      const infoEntryMap: {[key: string]: FileSystemEntry} = {};
      for (const e of entries) {
        if (!e.isFile || !e.name.endsWith('.trashinfo')) {
          continue;
        }
        infoEntryMap[e.name] = e;
      }
      let parsedEntries: chrome.fileManagerPrivate.ParsedTrashInfoFile[] = [];
      try {
        parsedEntries = await parseTrashInfoFiles(entries);
      } catch (e: any) {
        console.warn('Error parsing trash info entries', e);
        error(e);
        return;
      }
      for (const parsedEntry of parsedEntries) {
        const infoEntry = infoEntryMap[parsedEntry.trashInfoFileName];
        if (!infoEntry) {
          continue;
        }
        // In the event the parsed entry was deleted more than 30 days ago,
        // schedule them for deletion and don't render them in the view.
        if (parsedEntry.deletionDate < (dateNow - AUTO_DELETE_INTERVAL_MS)) {
          entriesToDelete.push(infoEntry);
          const trashEntry = this.getFilesEntry(parsedEntry.trashInfoFileName);
          if (trashEntry) {
            entriesToDelete.push(trashEntry);
          }
          delete infoEntryMap[parsedEntry.trashInfoFileName];
          continue;
        }
        const trashEntry = this.createTrashEntry_(parsedEntry, infoEntry);
        if (trashEntry) {
          result.push(trashEntry);
        }
        delete infoEntryMap[parsedEntry.trashInfoFileName];
      }

      // Any leftover entries in the `infoEntryMap` have no corresponding file
      // entry. This can be due to 2 possible reasons:
      // 1. An in progress trash operation that has written the trashinfo file
      //    but not moved the corresponding item.
      // 2. The trashinfo has been removed or is dangling from a previously
      //    failed operation.
      // To avoid (1) check the `modificationDate` and ensure it's >1 hour old,
      // given a trash operation is atomic (no cross filesystem trashes) this
      // should be sufficient time to ensure there is no file to be moved.
      for (const entry of Object.values(infoEntryMap)) {
        let itemMetadata = null;
        try {
          itemMetadata = await getFileMetadata(entry);
        } catch (e) {
          console.warn('Error getting trashinfo metadata:', e);
          continue;
        }
        if (itemMetadata.modificationTime.getTime() <
            (dateNow - STALE_TRASHINFO_INTERVAL_MS)) {
          entriesToDelete.push(entry);
        }
      }
    }
    success(result);

    if (entriesToDelete.length > 0) {
      startIOTask(
          chrome.fileManagerPrivate.IoTaskType.DELETE, entriesToDelete, {
            showNotification: false,
            destinationFolder: undefined,
            password: undefined,
          });
    }

    // Record the amount of files seen for this particularly directory reader.
    recordMediumCount(
        /*name=*/ `TrashFiles.${this.config_.volumeType}`, result.length);
  }

  readEntries(success: FileSystemEntriesCallback, error: ErrorCallback) {
    this.readEntriesAsync_(success, error);
  }
}

/**
 * Root Trash entry sits inside "My files". It shows the combined entries of
 * trashes defined in TrashConfig.
 */
export class TrashRootEntry extends FakeEntryImpl {
  constructor() {
    super(str('TRASH_ROOT_LABEL'), RootType.TRASH);
  }
}

/**
 * Returns all the Trash directory readers.
 */
export function createTrashReaders(volumeManager: VolumeManager) {
  const readers: FileSystemDirectoryReader[] = [];
  TRASH_CONFIG.forEach(c => {
    const info = volumeManager.getCurrentProfileVolumeInfo(c.volumeType);
    if (info && info.fileSystem) {
      readers.push(new TrashDirectoryReader(info.fileSystem, c));
    }
  });
  return readers;
}

/**
 * Promisifies retrieval of a files metadata.
 */
async function getFileMetadata(file: FileSystemEntry): Promise<Metadata> {
  return new Promise((resolve, reject) => {
    file.getMetadata(resolve, reject);
  });
}

// The UMA to track the enum that is reported below.
export const RestoreFailedUMA = 'Trash.RestoreFailedNoParent';

export const RestoreFailedType = {
  // A single item has attempted to be restored but the parent has been removed.
  SINGLE_ITEM: 'single-item',

  // Multiple items have attempted to be restored where they all shared the same
  // parent folder, but it has been removed.
  MULTIPLE_ITEMS_SAME_PARENTS: 'multiple-items-same-parents',

  // Multiple items have attempted to be restored and they all have different
  // parent folders but all the parent folders have been removed.
  MULTIPLE_ITEMS_DIFFERENT_PARENTS: 'multiple-items-different-parents',

  // Multiple items have attempted to be restored from different parents with
  // some parent folders still existing and some have been removed.
  MULTIPLE_ITEMS_MIXED: 'multiple-items-mixed',
};

/**
 * Keep the order of this in sync with RestoreFailedNoParentType in
 * tools/metrics/histograms/enums.xml.
 */
export const RestoreFailedTypesUMA = [
  RestoreFailedType.SINGLE_ITEM,                       // 0
  RestoreFailedType.MULTIPLE_ITEMS_SAME_PARENTS,       // 1
  RestoreFailedType.MULTIPLE_ITEMS_DIFFERENT_PARENTS,  // 2
  RestoreFailedType.MULTIPLE_ITEMS_MIXED,              // 3
];
