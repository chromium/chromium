// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Interfaces for the Files app Entry Types.
 */

import {RootType, VolumeType} from '../common/js/volume_manager_types.js';

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
export class FilesAppEntry {
  constructor() {
    /**
     * @public @type {boolean} true if this entry represents a Directory-like
     * entry, as in have sub-entries and implements {createReader} method. This
     * attribute is defined on Entry.
     */
    this.isDirectory = false;

    /**
     * @public @type {boolean} true if this entry represents a File-like entry.
     * Implementations of FilesAppEntry are expected to have this as |true|.
     * Whereas implementations of FilesAppDirEntry are expected to have this as
     * |false|.
     * This attribute is defined on Entry.
     */
    this.isFile = true;

    /**
     * @public @type {string} absolute path from the file system's root to the
     * entry. It can also be thought of as a path which is relative to the root
     * directory, prepended with a "/" character.
     * This attribute is defined on Entry.
     */
    this.fullPath = '';

    /**
     * @public @type {string} the name of the entry (the final part of the path,
     * after the last.
     * This attribute is defined on Entry.
     */
    this.name = '';

    /**
     * @public @type {string} the class name for this class. It's workaround for
     * the fact that an instance created on foreground page and sent to
     * background page can't be checked with "instanceof".
     */
    this.type_name = 'FilesAppEntry';

    /** @type {RootType|null} */
    this.rootType = null;

    /**
     * @type {?FileSystem}
     */
    this.filesystem = null;
  }

  /**
   * @param {(function(DirectoryEntry)|function(FilesAppDirEntry))=} success
   *     callback.
   * @param {function(Error)=} error callback.
   * This method is defined on Entry.
   */
  // @ts-ignore: error TS6133: 'error' is declared but its value is never read.
  getParent(success, error) {}

  /**
   * @return {string} used to compare entries. It should return an unique
   * identifier for such entry, usually prefixed with it's root type like:
   * "fake-entry://unique/path/to/entry".
   * This method is defined on Entry.
   */
  toURL() {
    return '';
  }

  /**
   * Return metadata via |success| callback. Relevant metadata are
   * "modificationTime" and "contentMimeType".
   * @param {function({modificationTime: Date, size: number}): void} _success
   *     callback to be called with the result metadata.
   * @param {function(FileError)=} _error callback to be called in case of error
   *     or ignored if no error happened.
   */
  getMetadata(_success, _error) {}

  /**
   * Returns true if this entry object has a native representation such as Entry
   * or DirectoryEntry, this means it can interact with VolumeManager.
   * @return {boolean}
   */
  // @ts-ignore: error TS2378: A 'get' accessor must return a value.
  get isNativeType() {
    return false;
  }

  /**
   * Returns a FileSystemEntry if this instance has one, returns null if it
   * doesn't have or the entry hasn't been resolved yet. It's used to unwrap a
   * FilesAppEntry to be able to send to FileSystem API or fileManagerPrivate.
   * @return {?Entry}
   */
  getNativeEntry() {
    return null;
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
export class FilesAppDirEntry extends FilesAppEntry {
  constructor() {
    super();
    /**
     * @public @type {boolean} true if this entry represents a Directory-like
     * entry, as in have sub-entries and implements {createReader} method.
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
  createReader() {
    return /** @type {DirectoryReader} */ ({});
  }

  /**
   * @param {string} path
   * @param {!FileSystemFlags=} options
   * @param {(function(!FileEntry)|function(!FilesAppEntry))=} success
   * @param {function(!DOMError):void=} error
   */
  // @ts-ignore: error TS6133: 'error' is declared but its value is never read.
  getFile(path, options, success, error) {}

  /**
   * @param {string} path
   * @param {!FileSystemFlags=} options
   * @param {(function(!DirectoryEntry):void|function(!FilesAppDirEntry):void)=}
   *     success
   * @param {function(!DOMError)=} error
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
 * FakeEntry is used for entries that used only for UI, that weren't generated
 * by FileSystem API, like Drive, Downloads or Provided.
 *
 * @interface
 */
export class FakeEntry extends FilesAppDirEntry {
  /**
   * @param {string} label Translated text to be displayed to user.
   * @param {!RootType} rootType Root type of this entry.
   * @param {chrome.fileManagerPrivate.SourceRestriction=} opt_sourceRestriction
   *    used on Recents to filter the source of recent files/directories.
   * @param {chrome.fileManagerPrivate.FileCategory=} opt_fileCategory
   *    used on Recents to filter recent files by their file types.
   */
  // @ts-ignore: error TS6133: 'opt_fileCategory' is declared but its value is
  // never read.
  constructor(label, rootType, opt_sourceRestriction, opt_fileCategory) {
    super();
    /**
     * @type {string} label: Label to be used when displaying to user, it
     *      should be already translated.
     */
    this.label;

    /** @type {string} Name for this volume. */
    this.name;

    /** @type {!RootType} */
    this.rootType;

    /** @type {boolean} true FakeEntry are always directory-like. */
    this.isDirectory = true;

    /** @type {boolean} false FakeEntry are always directory-like. */
    this.isFile = false;

    /**
     * @type {boolean} false FakeEntry can be disabled if it represents the
     * placeholder of the real volume.
     */
    this.disabled = false;

    /**
     * @type {chrome.fileManagerPrivate.SourceRestriction|undefined} It's used
     * to communicate restrictions about sources to
     * chrome.fileManagerPrivate.getRecentFiles API.
     */
    this.sourceRestriction;

    /**
     * @type {chrome.fileManagerPrivate.FileCategory|undefined} It's used to
     * communicate category filter to chrome.fileManagerPrivate.getRecentFiles
     * API.
     */
    this.fileCategory;

    /**
     * @type {string} the class name for this class. It's workaround for the
     * fact that an instance created on foreground page and sent to background
     * page can't be checked with "instanceof".
     */
    this.type_name = 'FakeEntry';
  }

  /**
   * String used to determine the icon.
   * @return {string}
   */
  // @ts-ignore: error TS2378: A 'get' accessor must return a value.
  get iconName() {
    return '';
  }

  /**
   * FakeEntry can be a placeholder for the real volume, if so this field will
   * be the volume type of the volume it represents.
   * @return {VolumeType|null}
   */
  // @ts-ignore: error TS2378: A 'get' accessor must return a value.
  get volumeType() {
    return null;
  }
}
