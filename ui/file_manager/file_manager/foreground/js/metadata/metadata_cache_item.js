// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

import {MetadataItem} from './metadata_item.js';

/**
 * Cache of metadata for a FileEntry.
 */
export class MetadataCacheItem {
  constructor() {
    /**
     * Map of property name and MetadataCacheItemProperty.
     * @private @type {!Record<string, !MetadataCacheItemProperty>}
     * @const
     */
    this.properties_ = {};
  }

  /**
   * Creates requested names that need to be loaded.
   * @param {!Array<string>} names
   * @return {!Array<string>} Property names that need to be loaded.
   */
  createRequests(names) {
    const loadRequested = [];
    for (let i = 0; i < names.length; i++) {
      const name = names[i];
      // @ts-ignore: error TS2345: Argument of type 'string | undefined' is not
      // assignable to parameter of type 'string'.
      assert(!/Error$/.test(name));
      // Check if the property needs to be updated.
      // @ts-ignore: error TS2538: Type 'undefined' cannot be used as an index
      // type.
      if (this.properties_[name] &&
          // @ts-ignore: error TS2538: Type 'undefined' cannot be used as an
          // index type.
          this.properties_[name].state !==
              MetadataCacheItemPropertyState.INVALIDATED) {
        continue;
      }
      loadRequested.push(name);
    }
    // @ts-ignore: error TS2322: Type '(string | undefined)[]' is not assignable
    // to type 'string[]'.
    return loadRequested;
  }

  /**
   * Marks the given properties as loading.
   * @param {number} requestId
   * @param {!Array<string>} names
   */
  startRequests(requestId, names) {
    for (let i = 0; i < names.length; i++) {
      const name = names[i];
      // @ts-ignore: error TS2345: Argument of type 'string | undefined' is not
      // assignable to parameter of type 'string'.
      assert(!/Error$/.test(name));
      // @ts-ignore: error TS2538: Type 'undefined' cannot be used as an index
      // type.
      if (!this.properties_[name]) {
        // @ts-ignore: error TS2538: Type 'undefined' cannot be used as an index
        // type.
        this.properties_[name] = new MetadataCacheItemProperty();
      }
      // @ts-ignore: error TS2538: Type 'undefined' cannot be used as an index
      // type.
      this.properties_[name].requestId = requestId;
      // @ts-ignore: error TS2538: Type 'undefined' cannot be used as an index
      // type.
      this.properties_[name].state = MetadataCacheItemPropertyState.LOADING;
    }
  }

  /**
   * Feeds the result of startRequests.
   * @param {number} requestId Request ID passed when calling startRequests.
   * @param {!MetadataItem} typedObject Map of property name and value.
   * @return {boolean} Whether at least one property is updated or not.
   */
  storeProperties(requestId, typedObject) {
    let changed = false;
    const object = /** @type {!Object} */ (typedObject);
    for (const name in object) {
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'string' can't be used to index type 'Object'.
      if (/.Error$/.test(name) && object[name]) {
        // @ts-ignore: error TS7053: Element implicitly has an 'any' type
        // because expression of type 'string' can't be used to index type
        // 'Object'.
        object[name.substr(0, name.length - 5)] = undefined;
      }
    }
    for (const name in object) {
      if (/.Error$/.test(name)) {
        continue;
      }
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'string' can't be used to index type '{}'.
      if (!this.properties_[name]) {
        // @ts-ignore: error TS7053: Element implicitly has an 'any' type
        // because expression of type 'string' can't be used to index type '{}'.
        this.properties_[name] = new MetadataCacheItemProperty();
      }
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'string' can't be used to index type '{}'.
      if (requestId < this.properties_[name].requestId ||
          // @ts-ignore: error TS7053: Element implicitly has an 'any' type
          // because expression of type 'string' can't be used to index type
          // '{}'.
          this.properties_[name].state ===
              MetadataCacheItemPropertyState.FULFILLED) {
        continue;
      }
      changed = true;
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'string' can't be used to index type '{}'.
      this.properties_[name].requestId = requestId;
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'string' can't be used to index type 'Object'.
      this.properties_[name].value = object[name];
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'string' can't be used to index type 'Object'.
      this.properties_[name].error = object[name + 'Error'];
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'string' can't be used to index type '{}'.
      this.properties_[name].state = MetadataCacheItemPropertyState.FULFILLED;
    }
    return changed;
  }

