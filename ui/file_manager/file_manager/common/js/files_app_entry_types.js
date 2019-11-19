// Copyright 2018 The Chromium Authors. All rights reserved.
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

/**
 * FilesAppEntry represents a single Entry (file, folder or root) in the Files
 * app. Previously, we used the Entry type directly, but this limits the code to
 * only work with native Entry type which can't be instantiated in JS.
 * For now, Entry and FilesAppEntry should be used interchangeably.
 * See also FilesAppDirEntry for a folder-like interface.
 *
 * TODO(lucmult): Replace uses of Entry with FilesAppEntry implementations.
 *
 * @interface
 */
class FilesAppEntry {
  constructor() {
    /**
     * @public {boolean} true if this entry represents a Directory-like entry,
     * as in have sub-entries and implements {createReader} method.
     * This attribute is defined on Entry.
     */
    this.isDirectory = false;

    /**
     * @public {boolean} true if this entry represents a File-like entry.
     * Implementations of FilesAppEntry are expected to have this as |true|.
     * Whereas implementations of FilesAppDirEntry are expected to have this as
     * |false|.
     * This attribute is defined on Entry.
     */
    this.isFile = true;

    /**
     * @public {string} absolute path from the file system's root to the entry.
     * It can also be thought of as a path which is relative to the root
     * directory, prepended with a "/" character.
     * This attribute is defined on Entry.
     */
    this.fullPath = '';

    /**
     * @public {string} the name of the entry (the final part of the path,
     * after the last.
     * This attribute is defined on Entry.
     */
    this.name = '';

    /**
     * @public {string} the class name for this class. It's workaround for the
     * fact that an instance created on foreground page and sent to background
     * page can't be checked with "instanceof".
     */
    this.type_name = 'FilesAppEntry';

    /** @public {VolumeManagerCommon.RootType|null} */
    this.rootType = null;
  }

  /**
   * @param {function(Entry)|function(FilesAppEntry)} success callback.
   * @param {function(Entry)|function(FilesAppEntry)} error callback.
   * This method is defined on Entry.
   */
  getParent(success, error) {}

  /**
   * @return {string} used to compare entries. It should return an unique
   * identifier for such entry, usually prefixed with it's root type like:
   * "fake-entry://unique/path/to/entry".
   * This method is defined on Entry.
   */
  toURL() {}

  /**
   * Return metadata via |success| callback. Relevant metadata are
   * "modificationTime" and "contentMimeType".
   * @param {function(Object)} success callback to be called with the result
   * metadata.
   * @param {function(Object)} error callback to be called in case of error or
   * ignored if no error happened.
   */
  getMetadata(success, error) {}

  /**
   * Returns true if this entry object has a native representation such as Entry
   * or DirectoryEntry, this means it can interact with VolumeManager.
   * @return {boolean}
   */
  get isNativeType() {}

  /**
   * Returns a FileSystemEntry if this instance has one, returns null if it
   * doesn't have or the entry hasn't been resolved yet. It's used to unwrap a
   * FilesAppEntry to be able to send to FileSystem API or fileManagerPrivate.
   * @return {Entry}
   */
  getNativeEntry() {}
}

/**
 * A reader compatible with DirectoryEntry.createReader (from Web Standards)
 * that reads a static list of entries, provided at construction time.
 * https://developer.mozilla.org/en-US/docs/Web/API/FileSystemDirectoryReader
 * It can be used by DirectoryEntry-like such as EntryList to return its
 * entries.
 * @extends {DirectoryReader}
 */
