/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Mock filesystem objects. These are all in the same file to
 * avoid circular dependency issues.
 */

goog.setTestOnly('goog.testing.fs.DirectoryEntry');

// TODO(user): We're trying to migrate all ES5 subclasses of Closure
// Library to ES6. In ES6 this cannot be referenced before super is called. This
// file has at least one this before a super call (in ES5) and cannot be
// automatically upgraded to ES6 as a result. Please fix this if you have a
// chance. Note: This can sometimes be caused by not calling the super
// constructor at all. You can run the conversion tool yourself to see what it
// does on this file: blaze run //javascript/refactoring/es6_classes:convert.

goog.provide('goog.testing.fs.DirectoryEntry');
goog.provide('goog.testing.fs.Entry');
goog.provide('goog.testing.fs.FileEntry');

goog.require('goog.Timer');
goog.require('goog.array');
goog.require('goog.asserts');
goog.require('goog.async.Deferred');
goog.require('goog.fs.DirectoryEntry');
goog.require('goog.fs.DirectoryEntryImpl');
goog.require('goog.fs.Entry');
goog.require('goog.fs.Error');
goog.require('goog.fs.FileEntry');
goog.require('goog.functions');
goog.require('goog.object');
goog.require('goog.string');
goog.require('goog.testing.fs.File');
goog.require('goog.testing.fs.FileWriter');
goog.requireType('goog.testing.fs.FileSystem');

/**
 * A mock filesystem entry object.
 *
 * @param {!goog.testing.fs.FileSystem} fs The filesystem containing this entry.
 * @param {!goog.testing.fs.DirectoryEntry} parent The directory entry directly
 *     containing this entry.
 * @param {string} name The name of this entry.
 * @constructor
 * @implements {goog.fs.Entry}
 */
goog.testing.fs.Entry = function(fs, parent, name) {
  'use strict';
  /**
   * This entry's filesystem.
   * @type {!goog.testing.fs.FileSystem}
   * @private
   */
  this.fs_ = fs;

  /**
   * The name of this entry.
   * @type {string}
   * @private
   */
  this.name_ = name;

  /**
   * The parent of this entry.
   * @type {!goog.testing.fs.DirectoryEntry}
   */
  this.parent = parent;
};


/**
 * Whether or not this entry has been deleted.
 * @type {boolean}
 */
goog.testing.fs.Entry.prototype.deleted = false;


/** @override */
goog.testing.fs.Entry.prototype.isFile = goog.abstractMethod;


/** @override */
goog.testing.fs.Entry.prototype.isDirectory = goog.abstractMethod;


/** @override */
goog.testing.fs.Entry.prototype.getName = function() {
  'use strict';
  return this.name_;
};


/** @override */
goog.testing.fs.Entry.prototype.getFullPath = function() {
  'use strict';
  if (this.getName() == '' || this.parent.getName() == '') {
    // The root directory has an empty name
    return '/' + this.name_;
  } else {
    return this.parent.getFullPath() + '/' + this.name_;
  }
};


/**
 * @return {!goog.testing.fs.FileSystem}
 * @override
 */
goog.testing.fs.Entry.prototype.getFileSystem = function() {
  'use strict';
  return this.fs_;
};


/** @override */
goog.testing.fs.Entry.prototype.getLastModified = goog.abstractMethod;


/** @override */
goog.testing.fs.Entry.prototype.getMetadata = goog.abstractMethod;


/** @override */
goog.testing.fs.Entry.prototype.moveTo = function(parent, opt_newName) {
  'use strict';
  const msg = 'moving ' + this.getFullPath() + ' into ' + parent.getFullPath() +
      (opt_newName ? ', renaming to ' + opt_newName : '');
  let newFile;
  return this.checkNotDeleted(msg)
      .addCallback(function() {
        'use strict';
        return this.copyTo(parent, opt_newName);
      })
      .addCallback(function(file) {
        'use strict';
        newFile = file;
        return this.remove();
      })
      .addCallback(function() {
        'use strict';
        return newFile;
      });
};


