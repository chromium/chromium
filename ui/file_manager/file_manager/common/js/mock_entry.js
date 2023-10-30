// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FileErrorToDomError} from './util.js';

/**
 * Joins paths so that the two paths are connected by only 1 '/'.
 * @param {string} a Path.
 * @param {string} b Path.
 * @return {string} Joined path.
 */
export function joinPath(a, b) {
  return a.replace(/\/+$/, '') + '/' + b.replace(/^\/+/, '');
}

/**
 * Mock class for DOMFileSystem.
 *
 * @extends {FileSystem}
 */
export class MockFileSystem {
  /**
   * @param {string} volumeId Volume ID.
   * @param {string=} opt_rootURL URL string of root which is used in
   *     MockEntry.toURL.
   */
  constructor(volumeId, opt_rootURL) {
    /** @type {string} */
    this.name = volumeId;

    /** @type {!Record<string, !Entry>} */
    this.entries = {};
    this.entries['/'] = MockDirectoryEntry.create(this, '/');

    /** @type {string} */
    this.rootURL = opt_rootURL || 'filesystem:' + volumeId + '/';
  }

  get root() {
    return /** @type {!DirectoryEntry} */ (this.entries['/']);
  }

  /**
   * Creates file and directory entries for all the given entries.  Entries can
   * be either string paths or objects containing properties 'fullPath',
   * 'metadata', 'content'.  Paths ending in slashes are interpreted as
   * directories.  All intermediate directories leading up to the
   * files/directories to be created, are also created.
   * @param {!Array<string|Object|!Entry>} entries An array of either string
   *     file paths, objects containing 'fullPath' and 'metadata', or Entry to
   *     populate in this file system.
   * @param {boolean=} opt_clear Optional, if true clears all entries before
   *     populating.
   */
  populate(entries, opt_clear) {
    if (opt_clear) {
      this.entries = {'/': MockDirectoryEntry.create(this, '/')};
    }
    entries.forEach(entry => {
      if (entry instanceof MockEntry) {
        this.entries[entry.fullPath] = entry;
        entry.filesystem = this;
        return;
      }
      // @ts-ignore: error TS2339: Property 'fullPath' does not exist on type
      // 'string | Object | FileSystemEntry'.
      const path = entry.fullPath || entry;
      // @ts-ignore: error TS2339: Property 'metadata' does not exist on type
      // 'string | Object | FileSystemEntry'.
      const metadata = entry.metadata || {size: 0};
      // @ts-ignore: error TS2339: Property 'content' does not exist on type
      // 'string | Object | FileSystemEntry'.
      const content = entry.content;
      const pathElements = path.split('/');
      // @ts-ignore: error TS7006: Parameter 'i' implicitly has an 'any' type.
      pathElements.forEach((_, i) => {
        const subpath = pathElements.slice(0, i).join('/');
        if (subpath && !(subpath in this.entries)) {
          this.entries[subpath] =
              MockDirectoryEntry.create(this, subpath, metadata);
        }
      });

      // If the path doesn't end in a slash, create a file.
      if (!/\/$/.test(path)) {
        this.entries[path] =
            MockFileEntry.create(this, path, metadata, content);
      }
    });
  }

  /**
   * Returns all children of the supplied directoryEntry.
   * @param  {!MockDirectoryEntry} directory parent directory to find children
   *     of.
   * @return {!Array<!Entry>}
   * @private
   */
  findChildren_(directory) {
    const parentPath = directory.fullPath.replace(/\/?$/, '/');
    const children = [];
    for (const path in this.entries) {
      if (path.indexOf(parentPath) === 0 && path !== parentPath) {
        const nextSeparator = path.indexOf('/', parentPath.length);
        // Add immediate children files and directories...
        if (nextSeparator === -1 || nextSeparator === path.length - 1) {
          children.push(this.entries[path]);
        }
      }
    }
    // @ts-ignore: error TS2322: Type '(FileSystemEntry | undefined)[]' is not
    // assignable to type 'FileSystemEntry[]'.
    return children;
  }
}

/** @interface */
export class MockEntryInterface {
  /**
   * Clones the entry with the new fullpath.
   *
   * @param {string} fullpath New fullpath.
   * @param {FileSystem=} opt_filesystem New file system
   * @return {Entry} Cloned entry.
   */
  // @ts-ignore: error TS6133: 'opt_filesystem' is declared but its value is
  // never read.
  clone(fullpath, opt_filesystem) {
    return /** @type {Entry}*/ ({});
  }
}

