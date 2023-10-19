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
 * The main intention of those types is to be able to provide alternative
 * implementations and from other sources for "entries", as well as be able to
 * extend the native "entry" types.
 *
 * Native Entry:
 * https://developer.mozilla.org/en-US/docs/Web/API/FileSystemEntry
 * Native DirectoryEntry:
 * https://developer.mozilla.org/en-US/docs/Web/API/FileSystemDirectoryReader
 */

import {FakeEntry, FilesAppDirEntry, FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';

import {vmTypeToIconName} from './icon_util.js';
import {VolumeManagerCommon} from './volume_manager_types.js';

/**
 * A reader compatible with DirectoryEntry.createReader (from Web Standards)
 * that reads a static list of entries, provided at construction time.
 * https://developer.mozilla.org/en-US/docs/Web/API/FileSystemDirectoryReader
 * It can be used by DirectoryEntry-like such as EntryList to return its
 * entries.
 * @extends {DirectoryReader}
 */
export class StaticReader {
  /**
   * @param {!Array<!Entry|!FilesAppEntry>} entries: Array of Entry-like
   * instances that will be returned/read by this reader.
   */
  constructor(entries) {
    this.entries_ = entries;
  }

  /**
   * Reads array of entries via |success| callback.
   *
   * @param {function(!Array<!Entry>):void} success: A callback that
   *     will be called multiple times with the entries, last call will be
   *     called with an empty array indicating that no more entries available.
   * @param {function(!FileError)=} _error: A callback that's never
   *     called, it's here to match the signature from the Web Standards.
   */
  readEntries(success, _error) {
    const entries = this.entries_;
    // readEntries is suppose to return empty result when there are no more
    // files to return, so we clear the entries_ attribute for next call.
    this.entries_ = [];
    // Triggers callback asynchronously.
    setTimeout(success, 0, entries);
  }
}

/**
 * A reader compatible with DirectoryEntry.createReader (from Web Standards),
 * It chains entries from one reader to another, creating a combined set of
 * entries from all readers.
 * @extends {DirectoryReader}
 */
export class CombinedReaders {
  /**
   * @param {!Array<!DirectoryReader>} readers Array of all readers that will
   * have their entries combined.
   */
  constructor(readers) {
    /**
     * @private @type {!Array<!DirectoryReader>} Reversed readers so the
     *     readEntries can just use pop() to get the next
     */
    this.readers_ = readers.reverse();

    /** @private @type {!DirectoryReader} */
    // @ts-ignore: error TS2322: Type 'DirectoryReader | undefined' is not
    // assignable to type 'DirectoryReader'.
    this.currentReader_ = readers.pop();
  }

  /**
   * @param {function(!Array<!Entry>):void} success returning entries
   *     of all readers, it's called with empty Array when there is no more
   *     entries to return.
   * @param {function(!FileError)=} error called when error happens when reading
   *    from readers.
   * for this implementation.
   */
  readEntries(success, error) {
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
        // @ts-ignore: error TS2322: Type 'DirectoryReader | undefined' is not
        // assignable to type 'DirectoryReader'.
        this.currentReader_ = this.readers_.pop();
        this.readEntries(success, error);
      }
      // @ts-ignore: error TS2345: Argument of type '((arg0: FileError) => any)
      // | undefined' is not assignable to parameter of type 'ErrorCallback |
      // undefined'.
    }, error);
  }
}


/**
 * EntryList, a DirectoryEntry-like object that contains entries. Initially used
 * to implement "My Files" containing VolumeEntry for "Downloads", "Linux
 * Files" and "Play Files".
 *
 * @implements FilesAppDirEntry
 */