/** @override */
goog.testing.fs.Entry.prototype.copyTo = function(parent, opt_newName) {
  'use strict';
  goog.asserts.assert(parent instanceof goog.testing.fs.DirectoryEntry);
  const msg = 'copying ' + this.getFullPath() + ' into ' +
      parent.getFullPath() +
      (opt_newName ? ', renaming to ' + opt_newName : '');
  const self = this;
  return this.checkNotDeleted(msg).addCallback(function() {
    'use strict';
    goog.asserts.assert(parent instanceof goog.testing.fs.DirectoryEntry);
    const name = opt_newName || self.getName();
    const entry = self.clone();
    /** @type {!goog.testing.fs.DirectoryEntry} */ (parent).children[name] =
        entry;
    parent.lastModifiedTimestamp_ = Date.now();
    entry.name_ = name;
    entry.parent = /** @type {!goog.testing.fs.DirectoryEntry} */ (parent);
    return entry;
  });
};


/**
 * @return {!goog.testing.fs.Entry} A shallow copy of this entry object.
 */
goog.testing.fs.Entry.prototype.clone = goog.abstractMethod;


/** @override */
goog.testing.fs.Entry.prototype.toUrl = function(opt_mimetype) {
  'use strict';
  return 'fakefilesystem:' + this.getFullPath();
};


/** @override */
goog.testing.fs.Entry.prototype.toUri = goog.testing.fs.Entry.prototype.toUrl;


/** @override */
goog.testing.fs.Entry.prototype.wrapEntry = goog.abstractMethod;


/** @override */
goog.testing.fs.Entry.prototype.remove = function() {
  'use strict';
  const msg = 'removing ' + this.getFullPath();
  const self = this;
  return this.checkNotDeleted(msg).addCallback(function() {
    'use strict';
    delete this.parent.children[self.getName()];
    self.parent.lastModifiedTimestamp_ = Date.now();
    self.deleted = true;
    return;
  });
};


/** @override */
goog.testing.fs.Entry.prototype.getParent = function() {
  'use strict';
  const msg = 'getting parent of ' + this.getFullPath();
  return this.checkNotDeleted(msg).addCallback(function() {
    'use strict';
    return this.parent;
  });
};


/**
 * Return a deferred that will call its errback if this entry has been deleted.
 * In addition, the deferred will only run after a timeout of 0, and all its
 * callbacks will run with the entry as "this".
 *
 * @param {string} action The name of the action being performed. For error
 *     reporting.
 * @return {!goog.async.Deferred} The deferred that will be called after a
 *     timeout of 0.
 * @protected
 */
goog.testing.fs.Entry.prototype.checkNotDeleted = function(action) {
  'use strict';
  const d = new goog.async.Deferred(undefined, this);
  goog.Timer.callOnce(function() {
    'use strict';
    if (this.deleted) {
      const err = new goog.fs.Error({'name': 'NotFoundError'}, action);
      d.errback(err);
    } else {
      d.callback();
    }
  }, 0, this);
  return d;
};



/**
 * A mock directory entry object.
 *
 * @param {!goog.testing.fs.FileSystem} fs The filesystem containing this entry.
 * @param {goog.testing.fs.DirectoryEntry} parent The directory entry directly
 *     containing this entry. If this is null, that means this is the root
 *     directory and so is its own parent.
 * @param {string} name The name of this entry.
 * @param {!Object<!goog.testing.fs.Entry>} children The map of child names to
 *     entry objects.
 * @constructor
 * @extends {goog.testing.fs.Entry}
 * @implements {goog.fs.DirectoryEntry}
 * @final
 */
goog.testing.fs.DirectoryEntry = function(fs, parent, name, children) {
  'use strict';
  goog.testing.fs.DirectoryEntry.base(
      this, 'constructor', fs, parent || this, name);

  /**
   * The map of child names to entry objects.
   * @type {!Object<!goog.testing.fs.Entry>}
   */
  this.children = children;

  /**
   * The modification time of the directory. Measured using Date.now, which may
   * be overridden with mock time providers.
   * @type {number}
   * @private
   */
  this.lastModifiedTimestamp_ = Date.now();
};
goog.inherits(goog.testing.fs.DirectoryEntry, goog.testing.fs.Entry);


/**
 * Constructs and returns the metadata object for this entry.
 * @return {{modificationTime: Date}} The metadata object.
 * @private
 */
goog.testing.fs.DirectoryEntry.prototype.getMetadata_ = function() {
  'use strict';
  return {'modificationTime': new Date(this.lastModifiedTimestamp_)};
};


/** @override */
goog.testing.fs.DirectoryEntry.prototype.isFile = function() {
  'use strict';
  return false;
};