/**
 * Base class of mock entries.
 *
 * @extends {Entry}
 * @implements {MockEntryInterface}
 */
export class MockEntry {
  /**
   * @param {FileSystem} filesystem File system where the entry is located.
   * @param {string} fullPath Full path of the entry.
   * @param {Metadata=} opt_metadata Metadata.
   */
  constructor(filesystem, fullPath, opt_metadata) {
    // @ts-ignore: error TS2339: Property 'entries' does not exist on type
    // 'FileSystem'.
    filesystem.entries[fullPath] = this;
    this.filesystem = filesystem;
    this.fullPath = fullPath;
    /** @public @type {!Metadata} */
    this.metadata = opt_metadata || /** @type {!Metadata} */ ({});
    this.removed_ = false;
    this.isFile = true;
    this.isDirectory = false;
  }

  /**
   * @return {string} Name of the entry.
   */
  get name() {
    return this.fullPath.replace(/^.*\//, '');
  }

  /**
   * Obtains metadata of the entry.
   *
   * @param {function(!Metadata):void} onSuccess Function to take the metadata.
   * @param {function(!FileError):void=} onError
   */
  getMetadata(onSuccess, onError) {
    // @ts-ignore: error TS2339: Property 'entries' does not exist on type
    // 'FileSystem'.
    if (this.filesystem.entries[this.fullPath]) {
      onSuccess(this.metadata);
    } else {
      // @ts-ignore: error TS2722: Cannot invoke an object which is possibly
      // 'undefined'.
      onError(
          /** @type {!FileError} */ (
              {name: FileErrorToDomError.NOT_FOUND_ERR}));
    }
  }

  /**
   * Returns fake URL.
   *
   * @return {string} Fake URL.
   */
  toURL() {
    const segments = this.fullPath.split('/');
    for (let i = 0; i < segments.length; i++) {
      // @ts-ignore: error TS2345: Argument of type 'string | undefined' is not
      // assignable to parameter of type 'string | number | boolean'.
      segments[i] = encodeURIComponent(segments[i]);
    }

    // @ts-ignore: error TS2339: Property 'rootURL' does not exist on type
    // 'FileSystem'.
    return joinPath(this.filesystem.rootURL, segments.join('/'));
  }

  /**
   * Obtains parent directory.
   *
   * @param {function(DirectoryEntry)=} onSuccess Callback invoked with
   *     the parent directory.
   * @param {function(!Error)=} onError Callback invoked with an error
   *     object.
   */
  getParent(onSuccess, onError) {
    const path = this.fullPath.replace(/\/[^\/]+$/, '') || '/';
    // @ts-ignore: error TS2339: Property 'entries' does not exist on type
    // 'FileSystem'.
    if (this.filesystem.entries[path]) {
      // @ts-ignore: error TS2339: Property 'entries' does not exist on type
      // 'FileSystem'.
      onSuccess(this.filesystem.entries[path]);
    } else {
      // @ts-ignore: error TS2722: Cannot invoke an object which is possibly
      // 'undefined'.
      onError(
          /** @type {!FileError} */ (
              {name: FileErrorToDomError.NOT_FOUND_ERR}));
    }
  }

  /**
   * Moves the entry to the directory.
   *
   * @param {!DirectoryEntry} parent Destination directory.
   * @param {string=} opt_newName New name.
   * @param {function(!Entry)=} opt_successCallback Callback invoked with the
   *     moved entry.
   * @param {function(!FileError)=} opt_errorCallback Callback invoked
   *     with an error object.
   */
  // @ts-ignore: error TS6133: 'opt_errorCallback' is declared but its value is
  // never read.
  moveTo(parent, opt_newName, opt_successCallback, opt_errorCallback) {
    Promise.resolve()
        .then(() => {
          // @ts-ignore: error TS2339: Property 'entries' does not exist on type
          // 'FileSystem'.
          delete this.filesystem.entries[this.fullPath];
          const newPath = joinPath(parent.fullPath, opt_newName || this.name);
          const newFs = parent.filesystem;
          // For directories, also move all descendant entries.
          if (this.isDirectory) {
            // @ts-ignore: error TS2339: Property 'entries' does not exist on
            // type 'FileSystem'.
            for (const e of Object.values(this.filesystem.entries)) {
              if (e.fullPath.startsWith(this.fullPath)) {
                // @ts-ignore: error TS2339: Property 'entries' does not exist
                // on type 'FileSystem'.
                delete this.filesystem.entries[e.fullPath];
                e.clone(e.fullPath.replace(this.fullPath, newPath), newFs);
              }
            }
          }
          return this.clone(newPath, newFs);
        })
        .then(opt_successCallback);
  }

  /**
   * @param {DirectoryEntry} parent
   * @param {string=} opt_newName
   * @param {function(!Entry)=} opt_successCallback
   * @param {function(!FileError)=} opt_errorCallback
   */
  // @ts-ignore: error TS6133: 'opt_errorCallback' is declared but its value is
  // never read.
  copyTo(parent, opt_newName, opt_successCallback, opt_errorCallback) {
    Promise.resolve()
        .then(() => {
          return this.clone(
              joinPath(parent.fullPath, opt_newName || this.name),
              parent.filesystem);
        })
        .then(opt_successCallback);
  }

  /**
   * Removes the entry.
   *
   * @param {function():void} onSuccess Success callback.
   * @param {function(!FileError):void=} onError Callback invoked with
   *     an error object.
   */
  // @ts-ignore: error TS6133: 'onError' is declared but its value is never
  // read.
  remove(onSuccess, onError) {
    this.removed_ = true;
    Promise.resolve().then(() => {
      // @ts-ignore: error TS2339: Property 'entries' does not exist on type
      // 'FileSystem'.
      delete this.filesystem.entries[this.fullPath];
      onSuccess();
    });
  }

  /**
   * Removes the entry and any children.
   *
   * @param {function():void} onSuccess Success callback.
   * @param {function(!FileError):void=} onError Callback invoked with
   *     an error object.
   */
  // @ts-ignore: error TS6133: 'onError' is declared but its value is never
  // read.
  removeRecursively(onSuccess, onError) {
    this.removed_ = true;
    Promise.resolve().then(() => {
      // @ts-ignore: error TS2339: Property 'entries' does not exist on type
      // 'FileSystem'.
      for (const path in this.filesystem.entries) {
        if (path.startsWith(this.fullPath)) {
          // @ts-ignore: error TS2339: Property 'entries' does not exist on type
          // 'FileSystem'.
          this.filesystem.entries[path].removed_ = true;
          // @ts-ignore: error TS2339: Property 'entries' does not exist on type
          // 'FileSystem'.
          delete this.filesystem.entries[path];
        }
      }
      onSuccess();
    });
  }

  /**
   * Asserts that the entry was removed.
   */
  assertRemoved() {
    if (!this.removed_) {
      throw new Error('expected removed for file ' + this.name);
    }
  }

  /**
   * @param {string} fullpath New fullpath.
   * @param {FileSystem=} opt_filesystem New file system
   * @return {Entry} Cloned entry.
   */
  // @ts-ignore: error TS6133: 'opt_filesystem' is declared but its value is
  // never read.
  clone(fullpath, opt_filesystem) {
    throw new Error('Not implemented.');
  }
}

/**
 * Mock class for FileEntry.
 *
 * @implements {MockEntryInterface}
 */
export class MockFileEntry extends MockEntry {
  /**
   * @param {FileSystem} filesystem File system where the entry is located.
   * @param {string} fullPath Full path for the entry.
   * @param {Metadata=} opt_metadata Metadata.
   * @param {Blob=} opt_content Optional content.
   */
  static create(filesystem, fullPath, opt_metadata, opt_content) {
    const instance = /** @type {!Object} */ (
        new MockFileEntry(filesystem, fullPath, opt_metadata, opt_content));
    return /** @type {!FileEntry} */ (instance);
  }

