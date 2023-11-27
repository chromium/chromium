// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import {entriesToURLs} from '../../../common/js/entry_utils.js';
import {FilesAppEntry} from '../../../externs/files_app_entry_interfaces.js';

import {MetadataCacheItem} from './metadata_cache_item.js';
import {MetadataItem} from './metadata_item.js';
import {MetadataRequest} from './metadata_request.js';

/**
 * Custom event dispatched by the metadata cache set when results from metadata
 * provider are set on it.
 */
export class MetadataSetEvent extends Event {
  /**
   * @param {string} name
   * @param {!Array<!FileEntry>} entries
   * @param {!Map<string, !FileEntry>} entriesMap
   * @param {!Set<string>} names
   */
  constructor(name, entries, entriesMap, names) {
    super(name);
    this.entries = entries;
    this.entriesMap = entriesMap;
    this.names = names;
  }
}


/**
 * A collection of MetadataCacheItem objects. This class acts as a map from file
 * entry URLs to metadata items. You can store metadata for entries, you can
 * retrieve metadata for entries, clear the entire cache, or just selected
 * entries. In addition, you can generate MetadataRequests and start them (i.e.,
 * put them in the LOADING state).
 */
export class MetadataCacheSet extends EventTarget {
  constructor() {
    super();

    /**
     * @private <!Map<string, !MetadataCacheItem>>
     * @const
     */
    this.items_ = new Map();

    /**
     * @private @type {number}
     */
    this.requestIdCounter_ = 0;
  }

  /**
   * Creates list of MetadataRequest based on the cache state.
   * @param {!Array<!Entry|!FilesAppEntry>} entries
   * @param {!Array<string>} names
   * @return {!Array<!MetadataRequest>}
   */
  createRequests(entries, names) {
    const urls = entriesToURLs(entries);
    const requests = [];
    for (let i = 0; i < entries.length; i++) {
      const item = this.items_.get(urls[i]);
      const requestedNames = item ? item.createRequests(names) : names;
      if (requestedNames.length) {
        // @ts-ignore: error TS2345: Argument of type 'FileSystemEntry |
        // undefined' is not assignable to parameter of type 'FileSystemEntry'.
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
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      const url = requests[i].entry['cachedUrl'] || requests[i].entry.toURL();
      let item = this.items_.get(url);
      if (!item) {
        item = new MetadataCacheItem();
        this.items_.set(url, item);
      }
      // @ts-ignore: error TS18048: 'request' is possibly 'undefined'.
      item.startRequests(requestId, request.names);
    }
  }

  /**
   * Stores results from MetadataProvider with the request Id.
   * @param {number} requestId Request ID. If a newer operation has already been
   *     done, the results must be ignored.
   * @param {!Array<!Entry|!FilesAppEntry>} entries
   * @param {!Array<!MetadataItem>} results
   * @param {!Array<string>} names Property names that have been requested and
   *     updated.
   * @return {boolean} Whether at least one result is stored or not.
   */
  storeProperties(requestId, entries, results, names) {
    /** @type {!Array<!FileEntry>} */
    const changedEntries = [];
    const urls = entriesToURLs(entries);
    const entriesMap = new Map();

    for (let i = 0; i < entries.length; i++) {
      const url = urls[i];
      const item = this.items_.get(url);
      if (item && item.storeProperties(requestId, results[i])) {
        changedEntries.push(/** @type{!FileEntry} */ (entries[i]));
        entriesMap.set(url, entries[i]);
      }
    }

    if (!changedEntries.length) {
      return false;
    }

    const event = new MetadataSetEvent(
        'update', changedEntries, entriesMap, new Set(names));
    this.dispatchEvent(event);
    return true;
  }

  /**
   * Obtains cached properties for entries and names.
   * Note that it returns invalidated properties also.
   * @param {!Array<!Entry|!FilesAppEntry>} entries Entries.
   * @param {!Array<string>} names Property names.
   * @return {!Array<!MetadataItem>} metadata for the given entries.
   */
  get(entries, names) {
    const results = [];
    const urls = entriesToURLs(entries);
    for (let i = 0; i < entries.length; i++) {
      const item = this.items_.get(urls[i]);
      results.push(item ? item.get(names) : {});
    }
    return results;
  }

  /**
   * Obtains cached properties for file URLs and names.
   * Note that it returns invalidated properties also.
   * @param {!Array<!string>} urls File URLs.
   * @param {!Array<string>} names Property names.
   * @return {!Array<!MetadataItem>} metadata for the given entries.
   */
  getByUrls(urls, names) {
    const results = [];
    for (let i = 0; i < urls.length; i++) {
      const item = this.items_.get(urls[i]);
      results.push(item ? item.get(names) : {});
    }
    return results;
  }

  /**
   * Marks the caches of entries as invalidates and forces to reload at the next
   * time of startRequests. Optionally, takes an array of metadata names and
   * only invalidates those.
   * @param {number} requestId Request ID of the invalidation request. This must
   *     be larger than other request ID passed to the set before.
   * @param {!Array<!Entry|!FilesAppEntry>} entries
   * @param {!Array<string>} [names]
   */
  invalidate(requestId, entries, names) {
    const urls = entriesToURLs(entries);
    for (let i = 0; i < entries.length; i++) {
      const item = this.items_.get(urls[i]);
      if (item) {
        item.invalidate(requestId, names);
      }
    }
  }

  /**
   * Clears the caches of entries.
   * @param {!Array<string>} urls
   */
  clear(urls) {
    for (let i = 0; i < urls.length; i++) {
      this.items_.delete(urls[i]);
    }
  }

  /**
   * Clears all cache.
   */
  clearAll() {
    this.items_.clear();
  }

  /**
   * Creates snapshot of the cache for entries.
   * @param {!Array<!Entry|!FilesAppEntry>} entries
   * @return {!MetadataCacheSet} a cache with metadata for the given entries.
   */
  createSnapshot(entries) {
    const snapshot = new MetadataCacheSet();
    const items = snapshot.items_;
    const urls = entriesToURLs(entries);
    for (let i = 0; i < entries.length; i++) {
      const url = urls[i];
      const item = this.items_.get(url);
      if (item) {
        items.set(url, item.clone());
      }
    }
    return snapshot;
  }

  /**
   * Returns whether all the given properties are fulfilled.
   * @param {!Array<!Entry|!FilesAppEntry>} entries Entries.
   * @param {!Array<string>} names Property names.
   * @return {boolean}
   */
  hasFreshCache(entries, names) {
    if (!names.length) {
      return true;
    }
    const urls = entriesToURLs(entries);
    for (let i = 0; i < entries.length; i++) {
      const item = this.items_.get(urls[i]);
      if (!(item && item.hasFreshCache(names))) {
        return false;
      }
    }
    return true;
  }

  /**
   * Generates a unique request ID every time when it is called.
   * @return {number}
   */
  generateRequestId() {
    return this.requestIdCounter_++;
  }
}
