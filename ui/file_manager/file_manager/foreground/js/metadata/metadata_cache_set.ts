// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import {entriesToURLs} from '../../../common/js/entry_utils.js';
import type {FilesAppEntry} from '../../../common/js/files_app_entry_types.js';

import {MetadataCacheItem} from './metadata_cache_item.js';
import type {MetadataItem} from './metadata_item.js';
import {type MetadataKey} from './metadata_item.js';
import {MetadataRequest} from './metadata_request.js';

/**
 * Custom event dispatched by the metadata cache set when results from metadata
 * provider are set on it.
 */
export class MetadataSetEvent extends Event {
  constructor(
      name: string, public entries: Array<Entry|FilesAppEntry>,
      public entriesMap: Map<string, Entry|FilesAppEntry>,
      public names: Set<string>) {
    super(name);
  }
}

export interface MetadataModelMap extends Record<string, MetadataSetEvent> {
  'update': MetadataSetEvent;
}

interface MetadataSetEventTarget {
  addEventListener<K extends keyof MetadataModelMap>(
      type: K, listener: (event: MetadataModelMap[K]) => void,
      options?: boolean|AddEventListenerOptions|undefined): void;
  addEventListener(
      type: string, callback: EventListenerOrEventListenerObject|null,
      options?: AddEventListenerOptions|boolean): void;
  removeEventListener<K extends keyof MetadataModelMap>(
      type: K, listener: (event: MetadataModelMap[K]) => void,
      options?: boolean|EventListenerOptions): void;
  removeEventListener(
      type: string, listener: EventListenerOrEventListenerObject|null,
      options?: boolean|EventListenerOptions): void;
}

class MetadataSetEventTarget extends EventTarget {}

/**
 * A collection of MetadataCacheItem objects. This class acts as a map from file
 * entry URLs to metadata items. You can store metadata for entries, you can
 * retrieve metadata for entries, clear the entire cache, or just selected
 * entries. In addition, you can generate MetadataRequests and start them (i.e.,
 * put them in the LOADING state).
 */
export class MetadataCacheSet extends MetadataSetEventTarget {
  private items_ = new Map<string, MetadataCacheItem>();
  private requestIdCounter_: number = 0;

  /**
   * Creates list of MetadataRequest based on the cache state.
   */
  createRequests(entries: Array<Entry|FilesAppEntry>, names: MetadataKey[]):
      MetadataRequest[] {
    const urls = entriesToURLs(entries);
    const requests = [];
    for (const [i, entry] of entries.entries()) {
      const item = this.items_.get(urls[i]!);
      const requestedNames = item ? item.createRequests(names) : names;
      if (requestedNames.length) {
        requests.push(new MetadataRequest(entry, requestedNames));
      }
    }
    return requests;
  }

  /**
   * Updates cache states to start the given requests.
   */
  startRequests(requestId: number, requests: MetadataRequest[]) {
    for (const request of requests) {
      const url = request.entry.toURL();
      let item = this.items_.get(url);
      if (!item) {
        item = new MetadataCacheItem();
        this.items_.set(url, item);
      }
      item.startRequests(requestId, request.names);
    }
  }

  /**
   * Stores results from MetadataProvider with the request ID.
   * @param requestId Request ID. If a newer operation has already been done,
   *     the results must be ignored.
   * @param names Property names that have been requested and updated.
   * @return Whether at least one result is stored or not.
   */
  storeProperties(
      requestId: number, entries: Array<Entry|FilesAppEntry>,
      results: MetadataItem[], names: MetadataKey[]): boolean {
    const changedEntries: Array<Entry|FilesAppEntry> = [];
    const urls = entriesToURLs(entries);
    const entriesMap = new Map<string, Entry|FilesAppEntry>();

    for (const [i, entry] of entries.entries()) {
      const url = urls[i]!;
      const item = this.items_.get(url);
      if (item && item.storeProperties(requestId, results[i]!)) {
        changedEntries.push(entry);
        entriesMap.set(url, entry);
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
   * @param entries Entries.
   * @param names Property names.
   * @return metadata for the given entries.
   */
  get(entries: Array<Entry|FilesAppEntry>,
      names: MetadataKey[]): MetadataItem[] {
    const results = [];
    const urls = entriesToURLs(entries);
    for (let i = 0; i < entries.length; i++) {
      const item = this.items_.get(urls[i]!);
      results.push(item ? item.get(names) : {});
    }
    return results;
  }

  /**
   * Obtains cached properties for file URLs and names.
   * Note that it returns invalidated properties also.
   * @param urls File URLs.
   * @param names Property names.
   * @return metadata for the given entries.
   */
  getByUrls(urls: string[], names: MetadataKey[]): MetadataItem[] {
    const results = [];
    for (const url of urls) {
      const item = this.items_.get(url);
      results.push(item ? item.get(names) : {});
    }
    return results;
  }

  /**
   * Marks the caches of entries as invalidates and forces to reload at the next
   * time of startRequests. Optionally, takes an array of metadata names and
   * only invalidates those.
   * @param requestId Request ID of the invalidation request. This must
   *     be larger than other request ID passed to the set before.
   * @param [names]
   */
  invalidate(
      requestId: number, entries: Array<Entry|FilesAppEntry>,
      names?: MetadataKey[]) {
    const urls = entriesToURLs(entries);
    for (let i = 0; i < entries.length; i++) {
      const item = this.items_.get(urls[i]!);
      if (item) {
        item.invalidate(requestId, names);
      }
    }
  }

  /**
   * Clears the caches of entries.
   */
  clear(urls: string[]) {
    for (const url of urls) {
      this.items_.delete(url);
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
   * @return a cache with metadata for the given entries.
   */
  createSnapshot(entries: Array<Entry|FilesAppEntry>): MetadataCacheSet {
    const snapshot = new MetadataCacheSet();
    const items = snapshot.items_;
    const urls = entriesToURLs(entries);
    for (let i = 0; i < entries.length; i++) {
      const url = urls[i]!;
      const item = this.items_.get(url);
      if (item) {
        items.set(url, item.clone());
      }
    }
    return snapshot;
  }

  /**
   * Returns whether all the given properties are fulfilled.
   * @param entries Entries.
   * @param names Property names.
   */
  hasFreshCache(entries: Array<Entry|FilesAppEntry>, names: MetadataKey[]):
      boolean {
    if (!names.length) {
      return true;
    }
    const urls = entriesToURLs(entries);
    for (let i = 0; i < entries.length; i++) {
      const item = this.items_.get(urls[i]!);
      if (!(item && item.hasFreshCache(names))) {
        return false;
      }
    }
    return true;
  }

  /**
   * Generates a unique request ID every time when it is called.
   */
  generateRequestId(): number {
    return this.requestIdCounter_++;
  }
}
