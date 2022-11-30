/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Provides a convenient API for data persistence with expiration.
 */

goog.provide('goog.storage.ExpiringStorage');

goog.require('goog.storage.RichStorage');
goog.requireType('goog.storage.mechanism.Mechanism');



/**
 * Provides a storage with expiring keys.
 *
 * @param {!goog.storage.mechanism.Mechanism} mechanism The underlying
 *     storage mechanism.
 * @constructor
 * @struct
 * @extends {goog.storage.RichStorage}
 */
goog.storage.ExpiringStorage = function(mechanism) {
  'use strict';
  goog.storage.ExpiringStorage.base(this, 'constructor', mechanism);
};
goog.inherits(goog.storage.ExpiringStorage, goog.storage.RichStorage);


/**
 * Metadata key under which the expiration time is stored.
 *
 * @type {string}
 * @protected
 */
goog.storage.ExpiringStorage.EXPIRATION_TIME_KEY = 'expiration';


/**
 * Metadata key under which the creation time is stored.
 *
 * @type {string}
 * @protected
 */
goog.storage.ExpiringStorage.CREATION_TIME_KEY = 'creation';


/**
 * Returns the wrapper creation time.
 *
 * @param {!Object} wrapper The wrapper.
 * @return {number|undefined} Wrapper creation time.
 */
goog.storage.ExpiringStorage.getCreationTime = function(wrapper) {
  'use strict';
  return wrapper[goog.storage.ExpiringStorage.CREATION_TIME_KEY];
};


/**
 * Returns the wrapper expiration time.
 *
 * @param {!Object} wrapper The wrapper.
 * @return {number|undefined} Wrapper expiration time.
 */
goog.storage.ExpiringStorage.getExpirationTime = function(wrapper) {
  'use strict';
  return wrapper[goog.storage.ExpiringStorage.EXPIRATION_TIME_KEY];
};


/**
 * Checks if the data item has expired.
 *
 * @param {!Object} wrapper The wrapper.
 * @return {boolean} True if the item has expired.
 */
goog.storage.ExpiringStorage.isExpired = function(wrapper) {
  'use strict';
  const creation = goog.storage.ExpiringStorage.getCreationTime(wrapper);
  const expiration = goog.storage.ExpiringStorage.getExpirationTime(wrapper);
  return !!expiration && expiration < goog.now() ||
      !!creation && creation > goog.now();
};


/**
 * Set an item in the storage.
 *
 * @param {string} key The key to set.
 * @param {*} value The value to serialize to a string and save.
 * @param {number=} opt_expiration The number of miliseconds since epoch
 *     (as in goog.now()) when the value is to expire. If the expiration
 *     time is not provided, the value will persist as long as possible.
 * @override
 */
goog.storage.ExpiringStorage.prototype.set = function(
    key, value, opt_expiration) {
  'use strict';
  const wrapper = goog.storage.RichStorage.Wrapper.wrapIfNecessary(value);
  if (wrapper) {
    if (opt_expiration) {
      if (opt_expiration < goog.now()) {
        goog.storage.ExpiringStorage.prototype.remove.call(this, key);
        return;
      }
      wrapper[goog.storage.ExpiringStorage.EXPIRATION_TIME_KEY] =
          opt_expiration;
    }
    wrapper[goog.storage.ExpiringStorage.CREATION_TIME_KEY] = goog.now();
  }
  goog.storage.ExpiringStorage.base(this, 'set', key, wrapper);
};


/**
 * Get an item wrapper (the item and its metadata) from the storage.
 *
 * @param {string} key The key to get.
 * @param {boolean=} opt_expired If true, return expired wrappers as well.
 * @return {(!Object|undefined)} The wrapper, or undefined if not found.
 * @override
 */
goog.storage.ExpiringStorage.prototype.getWrapper = function(key, opt_expired) {
  'use strict';
  const wrapper = goog.storage.ExpiringStorage.base(this, 'getWrapper', key);
  if (!wrapper) {
    return undefined;
  }
  if (!opt_expired && goog.storage.ExpiringStorage.isExpired(wrapper)) {
    goog.storage.ExpiringStorage.prototype.remove.call(this, key);
    return undefined;
  }
  return wrapper;
};
