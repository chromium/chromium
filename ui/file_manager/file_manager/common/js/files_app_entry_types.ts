// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Entry-like types for Files app UI.
 * This file defines the interface |FilesAppEntry| and some specialized
 * implementations of it.
 *
 * These entries are intended to behave like the browser native FileSystemEntry
 * (aka Entry) and FileSystemDirectoryEntry (aka DirectoryEntry), providing an
 * unified API for Files app UI components. UI components should be able to
 * display any implementation of FilesAppEntry.
 *
 * The main intention of those types is to be able to provide alternative
 * implementations and from other sources for "entries", as well as be able to
 * extend the native "entry" types.
 *
 * Native Entry:
 * https://developer.mozilla.org/en-US/docs/Web/API/FileSystemEntry
 * Native DirectoryEntry:
 * https://developer.mozilla.org/en-US/docs/Web/API/FileSystemDirectoryReader
 */

import type {VolumeInfo} from '../../background/js/volume_info.js';
import {oneDriveFakeRootKey} from '../../state/ducks/volumes.js';

import {isSameEntry} from './entry_utils.js';
import {vmTypeToIconName} from './icon_util.js';
import {getVolumeTypeFromRootType, RootType, VolumeType} from './volume_manager_types.js';

export interface Metadata {
  modificationTime: Date;
  size: number;
}

export type MetadataCallback = (a: Metadata) => void;
export type ErrorCallback = (e: Error) => void;
export type DomErrorCallback = (e: DOMError) => void;
export type FileErrorCallback = (e: FileError) => void;
export type FilesAppEntryCallback = (a: FilesAppEntry) => void;
export type FileEntryCallback = (a: FileEntry) => void;
export type FilesAppDirEntryCallback = (a: FilesAppDirEntry) => void;

// Generalized entry and directory entry definitions.
export type UniversalEntry = FilesAppEntry|Entry;
export type UniversalDirectory = FilesAppDirEntry|DirectoryEntry;

/**
 * FilesAppEntry represents a single Entry (file, folder or root) in the Files
 * app. Previously, we used the Entry type directly, but this limits the code to
 * only work with native Entry type which can't be instantiated in JS.
 * For now, Entry and FilesAppEntry should be used interchangeably.
 * See also FilesAppDirEntry for a folder-like interface.
 *
 * TODO(lucmult): Replace uses of Entry with FilesAppEntry implementations.
 */
export abstract class FilesAppEntry {
  constructor(public readonly rootType: RootType|null = null) {}

  /**
   * @returns the class name of this object. It's a workaround for the fact that
   * an instance created in the foreground page and sent to the background page
   * can't be checked with `instanceof`.
   */
  get typeName(): string {
    return 'FilesAppEntry';
  }

  /**
   * This attribute is defined on Entry.
   * @return true if this entry represents a Directory-like entry, as
   * in have sub-entries and implements {createReader} method.
   */
  get isDirectory(): boolean {
    return false;
  }

  /**
   * This attribute is defined on Entry.
   * @return true if this entry represents a File-like entry.
   * Implementations of FilesAppEntry are expected to have this as true.
   * Whereas implementations of FilesAppDirEntry are expected to have this as
   * false.
   */
  get isFile(): boolean {
    return true;
  }

  get filesystem(): FileSystem|null {
    return null;
  }

  /**
   * This attribute is defined on Entry.
   * @return absolute path from the file system's root to the entry. It can also
   * be thought of as a path which is relative to the root directory, prepended
   * with a "/" character.
   */
  get fullPath(): string {
    return '';
  }

  /**
   * This attribute is defined on Entry.
   * @return the name of the entry (the final part of the path, after the last.
   */
  get name(): string {
    return '';
  }

  /** This method is defined on Entry. */
  getParent(
      _success?: (DirectoryEntryCallback|FilesAppDirEntryCallback),
      error?: ErrorCallback): void {
    if (error) {
      setTimeout(error, 0, new Error('Not implemented'));
    }
  }

  /**
   * @return a string used to compare entries. It should return an unique
   * identifier for such entry, usually prefixed with it's root type like:
   * "fake-entry://unique/path/to/entry".
   * This method is defined on Entry.
   */
  // eslint-disable-next-line @typescript-eslint/naming-convention
  abstract toURL(): string;

