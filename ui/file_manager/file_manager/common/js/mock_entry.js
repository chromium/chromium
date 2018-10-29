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
 * @param {string} volumeId Volume ID.
 * @param {string=} opt_rootURL URL string of root which is used in
 *     MockEntry.toURL.
 * @constructor
 * @extends {FileSystem}
 */
function MockFileSystem(volumeId, opt_rootURL) {
  /** @type {string} */
  this.name = volumeId;

  /** @type {!Object<!Entry>} */
  this.entries = {};
  this.entries['/'] = new MockDirectoryEntry(this, '/');

  /** @type {string} */
  this.rootURL = opt_rootURL || 'filesystem:' + volumeId + '/';
}

MockFileSystem.prototype = {
  get root() { return this.entries['/']; }
};

/**
 * Creates file and directory entries for all the given entries.  Entries can
 * be either string paths or objects containing properties 'fullPath',
 * 'metadata', 'content'.  Paths ending in slashes are interpreted as
 * directories.  All intermediate directories leading up to the
 * files/directories to be created, are also created.
 * @param {!Array<string|Object>} entries An array of either string file paths,
 *     or objects containing 'fullPath' and 'metadata' to populate in this
 *     file system.
 * @param {boolean=} opt_clear Optional, if true clears all entries before
 *     populating.
 */
MockFileSystem.prototype.populate = function(entries, opt_clear) {
  if (opt_clear)
    this.entries = {'/': new MockDirectoryEntry(this, '/')};
  entries.forEach(function(entry) {
    var path = entry.fullPath || entry;
    var metadata = entry.metadata || {size: 0};
    var content = entry.content;
    var pathElements = path.split('/');
    pathElements.forEach(function(_, i) {
      var subpath = pathElements.slice(0, i).join('/');
      if (subpath && !(subpath in this.entries))
        this.entries[subpath] = new MockDirectoryEntry(this, subpath, metadata);
    }.bind(this));

    // If the path doesn't end in a slash, create a file.
    if (!/\/$/.test(path))
      this.entries[path] = new MockFileEntry(this, path, metadata, content);
  }.bind(this));
};

/**
 * Returns all children of the supplied directoryEntry.
 * @param  {!MockDirectoryEntry} directory parent directory to find children of.
 * @return {!Array<!Entry>}
 * @private
 */
MockFileSystem.prototype.findChildren_ = function(directory) {
  var parentPath = directory.fullPath.replace(/\/?$/, '/');
  var children = [];
  for (var path in this.entries) {
    if (path.indexOf(parentPath) === 0 && path !== parentPath) {
      var nextSeparator = path.indexOf('/', parentPath.length);
      // Add immediate children files and directories...
      if (nextSeparator === -1 ||
          nextSeparator === path.length - 1) {
        children.push(this.entries[path]);
      }
    }
  }
  return children;
};

/** @interface */
function MockEntryInterface() {}

/**
 * Clones the entry with the new fullpath.
 *
 * @param {string} fullpath New fullpath.
 * @param {FileSystem=} opt_filesystem New file system
 * @return {Entry} Cloned entry.
 */
MockEntryInterface.prototype.clone = function(fullpath, opt_filesystem) {};

/**
 * Base class of mock entries.
 *
 * @param {FileSystem} filesystem File system where the entry is localed.
 * @param {string} fullPath Full path of the entry.
 * @param {Metadata=} opt_metadata Metadata.
 * @constructor
 * @extends {Entry}
 * @implements {MockEntryInterface}
 */
function MockEntry(filesystem, fullPath, opt_metadata) {
  filesystem.entries[fullPath] = this;
  this.filesystem = filesystem;
  this.fullPath = fullPath;
  this.metadata = opt_metadata || /** @type {!Metadata} */ ({});
  this.removed_ = false;
}

