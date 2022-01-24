/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Definition for goog.tweak.Registry.
 * Most clients should not use this class directly, but instead use the API
 * defined in tweak.js. One possible use case for directly using TweakRegistry
 * is to register tweaks that are not known at compile time.
 */

goog.provide('goog.tweak.Registry');

goog.require('goog.array');
goog.require('goog.asserts');
goog.require('goog.log');
goog.require('goog.string');
goog.require('goog.tweak.BasePrimitiveSetting');
goog.require('goog.tweak.BaseSetting');
goog.require('goog.tweak.BooleanSetting');
goog.require('goog.tweak.NumericSetting');
goog.require('goog.tweak.StringSetting');
goog.require('goog.uri.utils');
goog.requireType('goog.tweak.BaseEntry');



/**
 * Singleton that manages all tweaks. This should be instantiated only from
 * goog.tweak.getRegistry().
 * @param {string} queryParams Value of window.location.search.
 * @constructor
 * @final
 */
goog.tweak.Registry = function(queryParams) {
  'use strict';
  /**
   * A map of entry id -> entry object
   * @type {!Object<!goog.tweak.BaseEntry>}
   * @private
   */
  this.entryMap_ = {};

  /**
   * The map of query params to use when initializing entry settings.
   * @type {!Object<string>}
   * @private
   */
  this.parsedQueryParams_ = goog.tweak.Registry.parseQueryParams(queryParams);

  /**
   * List of callbacks to call when a new entry is registered.
   * @type {!Array<!Function>}
   * @private
   */
  this.onRegisterListeners_ = [];
};


/**
 * The logger for this class.
 * @type {goog.log.Logger}
 * @private
 */
goog.tweak.Registry.prototype.logger_ =
    goog.log.getLogger('goog.tweak.Registry');


/**
 * Simple parser for query params. Makes all keys lower-case.
 * @param {string} queryParams The part of the url between the ? and the #.
 * @return {!Object<string>} map of key->value.
 */
goog.tweak.Registry.parseQueryParams = function(queryParams) {
  'use strict';
  // Strip off the leading ? and split on &.
  var parts = queryParams.substr(1).split('&');
  var ret = {};

  for (var i = 0, il = parts.length; i < il; ++i) {
    var entry = parts[i].split('=');
    if (entry[0]) {
      ret[goog.string.urlDecode(entry[0]).toLowerCase()] =
          goog.string.urlDecode(entry[1] || '');
    }
  }
  return ret;
};


/**
 * Registers the given tweak setting/action.
 * @param {goog.tweak.BaseEntry} entry The entry.
 */
goog.tweak.Registry.prototype.register = function(entry) {
  'use strict';
  var id = entry.getId();
  var oldBaseEntry = this.entryMap_[id];
  if (oldBaseEntry) {
    if (oldBaseEntry == entry) {
      goog.log.warning(this.logger_, 'Tweak entry registered twice: ' + id);
      return;
    }
    goog.asserts.fail(
        'Tweak entry registered twice and with different types: ' + id);
  }

  // Set its value from the query params.
  if (entry instanceof goog.tweak.BaseSetting) {
    if (entry.getParamName()) {
      entry.setInitialQueryParamValue(
          this.parsedQueryParams_[entry.getParamName()]);
    }
  }

  this.entryMap_[id] = entry;
  // Call all listeners.
  for (var i = 0, callback; callback = this.onRegisterListeners_[i]; ++i) {
    callback(entry);
  }
};


/**
 * Adds a callback to be called whenever a new tweak is added.
 * @param {!Function} func The callback.
 */
goog.tweak.Registry.prototype.addOnRegisterListener = function(func) {
  'use strict';
  this.onRegisterListeners_.push(func);
};


/**
 * @param {string} id The unique string that identifies this entry.
 * @return {boolean} Whether a tweak with the given ID is registered.
 */
goog.tweak.Registry.prototype.hasEntry = function(id) {
  'use strict';
  return id in this.entryMap_;
};


/**
 * Returns the BaseEntry with the given ID. Asserts if it does not exists.
 * @param {string} id The unique string that identifies this entry.
 * @return {!goog.tweak.BaseEntry} The entry.
 */
goog.tweak.Registry.prototype.getEntry = function(id) {
  'use strict';
  var ret = this.entryMap_[id];
  goog.asserts.assert(ret, 'Tweak not registered: %s', id);
  return ret;
};


