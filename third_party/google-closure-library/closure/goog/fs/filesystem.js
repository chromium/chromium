/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A wrapper for the HTML5 FileSystem object.
 */

goog.provide('goog.fs.FileSystem');

goog.requireType('goog.fs.DirectoryEntry');



/**
 * A local filesystem.
 *
 * @interface
 */
goog.fs.FileSystem = function() {};


/**
 * @return {string} The name of the filesystem.
 */
goog.fs.FileSystem.prototype.getName = function() {};


/**
 * @return {!goog.fs.DirectoryEntry} The root directory of the filesystem.
 */
goog.fs.FileSystem.prototype.getRoot = function() {};
