/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Datastructure: Hash Map.
 *
 * This file provides a goog.structs.Map interface based on native Map.
 */
goog.module('goog.ui.Map');
goog.module.declareLegacyNamespace();


/**
 * Class for Hash Map datastructure.
 * @param {*=} map Map or Object to initialize the map with.
 * @constructor
 * @template K, V
 */
const UiMap = function(map = undefined) {
  /** @private @const {!Map<K, V>} */
  this.map_ = new Map();

  const argLength = arguments.length;

  if (argLength > 1) {
    if (argLength % 2) {
      throw new Error('Uneven number of arguments');
    }
    for (let i = 0; i < argLength; i += 2) {
      this.set(arguments[i], arguments[i + 1]);
    }
  } else if (map) {
    this.addAll(/** @type {!Object} */ (map));
  }
};


/**
 * @return {number} The number of key-value pairs in the map.
 */
UiMap.prototype.getCount = function() {
  return this.map_.size;
};


/**
 * Returns the values of the map.
 * @return {!Array<V>} The values in the map.
 */
UiMap.prototype.getValues = function() {
  return Array.from(this.map_.values());
};

/**
 * Returns the keys of the map.
 * @return {!Array<K>} Array of string values.
 */
UiMap.prototype.getKeys = function() {
  return Array.from(this.map_.keys());
};

/**
 * Whether the map contains the given key.
 * @param {K} key The key to check for.
 * @return {boolean} Whether the map contains the key.
 */
UiMap.prototype.containsKey = function(key) {
  return this.map_.has(key);
};


/**
 * Whether the map contains the given value. This is O(n).
 * @param {V} val The value to check for.
 * @return {boolean} Whether the map contains the value.
 */
UiMap.prototype.containsValue = function(val) {
  // NOTE: goog.structs.Map uses == instead of ===.
  return this.getValues().some((v) => v == val);
};

/**
 * Whether this map is equal to the argument map.
 * @param {!UiMap} otherMap The map against which to test equality.
 * @param {function(V, V): boolean=} equalityFn Optional equality function
 *     to test equality of values. If not specified, this will test whether
 *     the values contained in each map are identical objects.
 * @return {boolean} Whether the maps are equal.
 */
UiMap.prototype.equals = function(
    otherMap, equalityFn = (a, b) => a === b) {
  if (this === otherMap) {
    return true;
  }
  if (this.map_.size != otherMap.getCount()) {
    return false;
  }

  return this.getKeys().every((key) => {
    return equalityFn(this.map_.get(key), otherMap.get(key));
  });
};


/**
 * @return {boolean} Whether the map is empty.
 */
UiMap.prototype.isEmpty = function() {
  return this.map_.size == 0;
};


/**
 * Removes all key-value pairs from the map.
 */
UiMap.prototype.clear = function() {
  this.map_.clear();
};


/**
 * Removes a key-value pair based on the key. This is O(logN) amortized due to
 * updating the keys array whenever the count becomes half the size of the keys
 * in the keys array.
 * @param {K} key  The key to remove.
 * @return {boolean} Whether object was removed.
 */
UiMap.prototype.remove = function(key) {
  return this.map_.delete(key);
};


/**
 * Returns the value for the given key.  If the key is not found and the default
 * value is not given this will return `undefined`.
 * @param {*} key The key to get the value for.
 * @param {DEFAULT=} defaultValue The value to return if no item is found for
 *     the given key, defaults to undefined.
 * @return {V|DEFAULT} The value for the given key.
 * @template DEFAULT
 */
UiMap.prototype.get = function(key, defaultValue = undefined) {
  if (this.map_.has(key)) {
    return this.map_.get(key);
  }
  return defaultValue;
};


/**
 * Adds a key-value pair to the map.
 * @param {*} key The key.
 * @param {V} value The value to add.
 * @return {!THIS} Some subclasses return a value.
 * @this {THIS}
 * @template THIS
 */
UiMap.prototype.set = function(key, value) {
  const self = /** @type {!UiMap} */ (this);
  self.map_.set(key, value);
  return self;
};


/**
 * Adds multiple key-value pairs from another goog.ui.Map or Object.
 * @param {!Object<K, V>} map Object containing the data to add.
 */
UiMap.prototype.addAll = function(map) {
  if (map instanceof UiMap) {
    for (const [key, val] of map.map_) {
      this.map_.set(key, val);
    }
  } else if (!!map) {
    for (const [key, val] of Object.entries(map)) {
      this.map_.set(key, val);
    }
  }
};


/**
 * Calls the given function on each entry in the map.
 * @param {function(this:T, V, K, (!Map|!UiMap<K,V>|null))} callbackFn
 * @param {T=} thisArg The value of "this" inside callbackFn.
 * @template T
 */
UiMap.prototype.forEach = function(callbackFn, thisArg = this) {
  this.map_.forEach((val, key) => callbackFn.call(thisArg, val, key, this));
};


/**
 * Clones a map and returns a new map.
 * @return {!UiMap} A new map with the same key-value pairs.
 */
UiMap.prototype.clone = function() {
  return new UiMap(this);
};


/**
 * @return {!Object} Object representation of the map.
 */
UiMap.prototype.toObject = function() {
  const obj = {};
  for (const [key, val] of this.map_) {
    obj[key] = val;
  }
  return obj;
};

exports = UiMap;