class StaticReader {
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
   * @param {function(!Array<!Entry|!FilesAppEntry>)} success: A callback that
   *     will be called multiple times with the entries, last call will be
   *     called with an empty array indicating that no more entries available.
   * @param {function(!FileError)=} error: A callback that's never
   *     called, it's here to match the signature from the Web Standards.
   * @override
   */
  readEntries(success, error) {
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
class CombinedReaders {
  /**
   * @param {!Array<!DirectoryReader>} readers Array of all readers that will
   * have their entries combined.
   */
  constructor(readers) {
    /**
     * @private {!Array<!DirectoryReader>} Reversed readers so the readEntries
     *     can just use pop() to get the next
     */
    this.readers_ = readers.reverse();

    /** @private {!DirectoryReader} */
    this.currentReader_ = readers.pop();
  }

  /**
   * @param {function(!Array<!Entry|!FilesAppEntry>)} success returning entries
   *     of all readers, it's called with empty Array when there is no more
   *     entries to return.
   * @param {function(!FileError)=} error called when error happens when reading
   *    from readers.
   * for this implementation.
   * @override
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
        this.currentReader_ = this.readers_.pop();
        this.readEntries(success, error);
      }
    }, error);
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
 *
 * @interface
 */
class FilesAppDirEntry extends FilesAppEntry {
  constructor() {
    super();
    /**
     * @public {boolean} true if this entry represents a Directory-like entry,
     * as in have sub-entries and implements {createReader} method.
     * Implementations of FilesAppEntry are expected to have this as |true|.
     * This attribute is defined on Entry.
     */
    this.isDirectory = true;
    this.type_name = 'FilesAppDirEntry';
  }

  /**
   * @return {!DirectoryReader} Returns a reader compatible with
   * DirectoryEntry.createReader (from Web Standards) that reads the children of
   * this instance.
   * This method is defined on DirectoryEntry.
   */
  createReader() {}
}

/**
 * EntryList, a DirectoryEntry-like object that contains entries. Initially used
 * to implement "My Files" containing VolumeEntry for "Downloads", "Linux
 * Files" and "Play Files".
 *
 * @implements FilesAppDirEntry
 */
class EntryList {
  /**
   * @param {string} label: Label to be used when displaying to user, it should
   *    already translated.
   * @param {VolumeManagerCommon.RootType} rootType root type.
   * @param {string} devicePath Device path
   */
  constructor(label, rootType, devicePath = '') {
    /**
     * @private {string} label: Label to be used when displaying to user, it
     *      should be already translated.
     */
    this.label_ = label;

    /** @private {VolumeManagerCommon.RootType} rootType root type. */
    this.rootType_ = rootType;

    /**
     * @private {string} devicePath Path belonging to the external media
     * device. Partitions on the same external drive have the same device path.
     */
    this.devicePath_ = devicePath;

    /**
     * @private {!Array<!Entry|!FilesAppEntry>} children entries of
     * this EntryList instance.
     */
    this.children_ = [];

    this.isDirectory = true;
    this.isFile = false;
    this.type_name = 'EntryList';
    this.fullPath = '/';
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

  /** @override */
  get isNativeType() {
    return false;
  }

  /** @override */
  getMetadata(success, error) {
    // Defaults modificationTime to current time just to have a valid value.
    setTimeout(() => success({modificationTime: new Date()}));
  }

  /**
   * @return {string} used to compare entries.
   * @override
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
   * @param {function(Entry)|function(FilesAppEntry)} success callback, it
   * returns itself since EntryList is intended to be used as root node and the
   * Web Standard says to do so.
   * @param {function(Entry)|function(FilesAppEntry)} error callback, not used
   * for this implementation.
   *
   * @override
   */
  getParent(success, error) {
    setTimeout(success, 0, this);
  }

  /**
   * @param {!Entry|!FilesAppEntry} entry that should be added as
   * child of this EntryList.
   * This method is specific to EntryList instance.
   */
  addEntry(entry) {
    this.children_.push(entry);
    // Only VolumeEntry can have prefix set becuase it sets on VolumeInfo
    // which's then used on LocationInfo/LocationLine.
    if (entry.type_name == 'VolumeEntry') {
      const volumeEntry = /** @type {VolumeEntry} */ (entry);
      volumeEntry.setPrefix(this);
    }
  }

  /**
   * @return {!DirectoryReader} Returns a reader compatible with
   * DirectoryEntry.createReader (from Web Standards) that reads the children of
   * this EntryList instance.
   * This method is defined on DirectoryEntry.
   * @override
   */
  createReader() {
    return new StaticReader(this.children_);
  }

  /**
   * @param {!VolumeInfo} volumeInfo that's desired to be removed.
   * This method is specific to VolumeEntry/EntryList instance.
   * @return {number} index of entry on this EntryList or -1 if not found.
   */
  findIndexByVolumeInfo(volumeInfo) {
    return this.children_.findIndex(childEntry => {
      return /** @type {VolumeEntry} */ (childEntry).volumeInfo === volumeInfo;
    });
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
   * Removes the first entry that matches the rootType.
   * @param {!VolumeManagerCommon.RootType} rootType to be removed.
   * This method is specific to VolumeEntry/EntryList instance.
   * @return {boolean} if entry was removed.
   */
  removeByRootType(rootType) {
    const childIndex = this.children_.findIndex(
        childEntry => childEntry.rootType === rootType);
    if (childIndex !== -1) {
      this.children_.splice(childIndex, 1);
      return true;
    }
    return false;
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

  /** @override */
  getNativeEntry() {
    return null;
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
 *
 * @implements FilesAppDirEntry
 */
class VolumeEntry {
  /**
   * @param {!VolumeInfo} volumeInfo: VolumeInfo for this entry.
   */
  constructor(volumeInfo) {
    /**
     * @private {!VolumeInfo} holds a reference to VolumeInfo to delegate some
     * method calls to it.
     */
    this.volumeInfo_ = volumeInfo;

    /**
     * @private{!Array<!Entry|!FilesAppEntry>} additional entries that will be
     * displayed together with this Volume's entries.
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

    // TODO(lucmult): consider deriving this from volumeInfo.
    this.rootType = null;
  }

  /**
   * @return {!VolumeInfo} for this entry. This method is only valid for
   * VolumeEntry instances.
   */
  get volumeInfo() {
    return this.volumeInfo_;
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
   * @override.
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
   * @see https://github.com/google/closure-compiler/blob/master/externs/browser/fileapi.js
   * @param {string} path Entry fullPath.
   * @param {!FileSystemFlags=} options
   * @param {function(!DirectoryEntry)=} success
   * @param {function(!FileError)=} error
   */
  getDirectory(path, options, success, error) {
    if (!this.rootEntry_) {
      error && setTimeout(error, 0, new Error('root entry not resolved yet.'));
      return;
    }
    return this.rootEntry_.getDirectory(path, options, success, error);
  }

  /**
   * @see https://github.com/google/closure-compiler/blob/master/externs/browser/fileapi.js
   * @param {string} path
   * @param {!FileSystemFlags=} options
   * @param {function(!FileEntry)=} success
   * @param {function(!FileError)=} error
   * @return {undefined}
   */
  getFile(path, options, success, error) {
    if (!this.rootEntry_) {
      error && setTimeout(error, 0, new Error('root entry not resolved yet.'));
      return;
    }
    return this.rootEntry_.getFile(path, options, success, error);
  }

  /**
   * @return {string} Name for this volume.
   * @override.
   */
  get name() {
    return this.volumeInfo_.label;
  }

  /**
   * @return {string}
   * @override
   */
  toURL() {
    return this.rootEntry_ ? this.rootEntry_.toURL() : '';
  }

  /**
   * String used to determine the icon.
   * @return {string}
   */
  get iconName() {
    return this.volumeInfo_.volumeType;
  }

  /**
   * @param {function(Entry)|function(FilesAppEntry)} success callback, it
   * returns itself since EntryList is intended to be used as root node and the
   * Web Standard says to do so.
   * @param {function(Entry)|function(FilesAppEntry)} error callback, not used
   * for this implementation.
   *
   * @override
   */
  getParent(success, error) {
    setTimeout(success, 0, this);
  }

  /** @override */
  getMetadata(success, error) {
    this.rootEntry_.getMetadata(success, error);
  }

  /** @override */
  get isNativeType() {
    return true;
  }

  /** @override */
  getNativeEntry() {
    return this.rootEntry_;
  }

  /**
   * @return {!DirectoryReader} Returns a reader from root entry, which is
   * compatible with DirectoryEntry.createReader (from Web Standards).
   * This method is defined on DirectoryEntry.
   * @override
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
    // Only VolumeEntry can have prefix set becuase it sets on VolumeInfo
    // which's then used on LocationInfo/LocationLine.
    if (entry.type_name == 'VolumeEntry') {
      const volumeEntry = /** @type {VolumeEntry} */ (entry);
      volumeEntry.setPrefix(this);
    }
  }

  /**
   * @param {!VolumeInfo} volumeInfo that's desired to be removed.
   * This method is specific to VolumeEntry/EntryList instance.
   * @return {number} index of entry within VolumeEntry or -1 if not found.
   */
  findIndexByVolumeInfo(volumeInfo) {
    return this.children_.findIndex(
        childEntry =>
            /** @type {VolumeEntry} */ (childEntry).volumeInfo === volumeInfo);
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
   * Removes the first entry that matches the rootType.
   * @param {!VolumeManagerCommon.RootType} rootType to be removed.
   * This method is specific to VolumeEntry/EntryList instance.
   * @return {boolean} if entry was removed.
   */
  removeByRootType(rootType) {
    const childIndex = this.children_.findIndex(
        childEntry => childEntry.rootType === rootType);
    if (childIndex !== -1) {
      this.children_.splice(childIndex, 1);
      return true;
    }
    return false;
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
}

/**
 * FakeEntry is used for entries that used only for UI, that weren't generated
 * by FileSystem API, like Drive, Downloads or Provided.
 *
 * @implements FilesAppDirEntry
 */
class FakeEntry {
  /**
   * @param {string} label Translated text to be displayed to user.
   * @param {!VolumeManagerCommon.RootType} rootType Root type of this entry.
   * @param {chrome.fileManagerPrivate.SourceRestriction=} opt_sourceRestriction
   *    used on Recents to filter the source of recent files/directories.
   */
  constructor(label, rootType, opt_sourceRestriction) {
    /**
     * @public {string} label: Label to be used when displaying to user, it
     *      should be already translated.
     */
    this.label = label;

    /** @public {string} Name for this volume. */
    this.name = label;

    /** @public {!VolumeManagerCommon.RootType} */
    this.rootType = rootType;

    /** @public {boolean} true FakeEntry are always directory-like. */
    this.isDirectory = true;

    /** @public {boolean} false FakeEntry are always directory-like. */
    this.isFile = false;

    /**
     * @public {chrome.fileManagerPrivate.SourceRestriction|undefined} It's used
     * to communicate restrictions about sources to
     * chrome.fileManagerPrivate.getRecentFiles API.
     */
    this.sourceRestriction = opt_sourceRestriction;

    /**
     * @public {string} the class name for this class. It's workaround for the
     * fact that an instance created on foreground page and sent to background
     * page can't be checked with "instanceof".
     */
    this.type_name = 'FakeEntry';

    this.fullPath = '/';
  }

  /**
   * FakeEntry is used as root, so doesn't have a parent and should return
   * itself.
   *
   *  @override
   */
  getParent(success, error) {
    setTimeout(success, 0, this);
  }

  /** @override */
  toURL() {
    return 'fake-entry://' + this.rootType;
  }

  /**
   * String used to determine the icon.
   * @return {string}
   */
  get iconName() {
    return /** @type{string} */ (this.rootType);
  }

  /** @override */
  getMetadata(success, error) {
    setTimeout(() => success({}));
  }

  /** @override */
  get isNativeType() {
    return false;
  }

  /** @override */
  getNativeEntry() {
    return null;
  }

  /**
   * @return {!DirectoryReader} Returns a reader compatible with
   * DirectoryEntry.createReader (from Web Standards) that reads 0 entries.
   * @override
   */
  createReader() {
    return new StaticReader([]);
  }
}