/** @override */
goog.testing.fs.DirectoryEntry.prototype.isDirectory = function() {
  'use strict';
  return true;
};


/** @override */
goog.testing.fs.DirectoryEntry.prototype.getLastModified = function() {
  'use strict';
  const msg = 'reading last modified date for ' + this.getFullPath();
  return this.checkNotDeleted(msg).addCallback(function() {
    'use strict';
    return new Date(this.lastModifiedTimestamp_);
  });
};


/** @override */
goog.testing.fs.DirectoryEntry.prototype.getMetadata = function() {
  'use strict';
  const msg = 'reading metadata for ' + this.getFullPath();
  return this.checkNotDeleted(msg).addCallback(function() {
    'use strict';
    return this.getMetadata_();
  });
};


/** @override */
goog.testing.fs.DirectoryEntry.prototype.clone = function() {
  'use strict';
  return new goog.testing.fs.DirectoryEntry(
      this.getFileSystem(), this.parent, this.getName(), this.children);
};


/** @override */
goog.testing.fs.DirectoryEntry.prototype.remove = function() {
  'use strict';
  if (!goog.object.isEmpty(this.children)) {
    const d = new goog.async.Deferred();
    goog.Timer.callOnce(function() {
      'use strict';
      d.errback(new goog.fs.Error(
          {'name': 'InvalidModificationError'},
          'removing ' + this.getFullPath()));
    }, 0, this);
    return d;
  } else if (this != this.getFileSystem().getRoot()) {
    return goog.testing.fs.DirectoryEntry.base(this, 'remove');
  } else {
    // Root directory, do nothing.
    return goog.async.Deferred.succeed();
  }
};


/** @override */
goog.testing.fs.DirectoryEntry.prototype.getFile = function(
    path, opt_behavior) {
  'use strict';
  const msg = 'loading file ' + path + ' from ' + this.getFullPath();
  opt_behavior = opt_behavior || goog.fs.DirectoryEntry.Behavior.DEFAULT;
  return this.checkNotDeleted(msg).addCallback(function() {
    'use strict';
    try {
      return goog.async.Deferred.succeed(this.getFileSync(path, opt_behavior));
    } catch (e) {
      return goog.async.Deferred.fail(e);
    }
  });
};


/** @override */
goog.testing.fs.DirectoryEntry.prototype.getDirectory = function(
    path, opt_behavior) {
  'use strict';
  const msg = 'loading directory ' + path + ' from ' + this.getFullPath();
  opt_behavior = opt_behavior || goog.fs.DirectoryEntry.Behavior.DEFAULT;
  return this.checkNotDeleted(msg).addCallback(function() {
    'use strict';
    try {
      return goog.async.Deferred.succeed(
          this.getDirectorySync(path, opt_behavior));
    } catch (e) {
      return goog.async.Deferred.fail(e);
    }
  });
};


/**
 * Get a file entry synchronously, without waiting for a Deferred to resolve.
 *
 * @param {string} path The path to the file, relative to this directory.
 * @param {goog.fs.DirectoryEntry.Behavior=} opt_behavior The behavior for
 *     loading the file.
 * @param {string=} opt_data The string data encapsulated by the blob.
 * @param {string=} opt_type The mime type of the blob.
 * @return {!goog.testing.fs.FileEntry} The loaded file.
 */
goog.testing.fs.DirectoryEntry.prototype.getFileSync = function(
    path, opt_behavior, opt_data, opt_type) {
  'use strict';
  opt_behavior = opt_behavior || goog.fs.DirectoryEntry.Behavior.DEFAULT;
  return (
      /** @type {!goog.testing.fs.FileEntry} */ (this.getEntry_(
          path, opt_behavior, true /* isFile */,
          goog.bind(function(parent, name) {
            'use strict';
            return new goog.testing.fs.FileEntry(
                this.getFileSystem(), parent, name,
                opt_data !== undefined ? opt_data : '', opt_type);
          }, this))));
};


/**
 * Creates a file synchronously. This is a shorthand for getFileSync, useful for
 * setting up tests.
 *
 * @param {string} path The path to the file, relative to this directory.
 * @return {!goog.testing.fs.FileEntry} The created file.
 */
goog.testing.fs.DirectoryEntry.prototype.createFileSync = function(path) {
  'use strict';
  return this.getFileSync(path, goog.fs.DirectoryEntry.Behavior.CREATE);
};