  /**
   * Marks the caches of all properties in the item as invalidates and forces to
   * reload at the next time of startRequests. Optionally, takes an array of
   * names and only invalidates those.
   * @param {number} requestId Request ID of the invalidation request. This must
   *     be larger than other requests ID passed to the item before.
   * @param {!Array<string>} [names]
   */
  invalidate(requestId, names) {
    // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
    // expression of type 'string' can't be used to index type '{}'.
    const namesToInvalidate = names ? names.filter(n => this.properties_[n]) :
                                      Object.keys(this.properties_);
    for (const name of namesToInvalidate) {
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'string' can't be used to index type '{}'.
      assert(this.properties_[name].requestId < requestId);
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'string' can't be used to index type '{}'.
      this.properties_[name].requestId = requestId;
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'string' can't be used to index type '{}'.
      this.properties_[name].state = MetadataCacheItemPropertyState.INVALIDATED;
    }
  }

  /**
   * Obtains property for entries and names.
   * Note that it returns invalidated properties also.
   * @param {!Array<string>} names
   * @return {!MetadataItem}
   */
  get(names) {
    const result = /** @type {!Object} */ (new MetadataItem());
    for (let i = 0; i < names.length; i++) {
      const name = names[i];
      // @ts-ignore: error TS2345: Argument of type 'string | undefined' is not
      // assignable to parameter of type 'string'.
      assert(!/Error$/.test(name));
      // @ts-ignore: error TS2538: Type 'undefined' cannot be used as an index
      // type.
      if (this.properties_[name]) {
        // @ts-ignore: error TS2538: Type 'undefined' cannot be used as an index
        // type.
        result[name] = this.properties_[name].value;
        // @ts-ignore: error TS2538: Type 'undefined' cannot be used as an index
        // type.
        result[name + 'Error'] = this.properties_[name].error;
      }
    }
    return /** @type {!MetadataItem} */ (result);
  }

  /**
   * Creates deep copy of the item.
   * @return {!MetadataCacheItem}
   */
  clone() {
    const clonedItem = new MetadataCacheItem();
    for (const name in this.properties_) {
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'string' can't be used to index type '{}'.
      const property = this.properties_[name];
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'string' can't be used to index type '{}'.
      clonedItem.properties_[name] = new MetadataCacheItemProperty();
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'string' can't be used to index type '{}'.
      clonedItem.properties_[name].value = property.value;
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'string' can't be used to index type '{}'.
      clonedItem.properties_[name].error = property.error;
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'string' can't be used to index type '{}'.
      clonedItem.properties_[name].requestId = property.requestId;
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'string' can't be used to index type '{}'.
      clonedItem.properties_[name].state = property.state;
    }
    return clonedItem;
  }

  /**
   * Returns whether all the given properties are fulfilled.
   * @param {!Array<string>} names Property names.
   * @return {boolean}
   */
  hasFreshCache(names) {
    for (let i = 0; i < names.length; i++) {
      // @ts-ignore: error TS2538: Type 'undefined' cannot be used as an index
      // type.
      if (!(this.properties_[names[i]] &&
            // @ts-ignore: error TS2538: Type 'undefined' cannot be used as an
            // index type.
            this.properties_[names[i]].state ===
                MetadataCacheItemPropertyState.FULFILLED)) {
        return false;
      }
    }
    return true;
  }
}

/**
 * @enum {string}
 */
const MetadataCacheItemPropertyState = {
  INVALIDATED: 'invalidated',
  LOADING: 'loading',
  FULFILLED: 'fulfilled',
};

/**
 * Cache of metadata for a property.
 */
export class MetadataCacheItemProperty {
  constructor() {
    /**
     * Cached value of property.
     * @public @type {*}
     */
    this.value = null;

    /**
     * @public @type {?Error}
     */
    this.error = null;

    /**
     * Last request ID.
     * @public @type {number}
     */
    this.requestId = -1;

    /**
     * Cache state of the property.
     * @public @type {MetadataCacheItemPropertyState}
     */
    this.state = MetadataCacheItemPropertyState.INVALIDATED;
  }
}
