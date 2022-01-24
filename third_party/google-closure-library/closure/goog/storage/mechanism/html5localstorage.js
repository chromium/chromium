/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Provides data persistence using HTML5 local storage
 * mechanism. Local storage must be available under window.localStorage,
 * see: http://www.w3.org/TR/webstorage/#the-localstorage-attribute.
 */

goog.provide('goog.storage.mechanism.HTML5LocalStorage');

goog.require('goog.storage.mechanism.HTML5WebStorage');



/**
 * Provides a storage mechanism that uses HTML5 local storage.
 *
 * @constructor
 * @struct
 * @extends {goog.storage.mechanism.HTML5WebStorage}
 */
goog.storage.mechanism.HTML5LocalStorage = function() {
  'use strict';
  var storage = null;

  try {
    // May throw an exception in cases where the local storage object
    // is visible but access to it is disabled.
    storage = window.localStorage || null;
  } catch (e) {
  }
  goog.storage.mechanism.HTML5LocalStorage.base(this, 'constructor', storage);
};
goog.inherits(
    goog.storage.mechanism.HTML5LocalStorage,
    goog.storage.mechanism.HTML5WebStorage);