  /**
   * @private Use create() instead, so the instance gets the |FileEntry| type.
   * @param {FileSystem} filesystem File system where the entry is located.
   * @param {string} fullPath Full path for the entry.
   * @param {Metadata=} opt_metadata Metadata.
   * @param {Blob=} opt_content Optional content.
   */
  constructor(filesystem, fullPath, opt_metadata, opt_content) {
    super(filesystem, fullPath, opt_metadata);
    this.content = opt_content || new Blob([]);
    this.isFile = true;
    this.isDirectory = false;
  }

  /**
   * Returns a File that this represents.
   *
   * @param {function(!File):void} onSuccess Function to take the file.
   * @param {function(!FileError):void=} onError
   */
  // @ts-ignore: error TS6133: 'onError' is declared but its value is never
  // read.
  file(onSuccess, onError) {
    onSuccess(new File([this.content], this.toURL()));
  }

  /**
   * Returns a FileWriter.
   *
   * @param {function(!FileWriter):void} successCallback
   * @param {function(!FileError):void=} opt_errorCallback
   */
  // @ts-ignore: error TS6133: 'opt_errorCallback' is declared but its value is
  // never read.
  createWriter(successCallback, opt_errorCallback) {
    // @ts-ignore: error TS2345: Argument of type 'MockFileWriter' is not
    // assignable to parameter of type 'FileWriter'.
    successCallback(new MockFileWriter(this));
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'opt_filesystem' implicitly has an
  // 'any' type.
  clone(path, opt_filesystem) {
    return MockFileEntry.create(
        opt_filesystem || this.filesystem, path, this.metadata, this.content);
  }

  /**
   * Helper to expose methods mixed in via MockEntry to the type checker.
   *
   * @return {!MockEntry}
   */
  asMock() {
    // @ts-ignore: error TS1110: Type expected.
    return /** @type{!MockEntry} */ (/** @type(*) */ (this));
  }

  /**
   * @return {!FileEntry}
   */
  asFileEntry() {
    const instance = /** @type {!Object} */ (this);
    return /** @type {!FileEntry} */ (instance);
  }
}

/**
 * Mock class for FileWriter.
 * @extends {FileWriter}
 */
export class MockFileWriter {
  /**
   * @param {!MockFileEntry} entry
   */
  constructor(entry) {
    this.entry_ = entry;
    // @ts-ignore: error TS7006: Parameter 'e' implicitly has an 'any' type.
    this.onwriteend = (e) => {};
  }

