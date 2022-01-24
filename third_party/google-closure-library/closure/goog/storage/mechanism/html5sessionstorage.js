/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Provides data persistence using HTML5 session storage
 * mechanism. Session storage must be available under window.sessionStorage,
 * see: http://www.w3.org/TR/webstorage/#the-sessionstorage-attribute.
 */

goog.provide('goog.storage.mechanism.HTML5SessionStorage');

goog.require('goog.storage.mechanism.HTML5WebStorage');



/**
 * Provides a storage mechanism that uses HTML5 session storage.
 *
 * @constructor
 * @struct
 * @extends {goog.storage.mechanism.HTML5WebStorage}
 */
goog.storage.mechanism.HTML5SessionStorage = function() {
  'use strict';
  var storage = null;

  try {
    // May throw an exception in cases where the session storage object is
    // visible but access to it is disabled. For example, accessing the file
    // in local mode in Firefox throws 'Operation is not supported' exception.
    storage = window.sessionStorage || null;
  } catch (e) {
  }
  goog.storage.mechanism.HTML5SessionStorage.base(this, 'constructor', storage);
};
goog.inherits(
    goog.storage.mechanism.HTML5SessionStorage,
    goog.storage.mechanism.HTML5WebStorage);
