/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Mock file object.
 */

goog.setTestOnly('goog.testing.fs.File');
goog.provide('goog.testing.fs.File');

goog.require('goog.testing.fs.Blob');



/**
 * A mock file object.
 *
 * @param {string} name The name of the file.
 * @param {Date=} opt_lastModified The last modified date for this file. May be
 *     null if file modification dates are not supported.
 * @param {string=} opt_data The string data encapsulated by the blob.
 * @param {string=} opt_type The mime type of the blob.
 * @constructor
 * @extends {goog.testing.fs.Blob}
 * @final
 */
goog.testing.fs.File = function(name, opt_lastModified, opt_data, opt_type) {
  'use strict';
  goog.testing.fs.File.base(this, 'constructor', opt_data, opt_type);

  /**
   * @see http://www.w3.org/TR/FileAPI/#dfn-name
   * @type {string}
   */
  this.name = name;

  /**
   * @see http://www.w3.org/TR/FileAPI/#dfn-lastModifiedDate
   * @type {Date}
   */
  this.lastModifiedDate = opt_lastModified || null;
};
goog.inherits(goog.testing.fs.File, goog.testing.fs.Blob);
