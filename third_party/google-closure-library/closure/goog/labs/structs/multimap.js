/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A Map that associates multiple values with a single key.
 */

goog.provide('goog.labs.structs.Multimap');

goog.require('goog.array');



/**
 * Creates a new multimap.
 * @final
 * @template K, V
 */
goog.labs.structs.Multimap = class {
  constructor() {
    this.clear();
  }

  /**
   * Clears the multimap.
   */
  clear() {
    this.count_ = 0;
    this.map_ = new Map();
  }

  /**
   * Clones this multimap.
   * @return {!goog.labs.structs.Multimap<K, V>} A multimap that contains all
   *     the mapping this multimap has.
   */
  clone() {
    const map = new goog.labs.structs.Multimap();
    map.addAllFromMultimap(this);
    return map;
  }

  /**
   * Adds the given (key, value) pair to the map. The (key, value) pair
   * is guaranteed to be added.
   * @param {K} key The key to add.
   * @param {V} value The value to add.
   */
  add(key, value) {
    let values = this.map_.get(key);
    if (!values) {
      this.map_.set(key, (values = []));
    }

    values.push(value);
    this.count_++;
  }

  /**
   * Stores a collection of values to the given key. Does not replace
   * existing (key, value) pairs.
   * @param {K} key The key to add.
   * @param {!Array<V>} values The values to add.
   */
  addAllValues(key, values) {
    values.forEach(function(v) {
      'use strict';
      this.add(key, v);
    }, this);
  }

  /**
   * Adds the contents of the given map/multimap to this multimap.
   * @param {!goog.labs.structs.Multimap<K, V>} map The
   *     map to add.
   */
  addAllFromMultimap(map) {
    map.getEntries().forEach(function(entry) {
      'use strict';
      this.add(entry[0], entry[1]);
    }, this);
  }

  /**
   * Replaces all the values for the given key with the given values.
   * @param {K} key The key whose values are to be replaced.
   * @param {!Array<V>} values The new values. If empty, this is
   *     equivalent to `removeAll(key)`.
   */
  replaceValues(key, values) {
    this.removeAll(key);
    this.addAllValues(key, values);
  }

  /**
   * Gets the values correspond to the given key.
   * @param {K} key The key to retrieve.
   * @return {!Array<V>} An array of values corresponding to the given
   *     key. May be empty. Note that the ordering of values are not
   *     guaranteed to be consistent.
   */
  get(key) {
    const values = this.map_.get(key);
    return values ? goog.array.clone(values) : [];
  }

  /**
   * Removes a single occurrence of (key, value) pair.
   * @param {K} key The key to remove.
   * @param {V} value The value to remove.
   * @return {boolean} Whether any matching (key, value) pair is removed.
   */
  remove(key, value) {
    const values = this.map_.get(key);
    if (!values) {
      return false;
    }

    const removed = goog.array.removeIf(values, function(v) {
      'use strict';
      return Object.is(value, v);
    });

    if (removed) {
      this.count_--;
      if (values.length == 0) {
        this.map_.delete(key);
      }
    }
    return removed;
  }

  /**
   * Removes all values corresponding to the given key.
   * @param {K} key The key whose values are to be removed.
   * @return {boolean} Whether any value is removed.
   */
  removeAll(key) {
    // We have to first retrieve the values from the backing map because
    // we need to keep track of count (and correctly calculates the
    // return value). values may be undefined.
    const values = this.map_.get(key);
    if (this.map_.delete(key)) {
      this.count_ -= values.length;
      return true;
    }

    return false;
  }

  /**
   * @return {boolean} Whether the multimap is empty.
   */
  isEmpty() {
    return !this.count_;
  }

  /**
   * @return {number} The count of (key, value) pairs in the map.
   */
  getCount() {
    return this.count_;
  }

  /**
   * @param {K} key The key to check.
   * @param {V} value The value to check.
   * @return {boolean} Whether the (key, value) pair exists in the multimap.
   */
  containsEntry(key, value) {
    const values = this.map_.get(key);
    if (!values) {
      return false;
    }

    const index = values.findIndex(function(v) {
      'use strict';
      return Object.is(v, value);
    });
    return index >= 0;
  }

  /**
   * @param {K} key The key to check.
   * @return {boolean} Whether the multimap contains at least one (key,
   *     value) pair with the given key.
   */
  containsKey(key) {
    return this.getKeys().includes(key);
  }

  /**
   * @param {V} value The value to check.
   * @return {boolean} Whether the multimap contains at least one (key,
   *     value) pair with the given value.
   */
  containsValue(value) {
    return this.getValues().includes(value);
  }

  /**
   * @return {!Array<K>} An array of unique keys.
   */
  getKeys() {
    return [...this.map_.keys()];
  }

  /**
   * @return {!Array<V>} An array of values. There may be duplicates.
   */
  getValues() {
    return goog.array.flatten([...this.map_.values()]);
  }

  /**
   * @return {!Array<!Array<K|V>>} An array of entries. Each entry is of the
   *     form [key, value].
   */
  getEntries() {
    const keys = this.getKeys();
    const entries = [];
    for (let i = 0; i < keys.length; i++) {
      const key = keys[i];
      const values = this.get(key);
      for (let j = 0; j < values.length; j++) {
        entries.push([key, values[j]]);
      }
    }
    return entries;
  }
};


/**
 * The backing map.
 * @private {!Map<K, !Array<V>>}
 */
goog.labs.structs.Multimap.prototype.map_;


/**
 * @private {number}
 */
goog.labs.structs.Multimap.prototype.count_ = 0;
