/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Concrete implementations of the
 *     goog.fs.DirectoryEntry, and goog.fs.FileEntry interfaces.
 */
goog.provide('goog.fs.DirectoryEntryImpl');
goog.provide('goog.fs.EntryImpl');
goog.provide('goog.fs.FileEntryImpl');

goog.require('goog.async.Deferred');
goog.require('goog.fs.DirectoryEntry');
goog.require('goog.fs.Entry');
goog.require('goog.fs.Error');
goog.require('goog.fs.FileEntry');
goog.require('goog.fs.FileWriter');
goog.require('goog.functions');
goog.require('goog.string');
goog.requireType('goog.fs.FileSystem');



/**
 * Base class for concrete implementations of goog.fs.Entry.
 * @param {!goog.fs.FileSystem} fs The wrapped filesystem.
 * @param {!Entry} entry The underlying Entry object.
 * @constructor
 * @implements {goog.fs.Entry}
 */
goog.fs.EntryImpl = function(fs, entry) {
  'use strict';
  /**
   * The wrapped filesystem.
   *
   * @type {!goog.fs.FileSystem}
   * @private
   */
  this.fs_ = fs;

  /**
   * The underlying Entry object.
   *
   * @type {!Entry}
   * @private
   */
  this.entry_ = entry;
};


/** @override */
goog.fs.EntryImpl.prototype.isFile = function() {
  'use strict';
  return this.entry_.isFile;
};


/** @override */
goog.fs.EntryImpl.prototype.isDirectory = function() {
  'use strict';
  return this.entry_.isDirectory;
};


/** @override */
goog.fs.EntryImpl.prototype.getName = function() {
  'use strict';
  return this.entry_.name;
};


/** @override */
goog.fs.EntryImpl.prototype.getFullPath = function() {
  'use strict';
  return this.entry_.fullPath;
};


/** @override */
goog.fs.EntryImpl.prototype.getFileSystem = function() {
  'use strict';
  return this.fs_;
};


/** @override */
goog.fs.EntryImpl.prototype.getLastModified = function() {
  'use strict';
  return this.getMetadata().addCallback(function(metadata) {
    'use strict';
    return metadata.modificationTime;
  });
};


/** @override */
goog.fs.EntryImpl.prototype.getMetadata = function() {
  'use strict';
  const d = new goog.async.Deferred();

  this.entry_.getMetadata(function(metadata) {
    'use strict';
    d.callback(metadata);
  }, goog.bind(function(err) {
    'use strict';
    const msg = 'retrieving metadata for ' + this.getFullPath();
    d.errback(new goog.fs.Error(err, msg));
  }, this));
  return d;
};


/** @override */
goog.fs.EntryImpl.prototype.moveTo = function(parent, opt_newName) {
  'use strict';
  const d = new goog.async.Deferred();
  this.entry_.moveTo(
      /** @type {!goog.fs.DirectoryEntryImpl} */ (parent).dir_, opt_newName,
      goog.bind(function(entry) {
        'use strict';
        d.callback(this.wrapEntry(entry));
      }, this), goog.bind(function(err) {
        'use strict';
        const msg = 'moving ' + this.getFullPath() + ' into ' +
            parent.getFullPath() +
            (opt_newName ? ', renaming to ' + opt_newName : '');
        d.errback(new goog.fs.Error(err, msg));
      }, this));
  return d;
};


/** @override */
goog.fs.EntryImpl.prototype.copyTo = function(parent, opt_newName) {
  'use strict';
  const d = new goog.async.Deferred();
  this.entry_.copyTo(
      /** @type {!goog.fs.DirectoryEntryImpl} */ (parent).dir_, opt_newName,
      goog.bind(function(entry) {
        'use strict';
        d.callback(this.wrapEntry(entry));
      }, this), goog.bind(function(err) {
        'use strict';
        const msg = 'copying ' + this.getFullPath() + ' into ' +
            parent.getFullPath() +
            (opt_newName ? ', renaming to ' + opt_newName : '');
        d.errback(new goog.fs.Error(err, msg));
      }, this));
  return d;
};