/**
 * Returns the boolean setting with the given ID. Asserts if the ID does not
 * refer to a registered entry or if it refers to one of the wrong type.
 * @param {string} id The unique string that identifies this entry.
 * @return {!goog.tweak.BooleanSetting} The entry.
 */
goog.tweak.Registry.prototype.getBooleanSetting = function(id) {
  'use strict';
  var entry = this.getEntry(id);
  goog.asserts.assertInstanceof(
      entry, goog.tweak.BooleanSetting,
      'getBooleanSetting called on wrong type of BaseSetting');
  return /** @type {!goog.tweak.BooleanSetting} */ (entry);
};


/**
 * Returns the string setting with the given ID. Asserts if the ID does not
 * refer to a registered entry or if it refers to one of the wrong type.
 * @param {string} id The unique string that identifies this entry.
 * @return {!goog.tweak.StringSetting} The entry.
 */
goog.tweak.Registry.prototype.getStringSetting = function(id) {
  'use strict';
  var entry = this.getEntry(id);
  goog.asserts.assertInstanceof(
      entry, goog.tweak.StringSetting,
      'getStringSetting called on wrong type of BaseSetting');
  return /** @type {!goog.tweak.StringSetting} */ (entry);
};


/**
 * Returns the numeric setting with the given ID. Asserts if the ID does not
 * refer to a registered entry or if it refers to one of the wrong type.
 * @param {string} id The unique string that identifies this entry.
 * @return {!goog.tweak.NumericSetting} The entry.
 */
goog.tweak.Registry.prototype.getNumericSetting = function(id) {
  'use strict';
  var entry = this.getEntry(id);
  goog.asserts.assertInstanceof(
      entry, goog.tweak.NumericSetting,
      'getNumericSetting called on wrong type of BaseSetting');
  return /** @type {!goog.tweak.NumericSetting} */ (entry);
};


/**
 * Creates and returns an array of all BaseSetting objects with an associted
 * query parameter.
 * @param {boolean} excludeChildEntries Exclude BooleanInGroupSettings.
 * @param {boolean} excludeNonSettings Exclude entries that are not subclasses
 *     of BaseSetting.
 * @return {!Array<!goog.tweak.BaseSetting>} The settings.
 */
goog.tweak.Registry.prototype.extractEntries = function(
    excludeChildEntries, excludeNonSettings) {
  'use strict';
  var entries = [];
  for (var id in this.entryMap_) {
    var entry = this.entryMap_[id];
    if (entry instanceof goog.tweak.BaseSetting) {
      if (excludeChildEntries && !entry.getParamName()) {
        continue;
      }
    } else if (excludeNonSettings) {
      continue;
    }
    entries.push(entry);
  }
  return entries;
};


/**
 * Returns the query part of the URL that will apply all set tweaks.
 * @param {string=} opt_existingSearchStr The part of the url between the ? and
 *     the #. Uses window.location.search if not given.
 * @return {string} The query string.
 */
goog.tweak.Registry.prototype.makeUrlQuery = function(opt_existingSearchStr) {
  'use strict';
  var existingParams = opt_existingSearchStr == undefined ?
      window.location.search :
      opt_existingSearchStr;

  var sortedEntries = this.extractEntries(
      true /* excludeChildEntries */, true /* excludeNonSettings */);
  // Sort the params so that the urlQuery has stable ordering.
  sortedEntries.sort(function(a, b) {
    'use strict';
    return goog.array.defaultCompare(a.getParamName(), b.getParamName());
  });

  // Add all values that are not set to their defaults.
  var keysAndValues = [];
  for (var i = 0, entry; entry = sortedEntries[i]; ++i) {
    var encodedValue = entry.getNewValueEncoded();
    if (encodedValue != null) {
      keysAndValues.push(entry.getParamName(), encodedValue);
    }
    // Strip all tweak query params from the existing query string. This will
    // make the final query string contain only the tweak settings that are set
    // to their non-default values and also maintain non-tweak related query
    // parameters.
    existingParams = goog.uri.utils.removeParam(
        existingParams,
        encodeURIComponent(/** @type {string} */ (entry.getParamName())));
  }

  var tweakParams = goog.uri.utils.buildQueryData(keysAndValues);
  // Decode spaces and commas in order to make the URL more readable.
  tweakParams = tweakParams.replace(/%2C/g, ',').replace(/%20/g, '+');
  return !tweakParams ? existingParams : existingParams ?
                        existingParams + '&' + tweakParams :
                        '?' + tweakParams;
};

