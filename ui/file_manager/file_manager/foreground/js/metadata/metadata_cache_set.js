// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';

import {util} from '../../../common/js/util.js';

import {MetadataCacheItem} from './metadata_cache_item.js';
import {MetadataItem} from './metadata_item.js';
import {MetadataRequest} from './metadata_request.js';

/**
 * Set of MetadataCacheItem.
 */
export class MetadataCacheSet extends EventTarget {
  /**
   * @param {!MetadataCacheSetStorage} items Storage object containing
   *     MetadataCacheItem.
   */
  constructor(items) {
    super();

    /**
     * @private {!MetadataCacheSetStorage}
     * @const
     */
    this.items_ = items;
  }

  /**
   * Creates list of MetadataRequest based on the cache state.
   * @param {!Array<!Entry>} entries
   * @param {!Array<string>} names
   * @return {!Array<!MetadataRequest>}
   */
  createRequests(entries, names) {
    const urls = util.entriesToURLs(entries);
    const requests = [];
    for (let i = 0; i < entries.length; i++) {
      const item = this.items_.peek(urls[i]);
      const requestedNames = item ? item.createRequests(names) : names;
      if (requestedNames.length) {
        requests.push(new MetadataRequest(entries[i], requestedNames));
      }
    }
    return requests;
  }

  /**
   * Updates cache states to start the given requests.
   * @param {number} requestId
   * @param {!Array<!MetadataRequest>} requests
   */
  startRequests(requestId, requests) {
    for (let i = 0; i < requests.length; i++) {
      const request = requests[i];
      const url = requests[i].entry['cachedUrl'] || requests[i].entry.toURL();
      let item = this.items_.peek(url);
      if (!item) {
        item = new MetadataCacheItem();
        this.items_.put(url, item);
      }
      item.startRequests(requestId, request.names);
    }
  }

  /**
   * Stores results from MetadataProvider with the request Id.
   * @param {number} requestId Request ID. If a newer operation has already been
   *     done, the results must be ignored.
   * @param {!Array<!Entry>} entries
   * @param {!Array<!MetadataItem>} results
   * @param {!Array<string>} names Property names that have been requested and
   *     updated.
   * @return {boolean} Whether at least one result is stored or not.
   */
  storeProperties(requestId, entries, results, names) {
    const changedEntries = [];
    const urls = util.entriesToURLs(entries);
    const entriesMap = new Map();

    for (let i = 0; i < entries.length; i++) {
      const url = urls[i];
      const item = this.items_.peek(url);
      if (item && item.storeProperties(requestId, results[i])) {
        changedEntries.push(entries[i]);
        entriesMap.set(url, entries[i]);
      }
    }

    if (!changedEntries.length) {
      return false;
    }

    const event = new Event('update');
    event.entries = changedEntries;
    event.entriesMap = entriesMap;
    event.names = new Set(names);
    this.dispatchEvent(event);
    return true;
  }

  /**
   * Obtains cached properties for entries and names.
   * Note that it returns invalidated properties also.
   * @param {!Array<!Entry>} entries Entries.
   * @param {!Array<string>} names Property names.
   */
  get(entries, names) {
    const results = [];
    const urls = util.entriesToURLs(entries);
    for (let i = 0; i < entries.length; i++) {
      const item = this.items_.get(urls[i]);
      results.push(item ? item.get(names) : {});
    }
    return results;
  }

  /**
   * Marks the caches of entries as invalidates and forces to reload at the next
   * time of startRequests.
   * @param {number} requestId Request ID of the invalidation request. This must
   *     be larger than other requets ID passed to the set before.
   * @param {!Array<!Entry>} entries
   */
  invalidate(requestId, entries) {
    const urls = util.entriesToURLs(entries);
    for (let i = 0; i < entries.length; i++) {
      const item = this.items_.peek(urls[i]);
      if (item) {
        item.invalidate(requestId);
      }
    }
  }

  /**
   * Clears the caches of entries.
   * @param {!Array<string>} urls
   */
  clear(urls) {
    for (let i = 0; i < urls.length; i++) {
      this.items_.remove(urls[i]);
    }
  }

  /**
   * Clears all cache.
   */
  clearAll() {
    this.items_.removeAll();
  }

  /**
   * Creates snapshot of the cache for entries.
   * @param {!Array<!Entry>} entries
   */
  createSnapshot(entries) {
    const items = {};
    const urls = util.entriesToURLs(entries);
    for (let i = 0; i < entries.length; i++) {
      const url = urls[i];
      const item = this.items_.peek(url);
      if (item) {
        items[url] = item.clone();
      }
    }
    return new MetadataCacheSet(new MetadataCacheSetStorageForObject(items));
  }

  /**
   * Returns whether all the given properties are fulfilled.
   * @param {!Array<!Entry>} entries Entries.
   * @param {!Array<string>} names Property names.
   * @return {boolean}
   */
  hasFreshCache(entries, names) {
    if (!names.length) {
      return true;
    }
    const urls = util.entriesToURLs(entries);
    for (let i = 0; i < entries.length; i++) {
      const item = this.items_.peek(urls[i]);
      if (!(item && item.hasFreshCache(names))) {
        return false;
      }
    }
    return true;
  }
}

/**
 * Interface of raw strage for MetadataCacheItem.
 * @interface
 */
export class MetadataCacheSetStorage {
  /**
   * Returns an item corresponding to the given URL.
   * @param {string} url Entry URL.
   * @return {MetadataCacheItem}
   */
  get(url) {}

  /**
   * Returns an item corresponding to the given URL without changing orders in
   * the cache list.
   * @param {string} url Entry URL.
   * @return {MetadataCacheItem}
   */
  peek(url) {}

  /**
   * Saves an item corresponding to the given URL.
   * @param {string} url Entry URL.
   * @param {!MetadataCacheItem} item Item to be saved.
   */
  put(url, item) {}

  /**
   * Removes an item from the cache.
   * @param {string} url Entry URL.
   */
  remove(url) {}

  /**
   * Remove all items from the cache.
   */
  removeAll() {}
}

/**
 * Implementation of MetadataCacheSetStorage by using raw object.
 * @implements {MetadataCacheSetStorage}
 */
export class MetadataCacheSetStorageForObject {
  /** @param {Object} items Map of URL and MetadataCacheItem. */
  constructor(items) {
    this.items_ = items;
  }

  /**
   * @override
   */
  get(url) {
    return this.items_[url];
  }

  /**
   * @override
   */
  peek(url) {
    return this.items_[url];
  }

  /**
   * @override
   */
  put(url, item) {
    this.items_[url] = item;
  }

  /**
   * @override
   */
  remove(url) {
    delete this.items_[url];
  }

  /**
   * @override
   */
  removeAll() {
    for (const url in this.items_) {
      delete this.items_[url];
    }
  }
}