  /** Gets metadata, such as "modificationTime" and "contentMimeType". */
  getMetadata(_success: MetadataCallback, error?: FileErrorCallback): void {
    if (error) {
      setTimeout(error, 0, new Error('Not implemented'));
    }
  }

  /**
   * Returns true if this entry object has a native representation such as Entry
   * or DirectoryEntry, this means it can interact with VolumeManager.
   */
  get isNativeType(): boolean {
    return false;
  }

  /**
   * Returns a FileSystemEntry if this instance has one, returns null if it
   * doesn't have or the entry hasn't been resolved yet. It's used to unwrap a
   * FilesAppEntry to be able to send to FileSystem API or fileManagerPrivate.
   */
  getNativeEntry(): Entry|null {
    return null;
  }

  copyTo(
      _newParent: DirectoryEntry|FilesAppDirEntry, _newName?: string,
      _success?: EntryCallback|FilesAppEntryCallback,
      error?: FileErrorCallback): void {
    if (error) {
      setTimeout(error, 0, new Error('Not implemented'));
    }
  }

  moveTo(
      _newParent: DirectoryEntry|FilesAppDirEntry, _newName: string,
      _success?: EntryCallback|FilesAppEntryCallback,
      error?: FileErrorCallback): void {
    if (error) {
      setTimeout(error, 0, new Error('Not implemented'));
    }
  }

  remove(
      _success: EntryCallback|FilesAppEntryCallback,
      error?: FileErrorCallback) {
    if (error) {
      setTimeout(error, 0, new Error('Not implemented'));
    }
  }
}

/**
 * Interface with minimal API shared among different types of FilesAppDirEntry
 * and native DirectoryEntry. UI components should be able to display any
 * implementation of FilesAppEntry.
 *
 * FilesAppDirEntry represents a DirectoryEntry-like (folder or root) in the
 * Files app. It's a specialization of FilesAppEntry extending the behavior for
 * folder, which is basically the method createReader.
 * As in FilesAppEntry, FilesAppDirEntry should be interchangeable with Entry
 * and DirectoryEntry.
 */
export abstract class FilesAppDirEntry extends FilesAppEntry {
  override get typeName(): string {
    return 'FilesAppDirEntry';
  }

  override get isDirectory() {
    return true;
  }

  override get isFile() {
    return false;
  }

  /**
   * @return Returns a reader compatible with DirectoryEntry.createReader (from
   * Web Standards) that reads the children of this instance.
   *
   * This method is defined on DirectoryEntry.
   */
  createReader(): DirectoryReader {
    return {} as DirectoryReader;
  }

  getFile(
      _path: string, _options?: FileSystemFlags,
      _success?: (FileEntryCallback|FilesAppEntryCallback),
      error?: DomErrorCallback) {
    if (error) {
      setTimeout(error, 0, new Error('Not implemented'));
    }
  }

  getDirectory(
      _path: string, _options?: FileSystemFlags,
      _success?: DirectoryEntryCallback|FilesAppDirEntryCallback,
      error?: DomErrorCallback) {
    if (error) {
      setTimeout(error, 0, new Error('Not implemented'));
    }
  }

  removeRecursively(_success: VoidCallback, error?: ErrorCallback) {
    if (error) {
      setTimeout(error, 0, new Error('Not implemented'));
    }
  }
}

/**
 * FakeEntry is used for entries that used only for UI, that weren't generated
 * by FileSystem API, like Drive, Downloads or Provided.
 */
export abstract class FakeEntry extends FilesAppDirEntry {
  /**
   * FakeEntry can be disabled if it represents the placeholder of the real
   * volume.
   */
  disabled: boolean = false;

  /**
   * @param label Translated text to be displayed to user.
   * @param rootType Root type of this entry. Used on Recents to filter the
   *    source of recent files/directories. Used on Recents to filter recent
   *    files by their file types.
   * @param sourceRestriction Used to communicate restrictions about sources to
   *   chrome.fileManagerPrivate.getRecentFiles API.
   * @param fileCategory Used to communicate category filter to
   *   chrome.fileManagerPrivate.getRecentFiles API.
   */
  constructor(
      public readonly label: string, rootType: RootType,
      public readonly sourceRestriction?:
          chrome.fileManagerPrivate.SourceRestriction,
      public fileCategory?: chrome.fileManagerPrivate.FileCategory) {
    super(rootType);
  }

