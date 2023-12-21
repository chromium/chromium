// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {MetadataItem, type MetadataKey} from './metadata_item.js';

/**
 * Cache of metadata for a FileEntry.
 */
export class MetadataCacheItem {
  /**
   * Map of property name and MetadataCacheItemProperty.
   */
  private readonly properties_:
      Partial<Record<MetadataKey, MetadataCacheItemProperty>> = {};

  /**
   * Creates requested names that need to be loaded.
   * @return Property names that need to be loaded.
   */
  createRequests(names: MetadataKey[]): MetadataKey[] {
    const loadRequested: MetadataKey[] = [];
    for (const name of names) {
      assert(!/Error$/.test(name));
      // Check if the property needs to be updated.
      const property = this.properties_[name];
      if (property &&
          property.state !== MetadataCacheItemPropertyState.INVALIDATED) {
        continue;
      }
      loadRequested.push(name);
    }
    return loadRequested;
  }

  /**
   * Marks the given properties as loading.
   */
  startRequests(requestId: number, names: MetadataKey[]) {
    for (const name of names) {
      assert(!/Error$/.test(name));
      if (!this.properties_[name]) {
        this.properties_[name] = new MetadataCacheItemProperty();
      }
      this.properties_[name]!.requestId = requestId;
      this.properties_[name]!.state = MetadataCacheItemPropertyState.LOADING;
    }
  }

  /**
   * Feeds the result of startRequests.
   * @param requestId Request ID passed when calling startRequests.
   * @param typedObject Map of property name and value.
   * @return Whether at least one property is updated or not.
   */
  storeProperties(requestId: number, typedObject: MetadataItem): boolean {
    let changed = false;
    for (const name in typedObject) {
      if (/.Error$/.test(name) && typedObject[name as MetadataKey]) {
        typedObject[name.substr(0, name.length - 5) as MetadataKey] = undefined;
      }
    }
    for (const _name in typedObject) {
      if (/.Error$/.test(_name)) {
        continue;
      }
      const name = _name as MetadataKey;
      if (!this.properties_[name]) {
        this.properties_[name] = new MetadataCacheItemProperty();
      }
      if (requestId < this.properties_[name]!.requestId ||
          this.properties_[name]!.state ===
              MetadataCacheItemPropertyState.FULFILLED) {
        continue;
      }
      changed = true;
      this.properties_[name]!.requestId = requestId;
      this.properties_[name]!.value = typedObject[name as MetadataKey];
      const errorKey = `${name}Error` as const;
      this.properties_[name]!.error =
          typedObject[errorKey as (typeof errorKey) & MetadataKey];
      this.properties_[name]!.state = MetadataCacheItemPropertyState.FULFILLED;
    }
    return changed;
  }

  /**
   * Marks the caches of all properties in the item as invalidates and forces to
   * reload at the next time of startRequests. Optionally, takes an array of
   * names and only invalidates those.
   * @param requestId Request ID of the invalidation request. This must
   *     be larger than other requests ID passed to the item before.
   */
  invalidate(requestId: number, names?: MetadataKey[]): void {
    const namesToInvalidate = names ?
        names.filter(n => this.properties_[n]) :
        Object.keys(this.properties_) as MetadataKey[];
    for (const name of namesToInvalidate) {
      assert(this.properties_[name]!.requestId < requestId);
      this.properties_[name]!.requestId = requestId;
      this.properties_[name]!.state =
          MetadataCacheItemPropertyState.INVALIDATED;
    }
  }

  /**
   * Obtains property for entries and names.
   * Note that it returns invalidated properties also.
   */
  get(names: MetadataKey[]): MetadataItem {
    const result = new MetadataItem();
    for (const name of names) {
      assert(!/Error$/.test(name));
      if (this.properties_[name]) {
        // `undefined` is the intersection of all possible properties of
        // MetadataItem.
        result[name] = this.properties_[name]!.value as undefined;
        const errorKey = `${name}Error` as const;
        // TODO(TS): check ... if (!(errorKey in result)){
        result[errorKey as (typeof errorKey) & MetadataKey] =
            this.properties_[name]!.error;
      }
    }
    return result;
  }

  /**
   * Creates deep copy of the item.
   */
  clone(): MetadataCacheItem {
    const clonedItem = new MetadataCacheItem();
    for (const name in this.properties_) {
      const property = this.properties_[name as MetadataKey]!;
      const newItemProperties = new MetadataCacheItemProperty();
      newItemProperties.value = property.value;
      newItemProperties.error = property.error;
      newItemProperties.requestId = property.requestId;
      newItemProperties.state = property.state;
      clonedItem.properties_[name as MetadataKey] = newItemProperties;
    }
    return clonedItem;
  }

  /**
   * Returns whether all the given properties are fulfilled.
   * @param names Property names.
   */
  hasFreshCache(names: MetadataKey[]): boolean {
    for (const name of names) {
      const property = this.properties_[name];
      if (!(property &&
            property.state === MetadataCacheItemPropertyState.FULFILLED)) {
        return false;
      }
    }
    return true;
  }
}

enum MetadataCacheItemPropertyState {
  INVALIDATED = 'invalidated',
  LOADING = 'loading',
  FULFILLED = 'fulfilled',
}

/**
 * Cache of metadata for a property.
 */
class MetadataCacheItemProperty {
  /**
   * Cached value of property.
   */
  value: unknown = null;

  error: undefined|Error = undefined;

  /**
   * Last request ID.
   */
  requestId: number = -1;

  /**
   * Cache state of the property.
   */
  state = MetadataCacheItemPropertyState.INVALIDATED;
}
