/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Mock filesystem object.
 */

goog.setTestOnly('goog.testing.fs.FileSystem');
goog.provide('goog.testing.fs.FileSystem');

goog.require('goog.fs.FileSystem');
goog.require('goog.testing.fs.DirectoryEntry');



/**
 * A mock filesystem object.
 *
 * @param {string=} opt_name The name of the filesystem.
 * @constructor
 * @implements {goog.fs.FileSystem}
 * @final
 */
goog.testing.fs.FileSystem = function(opt_name) {
  'use strict';
  /**
   * The name of the filesystem.
   * @type {string}
   * @private
   */
  this.name_ = opt_name || 'goog.testing.fs.FileSystem';

  /**
   * The root entry of the filesystem.
   * @type {!goog.testing.fs.DirectoryEntry}
   * @private
   */
  this.root_ = new goog.testing.fs.DirectoryEntry(this, null, '', {});
};


/** @override */
goog.testing.fs.FileSystem.prototype.getName = function() {
  'use strict';
  return this.name_;
};


/**
 * @override
 * @return {!goog.testing.fs.DirectoryEntry}
 */
goog.testing.fs.FileSystem.prototype.getRoot = function() {
  'use strict';
  return this.root_;
};
