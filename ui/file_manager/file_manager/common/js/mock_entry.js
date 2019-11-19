// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Joins paths so that the two paths are connected by only 1 '/'.
 * @param {string} a Path.
 * @param {string} b Path.
 * @return {string} Joined path.
 */
function joinPath(a, b) {
  return a.replace(/\/+$/, '') + '/' + b.replace(/^\/+/, '');
}

/**
 * Mock class for DOMFileSystem.
 *
 * @extends {FileSystem}
 */
class MockFileSystem {
  /**
   * @param {string} volumeId Volume ID.
   * @param {string=} opt_rootURL URL string of root which is used in
   *     MockEntry.toURL.
   */
  constructor(volumeId, opt_rootURL) {
    /** @type {string} */
    this.name = volumeId;

    /** @type {!Object<!Entry>} */
    this.entries = {};
    this.entries['/'] = MockDirectoryEntry.create(this, '/');

    /** @type {string} */
    this.rootURL = opt_rootURL || 'filesystem:' + volumeId + '/';
  }

  get root() {
    return this.entries['/'];
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
      const path = entry.fullPath || entry;
      const metadata = entry.metadata || {size: 0};
      const content = entry.content;
      const pathElements = path.split('/');
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
    return children;
  }
}

/** @interface */
class MockEntryInterface {
  /**
   * Clones the entry with the new fullpath.
   *
   * @param {string} fullpath New fullpath.
   * @param {FileSystem=} opt_filesystem New file system
   * @return {Entry} Cloned entry.
   */
  clone(fullpath, opt_filesystem) {}
}

/**
 * Base class of mock entries.
 *
 * @extends {Entry}
 * @implements {MockEntryInterface}
 */