/** @override */
goog.fs.EntryImpl.prototype.wrapEntry = function(entry) {
  'use strict';
  return entry.isFile ?
      new goog.fs.FileEntryImpl(this.fs_, /** @type {!FileEntry} */ (entry)) :
      new goog.fs.DirectoryEntryImpl(
          this.fs_, /** @type {!DirectoryEntry} */ (entry));
};


/** @override */
goog.fs.EntryImpl.prototype.toUrl = function(opt_mimeType) {
  'use strict';
  return this.entry_.toURL(opt_mimeType);
};


/** @override */
goog.fs.EntryImpl.prototype.toUri = goog.fs.EntryImpl.prototype.toUrl;


/** @override */
goog.fs.EntryImpl.prototype.remove = function() {
  'use strict';
  const d = new goog.async.Deferred();
  this.entry_.remove(
      goog.bind(d.callback, d, true /* result */), goog.bind(function(err) {
        'use strict';
        const msg = 'removing ' + this.getFullPath();
        d.errback(new goog.fs.Error(err, msg));
      }, this));
  return d;
};


/** @override */
goog.fs.EntryImpl.prototype.getParent = function() {
  'use strict';
  const d = new goog.async.Deferred();
  this.entry_.getParent(goog.bind(function(parent) {
    'use strict';
    d.callback(new goog.fs.DirectoryEntryImpl(this.fs_, parent));
  }, this), goog.bind(function(err) {
    'use strict';
    const msg = 'getting parent of ' + this.getFullPath();
    d.errback(new goog.fs.Error(err, msg));
  }, this));
  return d;
};



/**
 * A directory in a local FileSystem.
 *
 * This should not be instantiated directly. Instead, it should be accessed via
 * {@link goog.fs.FileSystem#getRoot} or
 * {@link goog.fs.DirectoryEntry#getDirectoryEntry}.
 *
 * @param {!goog.fs.FileSystem} fs The wrapped filesystem.
 * @param {!DirectoryEntry} dir The underlying DirectoryEntry object.
 * @constructor
 * @extends {goog.fs.EntryImpl}
 * @implements {goog.fs.DirectoryEntry}
 * @final
 */
goog.fs.DirectoryEntryImpl = function(fs, dir) {
  'use strict';
  goog.fs.DirectoryEntryImpl.base(this, 'constructor', fs, dir);

  /**
   * The underlying DirectoryEntry object.
   *
   * @type {!DirectoryEntry}
   * @private
   */
  this.dir_ = dir;
};
goog.inherits(goog.fs.DirectoryEntryImpl, goog.fs.EntryImpl);


/** @override */
goog.fs.DirectoryEntryImpl.prototype.getFile = function(path, opt_behavior) {
  'use strict';
  const d = new goog.async.Deferred();
  this.dir_.getFile(
      path, this.getOptions_(opt_behavior), goog.bind(function(entry) {
        'use strict';
        d.callback(new goog.fs.FileEntryImpl(this.fs_, entry));
      }, this), goog.bind(function(err) {
        'use strict';
        const msg = 'loading file ' + path + ' from ' + this.getFullPath();
        d.errback(new goog.fs.Error(err, msg));
      }, this));
  return d;
};


/** @override */
goog.fs.DirectoryEntryImpl.prototype.getDirectory = function(
    path, opt_behavior) {
  'use strict';
  const d = new goog.async.Deferred();
  this.dir_.getDirectory(
      path, this.getOptions_(opt_behavior), goog.bind(function(entry) {
        'use strict';
        d.callback(new goog.fs.DirectoryEntryImpl(this.fs_, entry));
      }, this), goog.bind(function(err) {
        'use strict';
        const msg = 'loading directory ' + path + ' from ' + this.getFullPath();
        d.errback(new goog.fs.Error(err, msg));
      }, this));
  return d;
};


/** @override */
goog.fs.DirectoryEntryImpl.prototype.createPath = function(path) {
  'use strict';
  // If the path begins at the root, reinvoke createPath on the root directory.
  if (goog.string.startsWith(path, '/')) {
    const root = this.getFileSystem().getRoot();
    if (this.getFullPath() != root.getFullPath()) {
      return root.createPath(path);
    }
  }

  // Filter out any empty path components caused by '//' or a leading slash.
  const parts = path.split('/').filter(goog.functions.identity);

  /**
   * @param {goog.fs.DirectoryEntryImpl} dir
   * @return {!goog.async.Deferred}
   */
  function getNextDirectory(dir) {
    if (!parts.length) {
      return goog.async.Deferred.succeed(dir);
    }

    let def;
    const nextDir = parts.shift();

    if (nextDir == '..') {
      def = dir.getParent();
    } else if (nextDir == '.') {
      def = goog.async.Deferred.succeed(dir);
    } else {
      def = dir.getDirectory(nextDir, goog.fs.DirectoryEntry.Behavior.CREATE);
    }
    return def.addCallback(getNextDirectory);
  }

  return getNextDirectory(this);
};