  /**
   * @param {!Blob} data
   */
  write(data) {
    this.entry_.content = data;
    this.onwriteend(new ProgressEvent(
        'writeend',
        {lengthComputable: true, loaded: data.size, total: data.size}));
  }
}

/**
 * Mock class for DirectoryEntry.
 *
 * @implements {MockEntryInterface}
 */
export class MockDirectoryEntry extends MockEntry {
  /**
   * @param {FileSystem} filesystem File system where the entry is located.
   * @param {string} fullPath Full path for the entry.
   * @param {Metadata=} opt_metadata Metadata.
   */
  static create(filesystem, fullPath, opt_metadata) {
    const instance = /** @type {!Object} */ (
        new MockDirectoryEntry(filesystem, fullPath, opt_metadata));
    return /** @type {!DirectoryEntry} */ (instance);
  }

  /**
   * @private Use create() instead, so the instance gets the |DirectoryEntry|
   * type.
   * @param {FileSystem} filesystem File system where the entry is located.
   * @param {string} fullPath Full path for the entry.
   * @param {Metadata=} opt_metadata Metadata.
   */
  constructor(filesystem, fullPath, opt_metadata) {
    super(filesystem, fullPath, opt_metadata);
    this.metadata.size = this.metadata.size || 0;
    this.metadata.modificationTime =
        this.metadata.modificationTime || new Date();
    this.isFile = false;
    this.isDirectory = true;
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'opt_filesystem' implicitly has an
  // 'any' type.
  clone(path, opt_filesystem) {
    return MockDirectoryEntry.create(opt_filesystem || this.filesystem, path);
  }

  /**
   * Returns all children of the supplied directoryEntry.
   * @return {!Array<!Entry>}
   */
  getAllChildren() {
    // @ts-ignore: error TS2341: Property 'findChildren_' is private and only
    // accessible within class 'MockFileSystem'.
    return /** @type {MockFileSystem} */ (this.filesystem).findChildren_(this);
  }

  /**
   * Returns a file under the directory.
   *
   * @param {!Function} expectedClass class expected for entry. Either
   *     MockFileEntry or MockDirectoryEntry.
   * @param {string} path Path.
   * @param {!FileSystemFlags=} option Options
   * @param {function(!Entry)=} onSuccess Success callback.
   * @param {function(!FileError)=} onError Failure callback;
   * @private
   */
  getEntry_(expectedClass, path, option, onSuccess, onError) {
    // As onSuccess and onError are optional, if they are not supplied we
    // default them to be no-ops to save on checking their validity later.
    // @ts-ignore: error TS6133: 'entry' is declared but its value is never
    // read.
    onSuccess = onSuccess || (entry => {});  // no-op
    // @ts-ignore: error TS6133: 'error' is declared but its value is never
    // read.
    onError = onError || (error => {});      // no-op
    if (this.removed_) {
      return onError(
          /** @type {!FileError} */ (
              {name: FileErrorToDomError.NOT_FOUND_ERR}));
    }
    option = option || {};
    const fullPath = path[0] === '/' ? path : joinPath(this.fullPath, path);
    // @ts-ignore: error TS2339: Property 'entries' does not exist on type
    // 'FileSystem'.
    const result = this.filesystem.entries[fullPath];
    if (result) {
      if (!(result instanceof expectedClass)) {
        onError(
            /** @type {!FileError} */ (
                {name: FileErrorToDomError.TYPE_MISMATCH_ERR}));
      } else if (option['create'] && option['exclusive']) {
        onError(
            /** @type {!FileError} */ (
                {name: FileErrorToDomError.PATH_EXISTS_ERR}));
      } else {
        // @ts-ignore: error TS2345: Argument of type '{}' is not assignable to
        // parameter of type 'FileSystemEntry'.
        onSuccess(result);
      }
    } else {
      if (!option['create']) {
        onError(
            /** @type {!FileError} */ (
                {name: FileErrorToDomError.NOT_FOUND_ERR}));
      } else {
        // @ts-ignore: error TS2339: Property 'create' does not exist on type
        // 'Function'.
        const newEntry = expectedClass.create(this.filesystem, fullPath);
        onSuccess(newEntry);
      }
    }
  }

  /**
   * Returns a file under the directory.
   *
   * @param {string} path Path.
   * @param {!FileSystemFlags=} option Options
   * @param {function(!FileEntry)=} onSuccess Success callback.
   * @param {function(!FileError)=} onError Failure callback;
   */
  getFile(path, option, onSuccess, onError) {
    return this.getEntry_(
        MockFileEntry, path, option,
        // @ts-ignore: error TS7014: Function type, which lacks return-type
        // annotation, implicitly has an 'any' return type.
        /** @type {function(!Entry)|undefined} */ (onSuccess), onError);
  }

  /**
   * Returns a directory under the directory.
   *
   * @param {string} path Path.
   * @param {!FileSystemFlags=} option Options
   * @param {function(!DirectoryEntry)=} onSuccess Success callback.
   * @param {function(!FileError)=} onError Failure callback;
   */
  getDirectory(path, option, onSuccess, onError) {
    return this.getEntry_(
        MockDirectoryEntry, path, option,
        // @ts-ignore: error TS7014: Function type, which lacks return-type
        // annotation, implicitly has an 'any' return type.
        /** @type {function(!Entry)|undefined} */ (onSuccess), onError);
  }

  /**
   * Creates a MockDirectoryReader for the entry.
   * @return {!DirectoryReader} A directory reader.
   */
  createReader() {
    return new MockDirectoryReader(
        // @ts-ignore: error TS2341: Property 'findChildren_' is private and
        // only accessible within class 'MockFileSystem'.
        /** @type {MockFileSystem} */ (this.filesystem).findChildren_(this));
  }
}

/**
 * Mock class for DirectoryReader.
 * @extends {DirectoryReader}
 */
export class MockDirectoryReader {
  /**
   * @param {!Array<!Entry>} entries
   */
  constructor(entries) {
    this.entries_ = entries;
  }

  /**
   * Returns entries from the filesystem associated with this directory
   * in chunks of 2.
   *
   * @param {function(!Array<!Entry>):void} success Success callback.
   * @param {function(!FileError):void=} error Error callback.
   */
  // @ts-ignore: error TS6133: 'error' is declared but its value is never read.
  readEntries(success, error) {
    if (this.entries_.length > 0) {
      const chunk = this.entries_.splice(0, 2);
      success(chunk);
    } else {
      success([]);
    }
  }
}