class MockEntry {
  /**
   * @param {FileSystem} filesystem File system where the entry is located.
   * @param {string} fullPath Full path of the entry.
   * @param {Metadata=} opt_metadata Metadata.
   */
  constructor(filesystem, fullPath, opt_metadata) {
    filesystem.entries[fullPath] = this;
    this.filesystem = filesystem;
    this.fullPath = fullPath;
    /** @public {!Metadata} */
    this.metadata = opt_metadata || /** @type {!Metadata} */ ({});
    this.removed_ = false;
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
   * @param {function(!Metadata)} onSuccess Function to take the metadata.
   * @param {function(!FileError)=} onError
   */
  getMetadata(onSuccess, onError) {
    if (this.filesystem.entries[this.fullPath]) {
      onSuccess(this.metadata);
    } else {
      onError(/** @type {!FileError} */ ({name: util.FileError.NOT_FOUND_ERR}));
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
      segments[i] = encodeURIComponent(segments[i]);
    }

    return joinPath(this.filesystem.rootURL, segments.join('/'));
  }

  /**
   * Obtains parent directory.
   *
   * @param {function(!Entry)} onSuccess Callback invoked with
   *     the parent directory.
   * @param {function(!FileError)=} onError Callback invoked with an error
   *     object.
   */
  getParent(onSuccess, onError) {
    const path = this.fullPath.replace(/\/[^\/]+$/, '') || '/';
    if (this.filesystem.entries[path]) {
      onSuccess(this.filesystem.entries[path]);
    } else {
      onError(/** @type {!FileError} */ ({name: util.FileError.NOT_FOUND_ERR}));
    }
  }

  /**
   * Moves the entry to the directory.
   *
   * @param {!DirectoryEntry} parent Destination directory.
   * @param {string=} opt_newName New name.
   * @param {function(!Entry)=} opt_successCallback Callback invoked with the
   *     moved entry.
   * @param {function(!FileError)=} opt_errorCallback Callback invoked with an
   *     error object.
   */
  moveTo(parent, opt_newName, opt_successCallback, opt_errorCallback) {
    Promise.resolve()
        .then(() => {
          delete this.filesystem.entries[this.fullPath];
          return this.clone(
              joinPath(parent.fullPath, opt_newName || this.name),
              parent.filesystem);
        })
        .then(opt_successCallback);
  }

  /**
   * @param {DirectoryEntry} parent
   * @param {string=} opt_newName
   * @param {function(!Entry)=} opt_successCallback
   * @param {function(!FileError)=} opt_errorCallback
   */
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
   * @param {function()} onSuccess Success callback.
   * @param {function(!FileError)=} onError Callback invoked with an error
   *     object.
   */
  remove(onSuccess, onError) {
    this.removed_ = true;
    Promise.resolve().then(() => {
      delete this.filesystem.entries[this.fullPath];
      onSuccess();
    });
  }

  /**
   * Removes the entry and any children.
   *
   * @param {function()} onSuccess Success callback.
   * @param {function(!FileError)=} onError Callback invoked with an error
   *     object.
   */
  removeRecursively(onSuccess, onError) {
    this.removed_ = true;
    Promise.resolve().then(() => {
      for (let path in this.filesystem.entries) {
        if (path.startsWith(this.fullPath)) {
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

  /** @override */
  clone(fullpath, opt_filesystem) {
    throw new Error('Not implemented.');
  }
}

/**
 * Mock class for FileEntry.
 *
 * @implements {MockEntryInterface}
 */
class MockFileEntry extends MockEntry {
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
   * @param {function(!File)} onSuccess Function to take the file.
   * @param {function(!FileError)=} onError
   */
  file(onSuccess, onError) {
    onSuccess(new File([this.content], this.toURL()));
  }

  /** @override */
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
 * Mock class for DirectoryEntry.
 *
 * @implements {MockEntryInterface}
 */
class MockDirectoryEntry extends MockEntry {
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
  clone(path, opt_filesystem) {
    return MockDirectoryEntry.create(opt_filesystem || this.filesystem, path);
  }

  /**
   * Returns all children of the supplied directoryEntry.
   * @return {!Array<!Entry>}
   */
  getAllChildren() {
    return /** @type {MockFileSystem} */ (this.filesystem).findChildren_(this);
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
    // As onSuccess and onError are optional, if they are not supplied we
    // default them to be no-ops to save on checking their validity later.
    onSuccess = onSuccess || (entry => {});  // no-op
    onError = onError || (error => {});      // no-op
    const fullPath = path[0] === '/' ? path : joinPath(this.fullPath, path);
    if (!this.filesystem.entries[fullPath]) {
      onError(/** @type {!FileError} */ ({name: util.FileError.NOT_FOUND_ERR}));
    } else if (!(this.filesystem.entries[fullPath] instanceof MockFileEntry)) {
      onError(
          /** @type {!FileError} */ ({name: util.FileError.TYPE_MISMATCH_ERR}));
    } else {
      onSuccess(this.filesystem.entries[fullPath]);
    }
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
    // As onSuccess and onError are optional, if they are not supplied we
    // default them to be no-ops to save on checking their validity later.
    onSuccess = onSuccess || (entry => {});  // no-op
    onError = onError || (error => {});      // no-op
    const fullPath = path[0] === '/' ? path : joinPath(this.fullPath, path);
    const result = this.filesystem.entries[fullPath];
    if (result) {
      if (!(result instanceof MockDirectoryEntry)) {
        onError(
            /** @type {!FileError} */ (
                {name: util.FileError.TYPE_MISMATCH_ERR}));
      } else if (option['create'] && option['exclusive']) {
        onError(
            /** @type {!FileError} */ ({name: util.FileError.PATH_EXISTS_ERR}));
      } else {
        onSuccess(result);
      }
    } else {
      if (!option['create']) {
        onError(
            /** @type {!FileError} */ ({name: util.FileError.NOT_FOUND_ERR}));
      } else {
        const newEntry = MockDirectoryEntry.create(this.filesystem, fullPath);
        this.filesystem.entries[fullPath] = newEntry;
        onSuccess(newEntry);
      }
    }
  }

  /**
   * Creates a MockDirectoryReader for the entry.
   * @return {!DirectoryReader} A directory reader.
   */
  createReader() {
    return new MockDirectoryReader(
        /** @type {MockFileSystem} */ (this.filesystem).findChildren_(this));
  }
}

/**
 * Mock class for DirectoryReader.
 * @extends {DirectoryReader}
 */
class MockDirectoryReader {
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
   * @param {function(!Array<!Entry>)} success Success callback.
   * @param {function(!FileError)=} error Error callback.
   */
  readEntries(success, error) {
    if (this.entries_.length > 0) {
      const chunk = this.entries_.splice(0, 2);
      success(chunk);
    } else {
      success([]);
    }
  }
}