  override get typeName() {
    return 'FakeEntry';
  }

  override get isDirectory() {
    return true;
  }

  override get isFile() {
    return false;
  }

  /** String used to determine the icon. */
  get iconName(): string {
    return '';
  }

  /**
   * FakeEntry can be a placeholder for the real volume, if so
   * this field will be the volume type of the volume it
   * represents.
   */
  get volumeType(): VolumeType|null {
    return null;
  }
}


/**
 * A reader compatible with DirectoryEntry.createReader (from Web Standards)
 * that reads a static list of entries, provided at construction time.
 * https://developer.mozilla.org/en-US/docs/Web/API/FileSystemDirectoryReader
 * It can be used by DirectoryEntry-like such as EntryList to return its
 * entries.
 */
export class StaticReader implements DirectoryReader {
  /**
   * @param entries_ Array of Entry-like instances that will be returned/read by
   * this reader.
   */
  constructor(private entries_: Array<Entry|FilesAppEntry>) {}

  /**
   * Reads array of entries via |success| callback.
   *
   * @param success A callback that will be called multiple times with the
   * entries, last call will be called with an empty array indicating that no
   * more entries available.
   * @param _error A callback that's never called, it's here to match the
   * signature from the Web Standards.
   */
  readEntries(success: EntriesCallback, _error?: FileErrorCallback) {
    const entries = this.entries_;
    // readEntries is suppose to return empty result when there are no more
    // files to return, so we clear the entries_ attribute for next call.
    this.entries_ = [];
    // Triggers callback asynchronously.
    setTimeout(() => success(entries as Entry[]), 0);
  }
}

/**
 * A reader compatible with DirectoryEntry.createReader (from Web Standards),
 * It chains entries from one reader to another, creating a combined set of
 * entries from all readers.
 */
export class CombinedReaders implements DirectoryReader {
  private currentReader_: DirectoryReader|undefined;

  /**
   * @param readers_ Array of all readers that will have their entries combined.
   */
  constructor(private readers_: DirectoryReader[]) {
    // Reverse readers_ so the readEntries can just use pop() to get the next.
    this.readers_.reverse();
    this.currentReader_ = this.readers_.pop();
  }

  /**
   * @param success returning entries of all readers, it's called with empty
   * Array when there is no more entries to return.
   * @param error called when error happens when reading from readers for this
   * implementation.
   */
  readEntries(success: EntriesCallback, error?: FileErrorCallback) {
    if (!this.currentReader_) {
      // If there is no more reader to consume, just return an empty result
      // which indicates that read has finished.
      success([]);
      return;
    }

    this.currentReader_.readEntries((results) => {
      if (results.length) {
        success(results);
      } else {
        // If there isn't no more readers, finish by calling success with no
        // results.
        if (!this.readers_.length) {
          success([]);
          return;
        }

        // Move to next reader and start consuming it.
        this.currentReader_ = this.readers_.pop();
        this.readEntries(success, error);
      }
    }, error as ErrorCallback);
  }
}


/**
 * EntryList, a DirectoryEntry-like object that contains entries. Initially used
 * to implement "My Files" containing VolumeEntry for "Downloads", "Linux
 * Files" and "Play Files".
 */
export class EntryList extends FilesAppDirEntry {
  /** Children entries of this EntryList instance. */
  private children_: Array<Entry|FilesAppEntry> = [];

  /**
   * EntryList can be a placeholder of a real volume (e.g. MyFiles or
   * DriveFakeRootEntryList). It can be disabled if the corresponding volume
   * type is disabled.
   */
  disabled = false;

  /**
   * @param label: Label to be used when displaying to user, it should
   *    already translated.
   * @param rootType root type.
   * @param devicePath Path belonging to the external media device. Partitions
   * on the same external drive have the same device path.
   */
  constructor(
      public readonly label: string, rootType: RootType,
      public readonly devicePath: string = '') {
    super(rootType);
  }

  override get typeName() {
    return 'EntryList';
  }

  override get isDirectory() {
    return true;
  }

  override get isFile() {
    return false;
  }

  override get fullPath() {
    return '/';
  }

  /**
   * @return List of entries that are shown as
   *     children of this Volume in the UI, but are not actually entries of the
   *     Volume.  E.g. 'Play files' is shown as a child of 'My files'.
   */
  getUiChildren(): Array<Entry|FilesAppEntry> {
    return this.children_;
  }