/**
 * Get a directory synchronously, without waiting for a Deferred to resolve.
 *
 * @param {string} path The path to the directory, relative to this one.
 * @param {goog.fs.DirectoryEntry.Behavior=} opt_behavior The behavior for
 *     loading the directory.
 * @return {!goog.testing.fs.DirectoryEntry} The loaded directory.
 */
goog.testing.fs.DirectoryEntry.prototype.getDirectorySync = function(
    path, opt_behavior) {
  'use strict';
  opt_behavior = opt_behavior || goog.fs.DirectoryEntry.Behavior.DEFAULT;
  return (
      /** @type {!goog.testing.fs.DirectoryEntry} */ (this.getEntry_(
          path, opt_behavior, false /* isFile */,
          goog.bind(function(parent, name) {
            'use strict';
            return new goog.testing.fs.DirectoryEntry(
                this.getFileSystem(), parent, name, {});
          }, this))));
};


/**
 * Creates a directory synchronously. This is a shorthand for getFileSync,
 * useful for setting up tests.
 *
 * @param {string} path The path to the directory, relative to this directory.
 * @return {!goog.testing.fs.DirectoryEntry} The created directory.
 */
goog.testing.fs.DirectoryEntry.prototype.createDirectorySync = function(path) {
  'use strict';
  return this.getDirectorySync(path, goog.fs.DirectoryEntry.Behavior.CREATE);
};


/**
 * Get a file or directory entry from a path. This handles parsing the path for
 * subdirectories and throwing appropriate errors should something go wrong.
 *
 * @param {string} path The path to the entry, relative to this directory.
 * @param {goog.fs.DirectoryEntry.Behavior} behavior The behavior for loading
 *     the entry.
 * @param {boolean} isFile Whether a file or directory is being loaded.
 * @param {function(!goog.testing.fs.DirectoryEntry, string) :
 *             !goog.testing.fs.Entry} createFn
 *     The function for creating the entry if it doesn't yet exist. This is
 *     passed the parent entry and the name of the new entry.
 * @return {!goog.testing.fs.Entry} The loaded entry.
 * @private
 */
goog.testing.fs.DirectoryEntry.prototype.getEntry_ = function(
    path, behavior, isFile, createFn) {
  'use strict';
  // Filter out leading, trailing, and duplicate slashes.
  const components = path.split('/').filter(goog.functions.identity);

  const basename = /** @type {string} */ (goog.array.peek(components)) || '';
  let dir =
      goog.string.startsWith(path, '/') ? this.getFileSystem().getRoot() : this;

  components.slice(0, -1).forEach(function(p) {
    'use strict';
    const subdir = dir.children[p];
    if (!subdir) {
      throw new goog.fs.Error(
          {'name': 'NotFoundError'},
          'loading ' + path + ' from ' + this.getFullPath() + ' (directory ' +
              dir.getFullPath() + '/' + p + ')');
    }
    dir = subdir;
  }, this);

  // If there is no basename, the path must resolve to the root directory.
  let entry = basename ? dir.children[basename] : dir;

  if (!entry) {
    if (behavior == goog.fs.DirectoryEntry.Behavior.DEFAULT) {
      throw new goog.fs.Error(
          {'name': 'NotFoundError'},
          'loading ' + path + ' from ' + this.getFullPath());
    } else {
      goog.asserts.assert(
          behavior == goog.fs.DirectoryEntry.Behavior.CREATE ||
          behavior == goog.fs.DirectoryEntry.Behavior.CREATE_EXCLUSIVE);
      entry = createFn(dir, basename);
      dir.children[basename] = entry;
      this.lastModifiedTimestamp_ = Date.now();
      return entry;
    }
  } else if (behavior == goog.fs.DirectoryEntry.Behavior.CREATE_EXCLUSIVE) {
    throw new goog.fs.Error(
        {'name': 'InvalidModificationError'},
        'loading ' + path + ' from ' + this.getFullPath());
  } else if (entry.isFile() != isFile) {
    throw new goog.fs.Error(
        {'name': 'TypeMismatchError'},
        'loading ' + path + ' from ' + this.getFullPath());
  } else {
    if (behavior == goog.fs.DirectoryEntry.Behavior.CREATE) {
      this.lastModifiedTimestamp_ = Date.now();
    }
    return entry;
  }
};


/**
 * Returns whether this directory has a child with the given name.
 *
 * @param {string} name The name of the entry to check for.
 * @return {boolean} Whether or not this has a child with the given name.
 */
