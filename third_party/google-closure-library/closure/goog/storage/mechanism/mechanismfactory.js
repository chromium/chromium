/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Provides factory methods for selecting the best storage
 * mechanism, depending on availability and needs.
 */

goog.provide('goog.storage.mechanism.mechanismfactory');

goog.require('goog.storage.mechanism.HTML5LocalStorage');
goog.require('goog.storage.mechanism.HTML5SessionStorage');
goog.require('goog.storage.mechanism.IEUserData');
goog.require('goog.storage.mechanism.PrefixedMechanism');
goog.requireType('goog.storage.mechanism.IterableMechanism');


/**
 * The key to shared userData storage.
 * @type {string}
 */
goog.storage.mechanism.mechanismfactory.USER_DATA_SHARED_KEY =
    'UserDataSharedStore';


/**
 * Returns the best local storage mechanism, or null if unavailable.
 * Local storage means that the database is placed on user's computer.
 * The key-value database is normally shared between all the code paths
 * that request it, so using an optional namespace is recommended. This
 * provides separation and makes key collisions unlikely.
 *
 * @param {string=} opt_namespace Restricts the visibility to given namespace.
 * @return {goog.storage.mechanism.IterableMechanism} Created mechanism or null.
 */
goog.storage.mechanism.mechanismfactory.create = function(opt_namespace) {
  'use strict';
  return goog.storage.mechanism.mechanismfactory.createHTML5LocalStorage(
             opt_namespace) ||
      goog.storage.mechanism.mechanismfactory.createIEUserData(opt_namespace);
};


/**
 * Returns an HTML5 local storage mechanism, or null if unavailable.
 * Since the HTML5 local storage does not support namespaces natively,
 * and the key-value database is shared between all the code paths
 * that request it, it is recommended that an optional namespace is
 * used to provide key separation employing a prefix.
 *
 * @param {string=} opt_namespace Restricts the visibility to given namespace.
 * @return {goog.storage.mechanism.IterableMechanism} Created mechanism or null.
 */
goog.storage.mechanism.mechanismfactory.createHTML5LocalStorage = function(
    opt_namespace) {
  'use strict';
  var storage = new goog.storage.mechanism.HTML5LocalStorage();
  if (storage.isAvailable()) {
    return opt_namespace ?
        new goog.storage.mechanism.PrefixedMechanism(storage, opt_namespace) :
        storage;
  }
  return null;
};


/**
 * Returns an HTML5 session storage mechanism, or null if unavailable.
 * Since the HTML5 session storage does not support namespaces natively,
 * and the key-value database is shared between all the code paths
 * that request it, it is recommended that an optional namespace is
 * used to provide key separation employing a prefix.
 *
 * @param {string=} opt_namespace Restricts the visibility to given namespace.
 * @return {goog.storage.mechanism.IterableMechanism} Created mechanism or null.
 */
goog.storage.mechanism.mechanismfactory.createHTML5SessionStorage = function(
    opt_namespace) {
  'use strict';
  var storage = new goog.storage.mechanism.HTML5SessionStorage();
  if (storage.isAvailable()) {
    return opt_namespace ?
        new goog.storage.mechanism.PrefixedMechanism(storage, opt_namespace) :
        storage;
  }
  return null;
};


/**
 * Returns an IE userData local storage mechanism, or null if unavailable.
 * Using an optional namespace is recommended to provide separation and
 * avoid key collisions.
 *
 * @param {string=} opt_namespace Restricts the visibility to given namespace.
 * @return {goog.storage.mechanism.IterableMechanism} Created mechanism or null.
 */
goog.storage.mechanism.mechanismfactory.createIEUserData = function(
    opt_namespace) {
  'use strict';
  var storage = new goog.storage.mechanism.IEUserData(
      opt_namespace ||
      goog.storage.mechanism.mechanismfactory.USER_DATA_SHARED_KEY);
  if (storage.isAvailable()) {
    return storage;
  }
  return null;
};
