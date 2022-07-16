/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Concrete implementation of the goog.fs.FileSystem interface
 *     using an HTML FileSystem object.
 */
goog.provide('goog.fs.FileSystemImpl');

goog.require('goog.fs.DirectoryEntryImpl');
goog.require('goog.fs.FileSystem');



/**
 * A local filesystem.
 *
 * This shouldn't be instantiated directly. Instead, it should be accessed via
 * {@link goog.fs.getTemporary} or {@link goog.fs.getPersistent}.
 *
 * @param {!FileSystem} fs The underlying FileSystem object.
 * @constructor
 * @implements {goog.fs.FileSystem}
 * @final
 */
goog.fs.FileSystemImpl = function(fs) {
  'use strict';
  /**
   * The underlying FileSystem object.
   *
   * @type {!FileSystem}
   * @private
   */
  this.fs_ = fs;
};


/** @override */
goog.fs.FileSystemImpl.prototype.getName = function() {
  'use strict';
  return this.fs_.name;
};


/** @override */
goog.fs.FileSystemImpl.prototype.getRoot = function() {
  'use strict';
  return new goog.fs.DirectoryEntryImpl(this, this.fs_.root);
};


/**
 * @return {!FileSystem} The underlying FileSystem object.
 */
goog.fs.FileSystemImpl.prototype.getBrowserFileSystem = function() {
  'use strict';
  return this.fs_;
};