/** @override */
goog.fs.DirectoryEntryImpl.prototype.listDirectory = function() {
  'use strict';
  const d = new goog.async.Deferred();
  const reader = this.dir_.createReader();
  const results = [];

  const errorCallback = goog.bind(function(err) {
    'use strict';
    const msg = 'listing directory ' + this.getFullPath();
    d.errback(new goog.fs.Error(err, msg));
  }, this);

  const successCallback = goog.bind(function(entries) {
    'use strict';
    if (entries.length) {
      for (let i = 0, entry; entry = entries[i]; i++) {
        results.push(this.wrapEntry(entry));
      }
      reader.readEntries(successCallback, errorCallback);
    } else {
      d.callback(results);
    }
  }, this);

  reader.readEntries(successCallback, errorCallback);
  return d;
};


/** @override */
goog.fs.DirectoryEntryImpl.prototype.removeRecursively = function() {
  'use strict';
  const d = new goog.async.Deferred();
  this.dir_.removeRecursively(
      goog.bind(d.callback, d, true /* result */), goog.bind(function(err) {
        'use strict';
        const msg = 'removing ' + this.getFullPath() + ' recursively';
        d.errback(new goog.fs.Error(err, msg));
      }, this));
  return d;
};


/**
 * Converts a value in the Behavior enum into an options object expected by the
 * File API.
 *
 * @param {goog.fs.DirectoryEntry.Behavior=} opt_behavior The behavior for
 *     existing files.
 * @return {!Object<boolean>} The options object expected by the File API.
 * @private
 */
goog.fs.DirectoryEntryImpl.prototype.getOptions_ = function(opt_behavior) {
  'use strict';
  if (opt_behavior == goog.fs.DirectoryEntry.Behavior.CREATE) {
    return {'create': true};
  } else if (opt_behavior == goog.fs.DirectoryEntry.Behavior.CREATE_EXCLUSIVE) {
    return {'create': true, 'exclusive': true};
  } else {
    return {};
  }
};



/**
 * A file in a local filesystem.
 *
 * This should not be instantiated directly. Instead, it should be accessed via
 * {@link goog.fs.DirectoryEntry#getFile}.
 *
 * @param {!goog.fs.FileSystem} fs The wrapped filesystem.
 * @param {!FileEntry} file The underlying FileEntry object.
 * @constructor
 * @extends {goog.fs.EntryImpl}
 * @implements {goog.fs.FileEntry}
 * @final
 */
goog.fs.FileEntryImpl = function(fs, file) {
  'use strict';
  goog.fs.FileEntryImpl.base(this, 'constructor', fs, file);

  /**
   * The underlying FileEntry object.
   *
   * @type {!FileEntry}
   * @private
   */
  this.file_ = file;
};
goog.inherits(goog.fs.FileEntryImpl, goog.fs.EntryImpl);


/** @override */
goog.fs.FileEntryImpl.prototype.createWriter = function() {
  'use strict';
  const d = new goog.async.Deferred();
  this.file_.createWriter(function(w) {
    'use strict';
    d.callback(new goog.fs.FileWriter(w));
  }, goog.bind(function(err) {
    'use strict';
    const msg = 'creating writer for ' + this.getFullPath();
    d.errback(new goog.fs.Error(err, msg));
  }, this));
  return d;
};


/** @override */
goog.fs.FileEntryImpl.prototype.file = function() {
  'use strict';
  const d = new goog.async.Deferred();
  this.file_.file(function(f) {
    'use strict';
    d.callback(f);
  }, goog.bind(function(err) {
    'use strict';
    const msg = 'getting file for ' + this.getFullPath();
    d.errback(new goog.fs.Error(err, msg));
  }, this));
  return d;
};