MockEntry.prototype = {
  /**
   * @return {string} Name of the entry.
   */
  get name() {
    return this.fullPath.replace(/^.*\//, '');
  }
};

/**
 * Obtains metadata of the entry.
 *
 * @param {function(!Metadata)} onSuccess Function to take the metadata.
 * @param {function(!FileError)=} onError
 */
MockEntry.prototype.getMetadata = function(onSuccess, onError) {
  if (this.filesystem.entries[this.fullPath])
    onSuccess(this.metadata);
  else
    onError(/** @type {!FileError} */ ({name: util.FileError.NOT_FOUND_ERR}));
};

/**
 * Returns fake URL.
 *
 * @return {string} Fake URL.
 */
MockEntry.prototype.toURL = function() {
  var segments = this.fullPath.split('/');
  for (var i = 0; i < segments.length; i++) {
    segments[i] = encodeURIComponent(segments[i]);
  }

  return joinPath(this.filesystem.rootURL, segments.join('/'));
};

/**
 * Obtains parent directory.
 *
 * @param {function(!Entry)} onSuccess Callback invoked with
 *     the parent directory.
 * @param {function(!FileError)=} onError Callback invoked with an error
 *     object.
 */
MockEntry.prototype.getParent = function(onSuccess, onError) {
  var path = this.fullPath.replace(/\/[^\/]+$/, '') || '/';
  if (this.filesystem.entries[path])
    onSuccess(this.filesystem.entries[path]);
  else
    onError(/** @type {!FileError} */ ({name: util.FileError.NOT_FOUND_ERR}));
};

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
MockEntry.prototype.moveTo = function(
    parent, opt_newName, opt_successCallback, opt_errorCallback) {
  Promise.resolve()
      .then(() => {
        delete this.filesystem.entries[this.fullPath];
        return this.clone(
            joinPath(parent.fullPath, opt_newName || this.name),
            parent.filesystem);
      })
      .then(opt_successCallback);
};

/**
 * @param {DirectoryEntry} parent
 * @param {string=} opt_newName
 * @param {function(!Entry)=} opt_successCallback
 * @param {function(!FileError)=} opt_errorCallback
 */
MockEntry.prototype.copyTo = function(
    parent, opt_newName, opt_successCallback, opt_errorCallback) {
  Promise.resolve()
      .then(() => {
        return this.clone(
            joinPath(parent.fullPath, opt_newName || this.name),
            parent.filesystem);
      })
      .then(opt_successCallback);
};

/**
 * Removes the entry.
 *
 * @param {function()} onSuccess Success callback.
 * @param {function(!FileError)=} onError Callback invoked with an error object.
 */
MockEntry.prototype.remove = function(onSuccess, onError) {
  this.removed_ = true;
  Promise.resolve().then(() => {
    delete this.filesystem.entries[this.fullPath];
    onSuccess();
  });
};

/**
 * Asserts that the entry was removed.
 */
MockEntry.prototype.assertRemoved = function() {
  if (!this.removed_)
    throw new Error('expected removed for file ' + this.name);
};

/** @override */
MockEntry.prototype.clone = function(fullpath, opt_filesystem) {
  throw new Error('Not implemented.');
};

/**
 * Mock class for FileEntry.
 *
 * @param {FileSystem} filesystem File system where the entry is localed.
 * @param {string} fullPath Full path for the entry.
 * @param {Metadata=} opt_metadata Metadata.
 * @param {Blob=} opt_content Optional content.
 * @extends {FileEntry}
 * @implements {MockEntryInterface}
 * @constructor
 */
function MockFileEntry(filesystem, fullPath, opt_metadata, opt_content) {
  filesystem.entries[fullPath] = this;
  this.filesystem = filesystem;
  this.fullPath = fullPath;
  this.metadata = opt_metadata || /** @type {!Metadata} */ ({});
  this.content = opt_content || new Blob([]);
  this.removed_ = false;
  this.isFile = true;
  this.isDirectory = false;
}

MockFileEntry.prototype = {
  __proto__: MockEntry.prototype
};

/**
 * Returns a File that this represents.
 *
 * @param {function(!File)} onSuccess Function to take the file.
 * @param {function(!FileError)=} onError
 */
MockFileEntry.prototype.file = function(onSuccess, onError) {
  onSuccess(new File([this.content], this.toURL()));
};

/** @override */
MockFileEntry.prototype.clone = function(path, opt_filesystem) {
  return new MockFileEntry(
      opt_filesystem || this.filesystem, path, this.metadata, this.content);
};

/**
 * Helper to expose methods mixed in via MockEntry to the type checker.
 *
 * @return {!MockEntry}
 */
MockFileEntry.prototype.asMock = function() {
  return /** @type{!MockEntry} */ (/** @type(*) */ (this));
};

/**
 * Mock class for DirectoryEntry.
 *
 * @param {FileSystem} filesystem File system where the entry is localed.
 * @param {string} fullPath Full path for the entry.
 * @param {Metadata=} opt_metadata Metadata.
 * @extends {DirectoryEntry} MockDirectoryEntry is used to mock the implement
 *   DirectoryEntry for testing.
 * @implements {MockEntryInterface}
 * @constructor
 */
function MockDirectoryEntry(filesystem, fullPath, opt_metadata) {
  filesystem.entries[fullPath] = this;
  this.filesystem = filesystem;
  this.fullPath = fullPath;
  this.metadata = opt_metadata || /** @type {!Metadata} */ ({});
  this.metadata.size = this.metadata.size || 0;
  this.metadata.modificationTime = this.metadata.modificationTime || new Date();
  this.removed_ = false;
  this.isFile = false;
  this.isDirectory = true;
}

MockDirectoryEntry.prototype = {
  __proto__: MockEntry.prototype
};

/** @override */
MockDirectoryEntry.prototype.clone = function(path, opt_filesystem) {
  return new MockDirectoryEntry(opt_filesystem || this.filesystem, path);
};

/**
 * Returns all children of the supplied directoryEntry.
 * @return {!Array<!Entry>}
 */
MockDirectoryEntry.prototype.getAllChildren = function() {
  return /** @type {MockFileSystem} */ (this.filesystem).findChildren_(this);
};

/**
 * Returns a file under the directory.
 *
 * @param {string} path Path.
 * @param {!FileSystemFlags=} option Options
 * @param {function(!FileEntry)=} onSuccess Success callback.
 * @param {function(!FileError)=} onError Failure callback;
 */
MockDirectoryEntry.prototype.getFile = function(
    path, option, onSuccess, onError) {
  // As onSuccess and onError are optional, if they are not supplied we default
  // them to be no-ops to save on checking their validity later.
  onSuccess = onSuccess || (entry => {});  // no-op
  onError = onError || (error => {});      // no-op
  var fullPath = path[0] === '/' ? path : joinPath(this.fullPath, path);
  if (!this.filesystem.entries[fullPath])
    onError(/** @type {!FileError} */ ({name: util.FileError.NOT_FOUND_ERR}));
  else if (!(this.filesystem.entries[fullPath] instanceof MockFileEntry))
    onError(
        /** @type {!FileError} */ ({name: util.FileError.TYPE_MISMATCH_ERR}));
  else
    onSuccess(this.filesystem.entries[fullPath]);
};

/**
 * Returns a directory under the directory.
 *
 * @param {string} path Path.
 * @param {!FileSystemFlags=} option Options
 * @param {function(!DirectoryEntry)=} onSuccess Success callback.
 * @param {function(!FileError)=} onError Failure callback;
 */
MockDirectoryEntry.prototype.getDirectory = function(
    path, option, onSuccess, onError) {
  // As onSuccess and onError are optional, if they are not supplied we default
  // them to be no-ops to save on checking their validity later.
  onSuccess = onSuccess || (entry => {});  // no-op
  onError = onError || (error => {});      // no-op
  var fullPath = path[0] === '/' ? path : joinPath(this.fullPath, path);
  var result = this.filesystem.entries[fullPath];
  if (result) {
    if (!(result instanceof MockDirectoryEntry))
      onError(
          /** @type {!FileError} */ ({name: util.FileError.TYPE_MISMATCH_ERR}));
    else if (option['create'] && option['exclusive'])
      onError(
          /** @type {!FileError} */ ({name: util.FileError.PATH_EXISTS_ERR}));
    else
      onSuccess(result);
  } else {
    if (!option['create']) {
      onError(/** @type {!FileError} */ ({name: util.FileError.NOT_FOUND_ERR}));
    } else {
      var newEntry = new MockDirectoryEntry(this.filesystem, fullPath);
      this.filesystem.entries[fullPath] = newEntry;
      onSuccess(newEntry);
    }
  }
};

/**
 * Creates a MockDirectoryReader for the entry.
 * @return {!DirectoryReader} A directory reader.
 */
MockDirectoryEntry.prototype.createReader = function() {
  return new MockDirectoryReader(
      /** @type {MockFileSystem} */ (this.filesystem).findChildren_(this));
};

/**
 * Mock class for DirectoryReader.
 * @param {!Array<!Entry>} entries
 * @constructor
 * @extends {DirectoryReader}
 */
function MockDirectoryReader(entries) {
  this.entries_ = entries;
}

/**
 * Returns entries from the filesystem associated with this directory
 * in chunks of 2.
 *
 * @param {function(!Array<!Entry>)} success Success callback.
 * @param {function(!FileError)=} error Error callback.
 */
MockDirectoryReader.prototype.readEntries = function(success, error) {
  if (this.entries_.length > 0) {
    var chunk = this.entries_.splice(0, 2);
    success(chunk);
  } else {
    success([]);
  }
};