export class EntryList {
  /**
   * @param {string} label: Label to be used when displaying to user, it should
   *    already translated.
   * @param {VolumeManagerCommon.RootType} rootType root type.
   * @param {string} devicePath Device path
   */
  constructor(label, rootType, devicePath = '') {
    /**
     * @private @type {string} label: Label to be used when displaying to user,
     *     it
     *      should be already translated.
     */
    this.label_ = label;

    /** @private @type {VolumeManagerCommon.RootType} rootType root type. */
    this.rootType_ = rootType;

    /**
     * @private @type {string} devicePath Path belonging to the external media
     * device. Partitions on the same external drive have the same device path.
     */
    this.devicePath_ = devicePath;

    /**
     * @private @type {!Array<!Entry|!FilesAppEntry>} children entries of
     * this EntryList instance.
     */
    this.children_ = [];

    this.isDirectory = true;
    this.isFile = false;
    this.type_name = 'EntryList';
    this.fullPath = '/';

    /**
     * @type {?FileSystem}
     */
    this.filesystem = null;

    /**
     * @public @type {boolean} EntryList can be a placeholder of a real volume
     * (e.g. MyFiles or DriveFakeRootEntryList), it can be disabled if the
     * corresponding volume type is disabled.
     */
    this.disabled = false;
  }

  /**
   * @return {!Array<!Entry|!FilesAppEntry>} List of entries that are shown as
   *     children of this Volume in the UI, but are not actually entries of the
   *     Volume.  E.g. 'Play files' is shown as a child of 'My files'.
   */
  getUIChildren() {
    return this.children_;
  }

  get label() {
    return this.label_;
  }

  get rootType() {
    return this.rootType_;
  }

  get name() {
    return this.label_;
  }

  get devicePath() {
    return this.devicePath_;
  }

  get isNativeType() {
    return false;
  }

  /**
   * @param {function({modificationTime: Date, size: number}): void} success
   * @param {function(FileError)=} _error
   */
  getMetadata(success, _error) {
    // Defaults modificationTime to current time just to have a valid value.
    setTimeout(() => success({modificationTime: new Date(), size: 0}));
  }

  /**
   * @return {string} used to compare entries.
   */
  toURL() {
    // There may be multiple entry lists. Append the device path to return
    // a unique identifiable URL for the entry list.
    if (this.devicePath_) {
      return 'entry-list://' + this.rootType + '/' + this.devicePath_;
    }
    return 'entry-list://' + this.rootType;
  }

  /**
   * @param {(function((DirectoryEntry|FilesAppDirEntry)):void)=} success
   * @param {function(Error)=} _error callback.
   */
  getParent(success, _error) {
    const self = /** @type {!FilesAppDirEntry} */ (this);
    setTimeout(() => success && success(self), 0, this);
  }

  /**
   * @param {!Entry|!FilesAppEntry} entry that should be added as
   * child of this EntryList.
   * This method is specific to EntryList instance.
   */
  addEntry(entry) {
    this.children_.push(entry);
    // Only VolumeEntry can have prefix set because it sets on VolumeInfo,
    // which is then used on LocationInfo/PathComponent.
    if (/** @type{FilesAppEntry} */ (entry).type_name == 'VolumeEntry') {
      const volumeEntry = /** @type {VolumeEntry} */ (entry);
      volumeEntry.setPrefix(this);
    }
  }

  /**
   * @return {!DirectoryReader} Returns a reader compatible with
   * DirectoryEntry.createReader (from Web Standards) that reads the children of
   * this EntryList instance.
   * This method is defined on DirectoryEntry.
   */
  createReader() {
    return new StaticReader(this.children_);
  }

  /**
   * @param {!import('../../externs/volume_info.js').VolumeInfo} volumeInfo
   *     that's desired to be removed.
   * This method is specific to VolumeEntry/EntryList instance.
   * Note: we compare the volumeId instead of the whole volumeInfo reference
   * because the same volume could be mounted multiple times and every time a
   * new volumeInfo is created.
   * @return {number} index of entry on this EntryList or -1 if not found.
   */
  findIndexByVolumeInfo(volumeInfo) {
    return this.children_.findIndex(
        childEntry =>
            /** @type {VolumeEntry} */ (childEntry).volumeInfo ?
            /** @type {VolumeEntry} */ (childEntry).volumeInfo.volumeId ===
                volumeInfo.volumeId :
            false,
    );
  }