  override get name() {
    return this.label;
  }

  override get isNativeType() {
    return false;
  }

  override getMetadata(success: MetadataCallback, _error?: FileErrorCallback) {
    // Defaults modificationTime to current time just to have a valid value.
    setTimeout(() => success({modificationTime: new Date(), size: 0}), 0);
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override toURL(): string {
    let url = `entry-list://${this.rootType}`;
    if (this.devicePath) {
      url += `/${this.devicePath}`;
    }
    return url;
  }

  override getParent(
      success?: (arg: DirectoryEntry|FilesAppDirEntry) => void,
      _error?: (e: Error) => void) {
    if (success) {
      setTimeout(() => success(this), 0);
    }
  }

  /**
   * @param entry that should be added as
   * child of this EntryList.
   * This method is specific to EntryList instance.
   */
  addEntry(entry: Entry|FilesAppEntry) {
    this.children_.push(entry);
    // Only VolumeEntry can have prefix set because it sets on VolumeInfo,
    // which is then used on LocationInfo/PathComponent.
    const volumeEntry: VolumeEntry = entry as VolumeEntry;
    if (volumeEntry.typeName === 'VolumeEntry') {
      volumeEntry.setPrefix(this);
    }
  }

  /**
   * @return Returns a reader compatible with
   * DirectoryEntry.createReader (from Web Standards) that reads the children of
   * this EntryList instance.
   * This method is defined on DirectoryEntry.
   */
  override createReader(): DirectoryReader {
    return new StaticReader(this.children_);
  }

  /**
   * This method is specific to VolumeEntry/EntryList instance.
   * Note: we compare the volumeId instead of the whole volumeInfo reference
   * because the same volume could be mounted multiple times and every time a
   * new volumeInfo is created.
   * @return index of entry on this EntryList or -1 if not found.
   */
  findIndexByVolumeInfo(volumeInfo: VolumeInfo): number {
    return this.children_.findIndex(
        childEntry => (childEntry as VolumeEntry).volumeInfo ?
            (childEntry as VolumeEntry).volumeInfo.volumeId ===
                volumeInfo.volumeId :
            false,
    );
  }

  /**
   * Removes the first volume with the given type.
   * @param volumeType desired type.
   * This method is specific to VolumeEntry/EntryList instance.
   * @return if entry was removed.
   */
  removeByVolumeType(volumeType: VolumeType): boolean {
    const childIndex = this.children_.findIndex(childEntry => {
      const volumeInfo = (childEntry as VolumeEntry).volumeInfo;
      return volumeInfo && volumeInfo.volumeType === volumeType;
    });
    if (childIndex !== -1) {
      this.children_.splice(childIndex, 1);
      return true;
    }
    return false;
  }

  /**
   * Removes all entries that match the rootType.
   * @param rootType to be removed.
   * This method is specific to VolumeEntry/EntryList instance.
   */
  removeAllByRootType(rootType: RootType) {
    this.children_ = this.children_.filter(
        entry => (entry as FilesAppEntry).rootType !== rootType);
  }

  /**
   * Removes all entries that match the volumeType.
   * @param volumeType to be removed.
   * This method is specific to VolumeEntry/EntryList instance.
   */
  removeAllByVolumeType(volumeType: VolumeType) {
    this.children_ = this.children_.filter(
        entry => (entry as VolumeEntry).volumeType !== volumeType);
  }

  /**
   * Removes the entry.
   * @param entry to be removed.
   * This method is specific to EntryList and VolumeEntry instance.
   * @return true if entry was removed.
   */
  removeChildEntry(entry: Entry|FilesAppEntry): boolean {
    const childIndex =
        this.children_.findIndex(childEntry => isSameEntry(childEntry, entry));
    if (childIndex !== -1) {
      this.children_.splice(childIndex, 1);
      return true;
    }
    return false;
  }

  removeAllChildren() {
    this.children_ = [];
  }

  override getNativeEntry() {
    return null;
  }

  /**
   * EntryList can be a placeholder for the real volume (e.g. MyFiles or
   * DriveFakeRootEntryList), if so this field will be the volume type of the
   * volume it represents.
   */
  get volumeType(): VolumeType|null {
    switch (this.rootType) {
      case RootType.MY_FILES:
        return VolumeType.DOWNLOADS;
      case RootType.DRIVE_FAKE_ROOT:
        return VolumeType.DRIVE;
      default:
        return null;
    }
  }
}

/**
 * A DirectoryEntry-like which represents a Volume, based on VolumeInfo.
 *
 * It uses composition to behave like a DirectoryEntry and proxies some calls
 * to its VolumeInfo instance.
 *
 * It's used to be able to add a volume as child of |EntryList| and make volume
 * displayable on file list.
 */
export class VolumeEntry extends FilesAppDirEntry {
  /**
   * Additional entries that will be displayed together with this Volume's
   * entries.
   */
  private children_: Array<Entry|FilesAppEntry> = [];
  private rootEntry_: DirectoryEntry;
  disabled = false;