goog.testing.fs.DirectoryEntry.prototype.hasChild = function(name) {
  'use strict';
  return name in this.children;
};


/** @override */
goog.testing.fs.DirectoryEntry.prototype.removeRecursively = function() {
  'use strict';
  const msg = 'removing ' + this.getFullPath() + ' recursively';
  return this.checkNotDeleted(msg).addCallback(function() {
    'use strict';
    const d = goog.async.Deferred.succeed(null);
    goog.object.forEach(this.children, function(child) {
      'use strict';
      d.awaitDeferred(
          child.isDirectory() ? child.removeRecursively() : child.remove());
    });
    d.addCallback(function() {
      'use strict';
      return this.remove();
    }, this);
    return d;
  });
};


/** @override */
goog.testing.fs.DirectoryEntry.prototype.listDirectory = function() {
  'use strict';
  const msg = 'listing ' + this.getFullPath();
  return this.checkNotDeleted(msg).addCallback(function() {
    'use strict';
    return goog.object.getValues(this.children);
  });
};


/** @override */
goog.testing.fs.DirectoryEntry.prototype.createPath =
    // This isn't really type-safe.
    /** @type {!Function} */ (goog.fs.DirectoryEntryImpl.prototype.createPath);



/**
 * A mock file entry object.
 *
 * @param {!goog.testing.fs.FileSystem} fs The filesystem containing this entry.
 * @param {!goog.testing.fs.DirectoryEntry} parent The directory entry directly
 *     containing this entry.
 * @param {string} name The name of this entry.
 * @param {string} data The data initially contained in the file.
 * @param {string=} opt_type The mime type of the blob.
 * @constructor
 * @extends {goog.testing.fs.Entry}
 * @implements {goog.fs.FileEntry}
 * @final
 */
goog.testing.fs.FileEntry = function(fs, parent, name, data, opt_type) {
  'use strict';
  goog.testing.fs.FileEntry.base(this, 'constructor', fs, parent, name);

  /**
   * The internal file blob referenced by this file entry.
   * @type {!goog.testing.fs.File}
   * @private
   */
  this.file_ =
      new goog.testing.fs.File(name, new Date(Date.now()), data, opt_type);

  /**
   * The metadata for file.
   * @type {{modificationTime: Date}}
   * @private
   */
  this.metadata_ = {'modificationTime': this.file_.lastModifiedDate};
};
goog.inherits(goog.testing.fs.FileEntry, goog.testing.fs.Entry);


/** @override */
goog.testing.fs.FileEntry.prototype.isFile = function() {
  'use strict';
  return true;
};


/** @override */
goog.testing.fs.FileEntry.prototype.isDirectory = function() {
  'use strict';
  return false;
};


/** @override */
goog.testing.fs.FileEntry.prototype.clone = function() {
  'use strict';
  return new goog.testing.fs.FileEntry(
      this.getFileSystem(), this.parent, this.getName(),
      this.fileSync().toString());
};


/** @override */
goog.testing.fs.FileEntry.prototype.getLastModified = function() {
  'use strict';
  return this.file().addCallback(function(file) {
    'use strict';
    return file.lastModifiedDate;
  });
};


/** @override */
goog.testing.fs.FileEntry.prototype.getMetadata = function() {
  'use strict';
  const msg = 'getting metadata for ' + this.getFullPath();
  return this.checkNotDeleted(msg).addCallback(function() {
    'use strict';
    return this.metadata_;
  });
};


/** @override */
goog.testing.fs.FileEntry.prototype.createWriter = function() {
  'use strict';
  const d = new goog.async.Deferred();
  goog.Timer.callOnce(
      goog.bind(d.callback, d, new goog.testing.fs.FileWriter(this)));
  return d;
};

/** @override */
goog.testing.fs.FileEntry.prototype.file = function() {
  'use strict';
  const msg = 'getting file for ' + this.getFullPath();
  return this.checkNotDeleted(msg).addCallback(function() {
    'use strict';
    return this.fileSync();
  });
};


/**
 * Get the internal file representation synchronously, without waiting for a
 * Deferred to resolve.
 *
 * @return {!goog.testing.fs.File} The internal file blob referenced by this
 *     FileEntry.
 */
goog.testing.fs.FileEntry.prototype.fileSync = function() {
  'use strict';
  return this.file_;
};