  /**
   * Removes the first volume with the given type.
   * @param {!VolumeManagerCommon.VolumeType} volumeType desired type.
   * This method is specific to VolumeEntry/EntryList instance.
   * @return {boolean} if entry was removed.
   */
  removeByVolumeType(volumeType) {
    const childIndex = this.children_.findIndex(childEntry => {
      const volumeInfo = /** @type {VolumeEntry} */ (childEntry).volumeInfo;
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
   * @param {!VolumeManagerCommon.RootType} rootType to be removed.
   * This method is specific to VolumeEntry/EntryList instance.
   */
  removeAllByRootType(rootType) {
    this.children_ = this.children_.filter(
        entry => /** @type{FilesAppEntry} */ (entry).rootType !== rootType);
  }

  /**
   * Removes all entries that match the volumeType.
   * @param {!VolumeManagerCommon.VolumeType} volumeType to be removed.
   * This method is specific to VolumeEntry/EntryList instance.
   */
  removeAllByVolumeType(volumeType) {
    this.children_ = this.children_.filter(
        entry => /** @type {VolumeEntry} */ (entry).volumeType !== volumeType);
  }

  /**
   * Removes the entry.
   * @param {!Entry|FilesAppEntry} entry to be removed.
   * This method is specific to EntryList and VolumeEntry instance.
   * @return {boolean} if entry was removed.
   */
  removeChildEntry(entry) {
    const childIndex =
        this.children_.findIndex(childEntry => childEntry === entry);
    if (childIndex !== -1) {
      this.children_.splice(childIndex, 1);
      return true;
    }
    return false;
  }

  getNativeEntry() {
    return null;
  }

  /**
   * EntryList can be a placeholder for the real volume (e.g. MyFiles or
   * DriveFakeRootEntryList), if so this field will be the volume type of the
   * volume it represents.
   * @return {VolumeManagerCommon.VolumeType|null}
   */
  get volumeType() {
    switch (this.rootType) {
      case VolumeManagerCommon.RootType.MY_FILES:
        return VolumeManagerCommon.VolumeType.DOWNLOADS;
      case VolumeManagerCommon.RootType.DRIVE_FAKE_ROOT:
        return VolumeManagerCommon.VolumeType.DRIVE;
      default:
        return null;
    }
  }

  /**
   * @param {!DirectoryEntry|!FilesAppDirEntry} newParent
   * @param {string=} newName
   * @param {(function(Entry)|function(FilesAppEntry))=} success
   * @param {function(FileError)=} error
   */
  // @ts-ignore: error TS6133: 'error' is declared but its value is never read.
  copyTo(newParent, newName, success, error) {}

  /**
   * @param {!DirectoryEntry|!FilesAppDirEntry} newParent
   * @param {string} newName
   * @param {(function(Entry)|function(FilesAppEntry))=} success
   * @param {function(FileError)=} error
   */
  // @ts-ignore: error TS6133: 'error' is declared but its value is never read.
  moveTo(newParent, newName, success, error) {}

  /**
   * @param {function(Entry):void|function(FilesAppEntry):void} success
   * @param {function(FileError)=} error
   */
  // @ts-ignore: error TS6133: 'error' is declared but its value is never read.
  remove(success, error) {}

  /**
   * @param {string} path
   * @param {!FileSystemFlags=} options
   * @param {(function(!FileEntry)|function(!FilesAppEntry))=} success
   * @param {function(!FileError)=} error
   */
  // @ts-ignore: error TS6133: 'error' is declared but its value is never read.
  getFile(path, options, success, error) {}

  /**
   * @param {string} path
   * @param {!FileSystemFlags=} options
   * @param {(function(!DirectoryEntry)|function(!FilesAppDirEntry))=} success
   * @param {function(!FileError)=} error
   */
  // @ts-ignore: error TS6133: 'error' is declared but its value is never read.
  getDirectory(path, options, success, error) {}

  /**
   * @param {function():void} success
   * @param {function(!Error)=} error
   */
  // @ts-ignore: error TS6133: 'error' is declared but its value is never read.
  removeRecursively(success, error) {}
}

/**
 * A DirectoryEntry-like which represents a Volume, based on VolumeInfo.
 *
 * It uses composition to behave like a DirectoryEntry and proxies some calls
 * to its VolumeInfo instance.
 *
 * It's used to be able to add a volume as child of |EntryList| and make volume
 * displayable on file list.
 *
 * @implements FilesAppDirEntry
 */
export class VolumeEntry {
  /**
   * @param {!import('../../externs/volume_info.js').VolumeInfo} volumeInfo:
   *     VolumeInfo for this entry.
   */
  constructor(volumeInfo) {
    /**
     * @private @type {!import('../../externs/volume_info.js').VolumeInfo} holds
     *     a reference to VolumeInfo to delegate some
     * method calls to it.
     */
    this.volumeInfo_ = volumeInfo;

    /**
     * @private @type{!Array<!Entry|!FilesAppEntry>} additional entries that
     *     will be displayed together with this Volume's entries.
     */
    this.children_ = [];

    /** @type {DirectoryEntry} from Volume's root. */
    this.rootEntry_ = volumeInfo.displayRoot;
    if (!volumeInfo.displayRoot) {
      volumeInfo.resolveDisplayRoot(displayRoot => {
        this.rootEntry_ = displayRoot;
      });
    }
    this.type_name = 'VolumeEntry';

    // TODO(b/271485133): consider deriving this from volumeInfo. Setting
    // rootType here breaks some integration tests, e.g.
    // saveAsDlpRestrictedAndroid.
    /** @type {?VolumeManagerCommon.RootType} */
    this.rootType = null;

    this.disabled_ = false;
  }

  /**
   * @return {!import('../../externs/volume_info.js').VolumeInfo} for this
   *     entry. This method is only valid for
   * VolumeEntry instances.
   */
  get volumeInfo() {
    return this.volumeInfo_;
  }

  /** @return {!VolumeManagerCommon.VolumeType} */
  get volumeType() {
    return this.volumeInfo_.volumeType;
  }

  /**
   * @return {?FileSystem} FileSystem for this volume.
   * This method is defined on Entry.
   */
  get filesystem() {
    return this.rootEntry_ ? this.rootEntry_.filesystem : null;
  }

  /**
   * @return {!Array<!Entry|!FilesAppEntry>} List of entries that are shown as
   *     children of this Volume in the UI, but are not actually entries of the
   *     Volume.  E.g. 'Play files' is shown as a child of 'My files'.  Use
   *     createReader to find real child entries of the Volume's filesystem.
   */
  getUIChildren() {
    return this.children_;
  }

  /**
   * @return {string} Full path for this volume.
   * This method is defined on Entry.
   */
  get fullPath() {
    return this.rootEntry_ ? this.rootEntry_.fullPath : '';
  }
  get isDirectory() {
    // Defaults to true if root entry isn't resolved yet, because a VolumeEntry
    // is like a directory.
    return this.rootEntry_ ? this.rootEntry_.isDirectory : true;
  }
  get isFile() {
    // Defaults to false if root entry isn't resolved yet.
    return this.rootEntry_ ? this.rootEntry_.isFile : false;
  }

  /**
   * @return {boolean} if this entry is disabled. This method is only valid for
   * VolumeEntry instances.
   */
  get disabled() {
    return this.disabled_;
  }

  /**
   * Sets the disabled property. This method is only valid for
   * VolumeEntry instances.
   * @param {boolean} disabled
   */
  set disabled(disabled) {
    this.disabled_ = disabled;
  }

  /**
   * @see https://github.com/google/closure-compiler/blob/mastexterns/browser/fileapi.js
   * @param {string} path Entry fullPath.
   * @param {!FileSystemFlags=} options
   * @param {function(!DirectoryEntry):void=} success
   * @param {function(!FileError):void=} error
   */
  getDirectory(path, options, success, error) {
    if (!this.rootEntry_) {
      error && setTimeout(error, 0, new Error('root entry not resolved yet.'));
      return;
    }
    // @ts-ignore: error TS2769: No overload matches this call.
    this.rootEntry_.getDirectory(path, options, success, error);
  }

  /**
   * @see https://github.com/google/closure-compiler/blob/mastexterns/browser/fileapi.js
   * @param {string} path
   * @param {!FileSystemFlags=} options
   * @param {function(!FileEntry):void=} success
   * @param {function(!FileError):void=} error
   * @return {undefined}
   */
  getFile(path, options, success, error) {
    if (!this.rootEntry_) {
      error && setTimeout(error, 0, new Error('root entry not resolved yet.'));
      return;
    }
    // @ts-ignore: error TS2769: No overload matches this call.
    this.rootEntry_.getFile(path, options, success, error);
  }

  /**
   * @return {string} Name for this volume.
   */
  get name() {
    return this.volumeInfo_.label;
  }

  /**
   * @return {string}
   */
  toURL() {
    return this.rootEntry_ ? this.rootEntry_.toURL() : '';
  }

  /**
   * String used to determine the icon.
   * @return {string}
   */
  get iconName() {
    if (this.volumeInfo_.volumeType ==
        VolumeManagerCommon.VolumeType.GUEST_OS) {
      return vmTypeToIconName(this.volumeInfo_.vmType);
    }
    if (this.volumeInfo_.volumeType ==
        VolumeManagerCommon.VolumeType.DOWNLOADS) {
      return /** @type {string} */ (VolumeManagerCommon.VolumeType.MY_FILES);
    }
    return /** @type {string} */ (this.volumeInfo_.volumeType);
  }

  /**
   * @param {function((DirectoryEntry|FilesAppDirEntry)):void=} success
   *     callback, it returns itself since EntryList is intended to be used as
   * root node and the Web Standard says to do so.
   * @param {function(Error)=} _error callback, not used for this
   *     implementation.
   */
  getParent(success, _error) {
    const self = /** @type {!FilesAppDirEntry} */ (this);
    setTimeout(() => success && success(self), 0, this);
  }

  /**
   * @param {function({modificationTime: Date, size: number}): void} success
   * @param {function(FileError)=} error
   */
  getMetadata(success, error) {
    // @ts-ignore: error TS2345: Argument of type '((arg0: FileError) => any) |
    // undefined' is not assignable to parameter of type 'ErrorCallback |
    // undefined'.
    this.rootEntry_.getMetadata(success, error);
  }

  get isNativeType() {
    return true;
  }

  getNativeEntry() {
    return this.rootEntry_;
  }

  /**
   * @return {!DirectoryReader} Returns a reader from root entry, which is
   * compatible with DirectoryEntry.createReader (from Web Standards).
   * This method is defined on DirectoryEntry.
   */
  createReader() {
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
   * @param {!FilesAppEntry} entry An entry to be used as prefix of this
   *     instance on breadcrumbs path, e.g. "My Files > Downloads", "My Files"
   *     is a prefixEntry on "Downloads" VolumeInfo.
   */
  setPrefix(entry) {
    this.volumeInfo_.prefixEntry = entry;
  }

  /**
   * @param {!Entry|!FilesAppEntry} entry that should be added as
   * child of this VolumeEntry.
   * This method is specific to VolumeEntry instance.
   */
  addEntry(entry) {
    this.children_.push(entry);
    // Only VolumeEntry can have prefix set because it sets on VolumeInfo,
    // which is then used on LocationInfo/PathComponent.
    if (/** @type {!FilesAppEntry} */ (entry).type_name == 'VolumeEntry') {
      const volumeEntry = /** @type {VolumeEntry} */ (entry);
      volumeEntry.setPrefix(this);
    }
  }

  /**
   * @param {!import('../../externs/volume_info.js').VolumeInfo} volumeInfo
   *     that's desired to be removed.
   * This method is specific to VolumeEntry/EntryList instance.
   * Note: we compare the volumeId instead of the whole volumeInfo reference
   * because the same volume could be mounted multiple times and every time a
   * new volumeInfo is created.
   * @return {number} index of entry within VolumeEntry or -1 if not found.
   */
  findIndexByVolumeInfo(volumeInfo) {
    return this.children_.findIndex(
        childEntry =>
            /** @type {VolumeEntry} */ (childEntry).volumeInfo ?
            /** @type {VolumeEntry} */ (childEntry).volumeInfo.volumeId ===
                volumeInfo.volumeId :
            false,
    );
  }

  /**
   * Removes the first volume with the given type.
   * @param {!VolumeManagerCommon.VolumeType} volumeType desired type.
   * This method is specific to VolumeEntry/EntryList instance.
   * @return {boolean} if entry was removed.
   */
  removeByVolumeType(volumeType) {
    const childIndex = this.children_.findIndex(childEntry => {
      const entry = /** @type {VolumeEntry} */ (childEntry);
      return entry.volumeInfo && entry.volumeInfo.volumeType === volumeType;
    });
    if (childIndex !== -1) {
      this.children_.splice(childIndex, 1);
      return true;
    }
    return false;
  }

  /**
   * Removes all entries that match the rootType.
   * @param {!VolumeManagerCommon.RootType} rootType to be removed.
   * This method is specific to VolumeEntry/EntryList instance.
   */
  removeAllByRootType(rootType) {
    this.children_ = this.children_.filter(
        entry => /** @type {!FilesAppEntry} */ (entry).rootType !== rootType);
  }

  /**
   * Removes all entries that match the volumeType.
   * @param {!VolumeManagerCommon.VolumeType} volumeType to be removed.
   * This method is specific to VolumeEntry/EntryList instance.
   */
  removeAllByVolumeType(volumeType) {
    this.children_ = this.children_.filter(
        entry => /** @type {VolumeEntry} */ (entry).volumeType !== volumeType);
  }

  /**
   * Removes the entry.
   * @param {!Entry|FilesAppEntry} entry to be removed.
   * This method is specific to EntryList and VolumeEntry instance.
   * @return {boolean} if entry was removed.
   */
  removeChildEntry(entry) {
    const childIndex =
        this.children_.findIndex(childEntry => childEntry === entry);
    if (childIndex !== -1) {
      this.children_.splice(childIndex, 1);
      return true;
    }
    return false;
  }

  /**
   * @param {!DirectoryEntry|!FilesAppDirEntry} newParent
   * @param {string=} newName
   * @param {(function(Entry)|function(FilesAppEntry))=} success
   * @param {function(FileError)=} error
   */
  // @ts-ignore: error TS6133: 'error' is declared but its value is never read.
  copyTo(newParent, newName, success, error) {}

  /**
   * @param {!DirectoryEntry|!FilesAppDirEntry} newParent
   * @param {string} newName
   * @param {(function(!Entry)|function(!FilesAppEntry))=} success
   * @param {function(!FileError)=} error
   */
  // @ts-ignore: error TS6133: 'error' is declared but its value is never read.
  moveTo(newParent, newName, success, error) {}

  /**
   * @param {function(!Entry):void|function(!FilesAppEntry):void} success
   * @param {function(!FileError)=} error
   */
  // @ts-ignore: error TS6133: 'error' is declared but its value is never read.
  remove(success, error) {}

  /**
   * @param {function():void} success
   * @param {function(!Error)=} error
   */
  // @ts-ignore: error TS6133: 'error' is declared but its value is never read.
  removeRecursively(success, error) {}
}

/**
 * FakeEntry is used for entries that used only for UI, that weren't generated
 * by FileSystem API, like Drive, Downloads or Provided.
 *
 * @implements FakeEntry
 */
export class FakeEntryImpl {
  /**
   * @param {string} label Translated text to be displayed to user.
   * @param {!VolumeManagerCommon.RootType} rootType Root type of this entry.
   * @param {chrome.fileManagerPrivate.SourceRestriction=} opt_sourceRestriction
   *    used on Recents to filter the source of recent files/directories.
   * @param {chrome.fileManagerPrivate.FileCategory=} opt_fileCategory
   *    used on Recents to filter recent files by their file types.
   */
  constructor(label, rootType, opt_sourceRestriction, opt_fileCategory) {
    /**
     * @public @type {string} label: Label to be used when displaying to user,
     * it should be already translated.
     */
    this.label = label;

    /** @public @type {string} Name for this volume. */
    this.name = label;

    /** @public @type {!VolumeManagerCommon.RootType} */
    this.rootType = rootType;

    /** @public @type {boolean} true FakeEntry are always directory-like. */
    this.isDirectory = true;

    /** @public @type {boolean} false FakeEntry are always directory-like. */
    this.isFile = false;

    /**
     * @public @type {boolean} false FakeEntry can be disabled if it represents
     * the placeholder of the real volume.
     */
    this.disabled = false;

    /**
     * @public @type {chrome.fileManagerPrivate.SourceRestriction|undefined}
     * It's used to communicate restrictions about sources to
     * chrome.fileManagerPrivate.getRecentFiles API.
     */
    this.sourceRestriction = opt_sourceRestriction;

    /**
     * @public @type {chrome.fileManagerPrivate.FileCategory|undefined} It's
     * used to communicate file-type filter to
     * chrome.fileManagerPrivate.getRecentFiles API.
     */
    this.fileCategory = opt_fileCategory;

    /**
     * @public @type {string} the class name for this class. It's workaround for
     * the fact that an instance created on foreground page and sent to
     * background page can't be checked with "instanceof".
     */
    this.type_name = 'FakeEntry';

    this.fullPath = '/';

    /**
     * @type {?FileSystem}
     */
    this.filesystem = null;
  }

  /**
   * FakeEntry is used as root, so doesn't have a parent and should return
   * itself.
   * @param {(function((DirectoryEntry|FilesAppDirEntry)):void)=} success
   *     callback, it returns itself since EntryList is intended to be used as
   * root node and the Web Standard says to do so.
   * @param {function(Error)=} _error callback, not used for this
   *     implementation.
   */
  getParent(success, _error) {
    const self = /** @type {!FilesAppDirEntry} */ (this);
    setTimeout(() => success && success(self), 0, this);
  }

  toURL() {
    let url = 'fake-entry://' + this.rootType;
    if (this.fileCategory) {
      url += '/' + this.fileCategory;
    }
    return url;
  }

  /**
   * @return {!Array<!Entry|!FilesAppEntry>} List of entries that are shown as
   *     children of this Volume in the UI, but are not actually entries of the
   *     Volume.  E.g. 'Play files' is shown as a child of 'My files'.
   */
  getUIChildren() {
    return [];
  }

  /**
   * String used to determine the icon.
   * @return {string}
   */
  get iconName() {
    // When Drive volume isn't available yet, the FakeEntry should show the
    // "drive" icon.
    if (this.rootType === VolumeManagerCommon.RootType.DRIVE_FAKE_ROOT) {
      return /** @type {string}  */ (VolumeManagerCommon.RootType.DRIVE);
    }

    return /** @type{string} */ (this.rootType);
  }

  /**
   * @param {function({modificationTime: Date, size: number}): void} success
   * @param {function(FileError)=} _error
   */
  getMetadata(success, _error) {
    setTimeout(() => success({modificationTime: new Date(), size: 0}));
  }

  get isNativeType() {
    return false;
  }

  getNativeEntry() {
    return null;
  }

  /**
   * @return {!DirectoryReader} Returns a reader compatible with
   * DirectoryEntry.createReader (from Web Standards) that reads 0 entries.
   */
  createReader() {
    return new StaticReader([]);
  }

  /**
   * FakeEntry can be a placeholder for the real volume, if so this field will
   * be the volume type of the volume it represents.
   * @return {VolumeManagerCommon.VolumeType|null}
   */
  get volumeType() {
    // Recent rootType has no corresponding volume type, and it will throw error
    // in the below getVolumeTypeFromRootType() call, we need to return null
    // here.
    if (this.rootType === VolumeManagerCommon.RootType.RECENT) {
      return null;
    }
    return VolumeManagerCommon.getVolumeTypeFromRootType(this.rootType);
  }

  /**
   * @param {!DirectoryEntry|!FilesAppDirEntry} newParent
   * @param {string=} newName
   * @param {(function(Entry)|function(FilesAppEntry))=} success
   * @param {function(FileError)=} error
   */
  // @ts-ignore: error TS6133: 'error' is declared but its value is never read.
  copyTo(newParent, newName, success, error) {}

  /**
   * @param {!DirectoryEntry|!FilesAppDirEntry} newParent
   * @param {string} newName
   * @param {(function(Entry)|function(FilesAppEntry))=} success
   * @param {function(FileError)=} error
   */
  // @ts-ignore: error TS6133: 'error' is declared but its value is never read.
  moveTo(newParent, newName, success, error) {}

  /**
   * @param {function(Entry):void|function(FilesAppEntry):void} success
   * @param {function(FileError)=} error
   */
  // @ts-ignore: error TS6133: 'error' is declared but its value is never read.
  remove(success, error) {}

  /**
   * @param {string} path
   * @param {!FileSystemFlags=} options
   * @param {(function(!FileEntry)|function(!FilesAppEntry))=} success
   * @param {function(!FileError)=} error
   */
  // @ts-ignore: error TS6133: 'error' is declared but its value is never read.
  getFile(path, options, success, error) {}

  /**
   * @param {string} path
   * @param {!FileSystemFlags=} options
   * @param {(function(!DirectoryEntry)|function(!FilesAppDirEntry))=} success
   * @param {function(!FileError)=} error
   */
  // @ts-ignore: error TS6133: 'error' is declared but its value is never read.
  getDirectory(path, options, success, error) {}

  /**
   * @param {function():void} success
   * @param {function(!Error)=} error
   */
  // @ts-ignore: error TS6133: 'error' is declared but its value is never read.
  removeRecursively(success, error) {}
}

/**
 * GuestOsPlaceholder is used for placeholder entries in the UI, representing
 * Guest OSs (e.g. Crostini) that could be mounted but aren't yet.
 *
 * @implements FakeEntry
 */
export class GuestOsPlaceholder extends FakeEntryImpl {
  /**
   * @param {string} label Translated text to be displayed to user.
   * @param {number} guest_id Id of the guest
   * @param {!chrome.fileManagerPrivate.VmType} vm_type Type of the underlying
   *     VM
   */
  constructor(label, guest_id, vm_type) {
    super(label, VolumeManagerCommon.RootType.GUEST_OS, undefined, undefined);

    /**
     * @public @type {number} The id of this guest
     */
    this.guest_id = guest_id;

    /**
     * @public @type {string} the class name for this class. It's workaround for
     * the fact that an instance created on foreground page and sent to
     * background page can't be checked with "instanceof".
     */
    this.type_name = 'GuestOsPlaceholder';

    this.vm_type = vm_type;
  }

  /**
   * String used to determine the icon.
   * @return {string}
   * @override
   */
  get iconName() {
    return vmTypeToIconName(this.vm_type);
  }

  /** @override */
  toURL() {
    return `fake-entry://guest-os/${this.guest_id}`;
  }

  /** @override */
  get volumeType() {
    if (this.vm_type === chrome.fileManagerPrivate.VmType.ARCVM) {
      return VolumeManagerCommon.VolumeType.ANDROID_FILES;
    }
    return VolumeManagerCommon.VolumeType.GUEST_OS;
  }
}