  /** @param volumeInfo VolumeInfo for this entry. */
  constructor(public readonly volumeInfo: VolumeInfo) {
    super();

    this.rootEntry_ = this.volumeInfo.displayRoot;
    if (!this.rootEntry_) {
      this.volumeInfo.resolveDisplayRoot((displayRoot: DirectoryEntry) => {
        this.rootEntry_ = displayRoot;
      });
    }
  }

  override get typeName() {
    return 'VolumeEntry';
  }

  get volumeType(): VolumeType {
    return this.volumeInfo.volumeType;
  }

  override get filesystem(): FileSystem|null {
    return this.rootEntry_ ? this.rootEntry_.filesystem : null;
  }

  /**
   * @return List of entries that are shown as
   *     children of this Volume in the UI, but are not
   * actually entries of the Volume.  E.g. 'Play files' is
   * shown as a child of 'My files'.  Use createReader to find
   * real child entries of the Volume's filesystem.
   */
  getUiChildren(): Array<Entry|FilesAppEntry> {
    return this.children_;
  }

  override get fullPath(): string {
    return this.rootEntry_ ? this.rootEntry_.fullPath : '';
  }

  override get isDirectory() {
    return this.rootEntry_ ? this.rootEntry_.isDirectory : true;
  }

  override get isFile() {
    return this.rootEntry_ ? this.rootEntry_.isFile : false;
  }

  /**
   * @see https://github.com/google/closure-compiler/blob/mastexterns/browser/fileapi.js
   * @param path Entry fullPath.
   */
  override getDirectory(
      path: string, options?: FileSystemFlags, success?: DirectoryEntryCallback,
      error?: FileErrorCallback) {
    if (!this.rootEntry_) {
      if (error) {
        setTimeout(
            () => error(new Error('Root entry not resolved yet') as FileError),
            0);
      }
      return;
    }

    this.rootEntry_.getDirectory(
        path, options, success, error as (ErrorCallback | undefined));
  }

  /**
   * @see https://github.com/google/closure-compiler/blob/mastexterns/browser/fileapi.js
   */
  override getFile(
      path: string, options?: FileSystemFlags, success?: FileEntryCallback,
      error?: FileErrorCallback) {
    if (!this.rootEntry_) {
      if (error) {
        setTimeout(
            () => error(new Error('Root entry not resolved yet') as FileError),
            0);
      }
      return;
    }

    this.rootEntry_.getFile(
        path, options, success, error as (ErrorCallback | undefined));
  }

