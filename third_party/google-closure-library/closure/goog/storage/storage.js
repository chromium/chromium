/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Provides a convenient API for data persistence using a selected
 * data storage mechanism.
 */

goog.provide('goog.storage.Storage');

goog.require('goog.json');
goog.require('goog.storage.ErrorCode');
goog.requireType('goog.storage.mechanism.Mechanism');



/**
 * The base implementation for all storage APIs.
 *
 * @param {!goog.storage.mechanism.Mechanism} mechanism The underlying
 *     storage mechanism.
 * @constructor
 * @struct
 */
goog.storage.Storage = function(mechanism) {
  'use strict';
  /**
   * The mechanism used to persist key-value pairs.
   *
   * @protected {goog.storage.mechanism.Mechanism}
   */
  this.mechanism = mechanism;
};


/**
 * Sets an item in the data storage.
 *
 * @param {string} key The key to set.
 * @param {*} value The value to serialize to a string and save.
 */
goog.storage.Storage.prototype.set = function(key, value) {
  'use strict';
  if (value === undefined) {
    this.mechanism.remove(key);
    return;
  }
  this.mechanism.set(key, goog.json.serialize(value));
};


/**
 * Gets an item from the data storage.
 *
 * @param {string} key The key to get.
 * @return {*} Deserialized value or undefined if not found.
 */
goog.storage.Storage.prototype.get = function(key) {
  'use strict';
  let json;
  try {
    json = this.mechanism.get(key);
  } catch (e) {
    // If, for any reason, the value returned by a mechanism's get method is not
    // a string, an exception is thrown.  In this case, we must fail gracefully
    // instead of propagating the exception to clients.  See b/8095488 for
    // details.
    return undefined;
  }
  if (json === null) {
    return undefined;
  }

  try {
    return JSON.parse(json);
  } catch (e) {
    throw goog.storage.ErrorCode.INVALID_VALUE;
  }
};


/**
 * Removes an item from the data storage.
 *
 * @param {string} key The key to remove.
 */
goog.storage.Storage.prototype.remove = function(key) {
  'use strict';
  this.mechanism.remove(key);
};