  override get name(): string {
    return this.volumeInfo.label;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override toURL(): string {
    return this.rootEntry_?.toURL() ?? '';
  }

  /** String used to determine the icon. */
  get iconName(): string {
    if (this.volumeInfo.volumeType === VolumeType.GUEST_OS) {
      return vmTypeToIconName(this.volumeInfo.vmType);
    }
    if (this.volumeInfo.volumeType === VolumeType.DOWNLOADS) {
      return VolumeType.MY_FILES;
    }
    return this.volumeInfo.volumeType;
  }

  /**
   * callback, it returns itself since EntryList is intended to be used as
   * root node and the Web Standard says to do so.
   * @param _error callback, not used for this implementation.
   */
  override getParent(
      success?: (entry: DirectoryEntry|FilesAppDirEntry) => void,
      _error?: ErrorCallback) {
    if (success) {
      setTimeout(() => success(this), 0);
    }
  }

  override getMetadata(success: MetadataCallback, error?: FileErrorCallback) {
    this.rootEntry_.getMetadata(success, error as ErrorCallback);
  }

  override get isNativeType() {
    return true;
  }

  override getNativeEntry(): FileSystemDirectoryEntry {
    return this.rootEntry_;
  }

  /**
   * @return Returns a reader from root entry, which is compatible with
   * DirectoryEntry.createReader (from Web Standards). This method is defined on
   * DirectoryEntry.
   */
  override createReader(): DirectoryReader {
    const readers = [];
    if (this.rootEntry_) {
      readers.push(this.rootEntry_.createReader());
    }

    if (this.children_.length) {
      readers.push(new StaticReader(this.children_));
    }

    return new CombinedReaders(readers);
  }

  /**
   * @param entry An entry to be used as prefix of this instance on breadcrumbs
   *     path, e.g. "My Files > Downloads", "My Files" is a prefixEntry on
   *     "Downloads" VolumeInfo.
   */
  setPrefix(entry: FilesAppEntry) {
    this.volumeInfo.prefixEntry = entry;
  }

  /**
   * @param entry that should be added as child of this VolumeEntry. This method
   * is specific to VolumeEntry instance.
   */
  addEntry(entry: Entry|FilesAppEntry) {
    this.children_.push(entry);
    // Only VolumeEntry can have prefix set because it sets on
    // VolumeInfo, which is then used on
    // LocationInfo/PathComponent.
    const volumeEntry = entry as unknown as VolumeEntry;
    if (volumeEntry.typeName === 'VolumeEntry') {
      volumeEntry.setPrefix(this);
    }
  }

  /**
   *     that's desired to be removed.
   * This method is specific to VolumeEntry/EntryList instance.
   * Note: we compare the volumeId instead of the whole volumeInfo reference
   * because the same volume could be mounted multiple times and every time a
   * new volumeInfo is created.
   * @return index of entry within VolumeEntry or -1 if not found.
   */
  findIndexByVolumeInfo(volumeInfo: VolumeInfo): number {
    return this.children_.findIndex(
        childEntry =>
            (childEntry as unknown as VolumeEntry).volumeInfo?.volumeId ===
            volumeInfo.volumeId);
  }

  /**
   * Removes the first volume with the given type.
   * @param volumeType desired type.
   * This method is specific to VolumeEntry/EntryList instance.
   * @return if entry was removed.
   */
  removeByVolumeType(volumeType: VolumeType): boolean {
    const childIndex = this.children_.findIndex(childEntry => {
      return (childEntry as unknown as VolumeEntry).volumeInfo?.volumeType ===
          volumeType;
    });

    if (childIndex !== -1) {
      this.children_.splice(childIndex, 1);
      return true;
    }

    return false;
  }

  /**
   * Removes all entries that match the rootType.
   * @param rootType to be removed.
   * This method is specific to VolumeEntry/EntryList instance.
   */
  removeAllByRootType(rootType: RootType) {
    this.children_ = this.children_.filter(
        entry => (entry as FilesAppEntry).rootType !== rootType);
  }

  /**
   * Removes all entries that match the volumeType.
   * @param volumeType to be removed.
   * This method is specific to VolumeEntry/EntryList instance.
   */
  removeAllByVolumeType(volumeType: VolumeType) {
    this.children_ = this.children_.filter(
        entry => (entry as unknown as VolumeEntry).volumeType !== volumeType);
  }

  /**
   * Removes the entry.
   * @param entry to be removed.
   * This method is specific to EntryList and VolumeEntry instance.
   * @return if entry was removed.
   */
  removeChildEntry(entry: Entry|FilesAppEntry): boolean {
    const childIndex =
        this.children_.findIndex(childEntry => isSameEntry(childEntry, entry));
    if (childIndex !== -1) {
      this.children_.splice(childIndex, 1);
      return true;
    }
    return false;
  }
}

/**
 * FakeEntry is used for entries that used only for UI, that weren't generated
 * by FileSystem API, like Drive, Downloads or Provided.
 */
export class FakeEntryImpl extends FakeEntry {
  /**
   * @param label Translated text to be displayed to user.
   * @param rootType Root type of this entry. used on Recents to filter the
   *    source of recent files/directories. used on Recents to filter recent
   *    files by their file types.
   * @param sourceRestriction Used to communicate restrictions about sources to
   * chrome.fileManagerPrivate.getRecentFiles API.
   * @param fileCategory Used to communicate file-type filter to
   * chrome.fileManagerPrivate.getRecentFiles API.
   */
  constructor(
      label: string, rootType: RootType,
      sourceRestriction?: chrome.fileManagerPrivate.SourceRestriction,
      fileCategory?: chrome.fileManagerPrivate.FileCategory) {
    super(label, rootType, sourceRestriction, fileCategory);
  }

  override get name(): string {
    return this.label;
  }

  override get fullPath(): string {
    return '/';
  }

  /**
   * FakeEntry is used as root, so doesn't have a parent and should return
   * itself. callback, it returns itself since EntryList is intended to be used
   * as root node and the Web Standard says to do so.
   * @param _error callback, not used for this implementation.
   */
  override getParent(
      success?: (entry: (DirectoryEntry|FilesAppDirEntry)) => void,
      _error?: ErrorCallback) {
    if (success) {
      setTimeout(() => success(this), 0);
    }
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override toURL(): string {
    let url = `fake-entry://${this.rootType}`;
    if (this.fileCategory) {
      url += `/${this.fileCategory}`;
    }
    return url;
  }

  /**
   * @return List of entries that are shown as children of this Volume in the
   *     UI, but are not actually entries of the Volume.  E.g. 'Play files' is
   *     shown as a child of 'My files'.
   */
  getUiChildren(): Array<Entry|FilesAppEntry> {
    return [];
  }

  /** String used to determine the icon. */
  override get iconName(): string {
    // When Drive volume isn't available yet, the
    // FakeEntry should show the "drive" icon.
    if (this.rootType === RootType.DRIVE_FAKE_ROOT) {
      return RootType.DRIVE;
    }

    return this.rootType ?? '';
  }

  override getMetadata(success: MetadataCallback, _error?: FileErrorCallback) {
    setTimeout(() => success({modificationTime: new Date(), size: 0}), 0);
  }

  override get isNativeType() {
    return false;
  }

  override getNativeEntry() {
    return null;
  }

  /**
   * @return Returns a reader compatible with DirectoryEntry.createReader (from
   * Web Standards) that reads 0 entries.
   */
  override createReader(): DirectoryReader {
    return new StaticReader([]);
  }

  /**
   * FakeEntry can be a placeholder for the real volume, if so this field will
   * be the volume type of the volume it represents.
   */
  override get volumeType(): VolumeType|null {
    // Recent rootType has no corresponding volume
    // type, and it will throw error in the below
    // getVolumeTypeFromRootType() call, we need to
    // return null here.
    if (this.rootType === RootType.RECENT) {
      return null;
    }

    return getVolumeTypeFromRootType(this.rootType!);
  }
}

/**
 * GuestOsPlaceholder is used for placeholder entries in the UI, representing
 * Guest OSs (e.g. Crostini) that could be mounted but aren't yet.
 */
export class GuestOsPlaceholder extends FakeEntryImpl {
  /**
   * @param label Translated text to be displayed to user.
   * @param guest_id Id of the guest
   * @param vm_type Type of the underlying VM
   */
  constructor(
      label: string, public guest_id: number,
      public vm_type: chrome.fileManagerPrivate.VmType) {
    super(label, RootType.GUEST_OS);
  }

  override get typeName() {
    return 'GuestOsPlaceholder';
  }

  /** String used to determine the icon. */
  override get iconName(): string {
    return vmTypeToIconName(this.vm_type);
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override toURL() {
    return `fake-entry://guest-os/${this.guest_id}`;
  }

  override get volumeType() {
    if (this.vm_type === chrome.fileManagerPrivate.VmType.ARCVM) {
      return VolumeType.ANDROID_FILES;
    }
    return VolumeType.GUEST_OS;
  }
}

/**
 * OneDrivePlaceholder is used to represent OneDrive in the UI, before being
 * mounted and set up.
 */
export class OneDrivePlaceholder extends FakeEntryImpl {
  constructor(label: string) {
    super(label, RootType.PROVIDED);
  }

  override get typeName() {
    return 'OneDrivePlaceholder';
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override toURL() {
    return oneDriveFakeRootKey;
  }

  override get iconName(): string {
    // TODO(b/340168761): Use proper icon.
    return RootType.DRIVE;
  }
}
