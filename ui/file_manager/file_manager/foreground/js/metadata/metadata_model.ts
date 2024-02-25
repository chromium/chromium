// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {VolumeManager} from '../../../background/js/volume_manager.js';
import {entriesToURLs} from '../../../common/js/entry_utils.js';
import type {FilesAppEntry} from '../../../common/js/files_app_entry_types.js';
import {MetadataStats} from '../../../common/js/shared_types.js';
import {getStore} from '../../../state/store.js';

import {ContentMetadataProvider} from './content_metadata_provider.js';
import {DlpMetadataProvider} from './dlp_metadata_provider.js';
import {ExternalMetadataProvider} from './external_metadata_provider.js';
import {FileSystemMetadataProvider} from './file_system_metadata_provider.js';
import {MetadataCacheSet, type MetadataModelMap} from './metadata_cache_set.js';
import {MetadataItem, type MetadataKey} from './metadata_item.js';
import type {MetadataProvider} from './metadata_provider.js';
import {MultiMetadataProvider} from './multi_metadata_provider.js';

export {MetadataStats} from '../../../common/js/shared_types.js';

export class MetadataModel {
  private cache_ = new MetadataCacheSet();
  private callbackRequests_: MetadataProviderCallbackRequest[] = [];

  /** Record stats about Metadata when in tests. */
  private stats_: MetadataStats|null =
      window.IN_TEST ? new MetadataStats() : null;

  constructor(private rawProvider_: MetadataProvider) {}

  static create(volumeManager: VolumeManager): MetadataModel {
    return new MetadataModel(new MultiMetadataProvider(
        new FileSystemMetadataProvider(), new ExternalMetadataProvider(),
        new ContentMetadataProvider(), new DlpMetadataProvider(),
        volumeManager));
  }

  getProvider(): MetadataProvider {
    return this.rawProvider_;
  }

  /**
   * Obtains metadata for entries.
   * @param entries Entries.
   * @param names Metadata property names to be obtained.
   */
  get(entries: Array<Entry|FilesAppEntry>,
      names: readonly MetadataKey[]): Promise<MetadataItem[]> {
    this.rawProvider_.checkPropertyNames(names);

    // Check if the results are cached or not.
    if (this.cache_.hasFreshCache(entries, names)) {
      if (this.stats_) {
        this.stats_.fromCache += entries.length;
      }
      return Promise.resolve(this.getCache(entries, names));
    }

    if (this.stats_) {
      this.stats_.fullFetch += entries.length;
    }

    // The LRU cache may be cached out when the callback is completed.
    // To hold cached values, create snapshot of the cache for entries.
    const requestId = this.cache_.generateRequestId();
    const snapshot = this.cache_.createSnapshot(entries);
    const requests = snapshot.createRequests(entries, names);
    snapshot.startRequests(requestId, requests);
    this.cache_.startRequests(requestId, requests);

    // Register callback.
    const promise = new Promise<MetadataItem[]>(fulfill => {
      this.callbackRequests_.push(new MetadataProviderCallbackRequest(
          entries, names, snapshot, fulfill));
    });

    // If the requests are not empty, call the requests.
    if (requests.length) {
      this.rawProvider_.get(requests).then(list => {
        // Obtain requested entries and ensure all the requested properties are
        // contained in the result.
        const requestedEntries: Array<Entry|FilesAppEntry> = [];
        for (const [i, request] of requests.entries()) {
          requestedEntries.push(request.entry);
          for (const name of request.names) {
            if (!(name in list[i]!)) {
              list[i]![name] = undefined;
            }
          }
        }

        // Store cache.
        this.cache_.storeProperties(requestId, requestedEntries, list, names);

        // Invoke callbacks and remove the ones that were successful.
        this.callbackRequests_.filter(
            request =>
                !request.storeProperties(requestId, requestedEntries, list));
      });
    }

    return promise;
  }

  /**
   * Updates the metadata of the given fileUrls with the provided values for
   * each specified metadata name.
   * @param fileUrls FileURLs to have their metadata updated
   * @param names Metadata property names to be updated.
   * @param values Contains an array for each file, where the array contains a
   *     new value for each metadata property passed in `names`.
   */
  update(
      fileUrls: string[], names: MetadataKey[],
      values: Array<Array<string|number|boolean>>) {
    const {allEntries} = getStore().getState();

    // Only update corresponding entries that are available in the store.
    const itemsToUpdate = [];
    const entriesToUpdate = [];
    for (const [i, url] of fileUrls.entries()) {
      const entry = allEntries[url]?.entry;
      if (!entry) {
        continue;
      }
      entriesToUpdate.push(entry);
      const item = new MetadataItem();
      // TODO(austinct): Change the function call signature and update all
      // callers to allow this statement to be well typed without the type
      // assertions.
      names.forEach((key, j) => (item[key] as any) = values[i]![j]);
      itemsToUpdate.push(item);
    }

    if (entriesToUpdate.length === 0) {
      return;
    }

    this.cache_.invalidate(
        this.cache_.generateRequestId(), entriesToUpdate, names);
    this.cache_.storeProperties(
        this.cache_.generateRequestId(), entriesToUpdate, itemsToUpdate, names);
  }

  /**
   * Obtains metadata cache for entries.
   * @param entries Entries.
   * @param names Metadata property names to be obtained.
   */
  getCache(entries: Array<Entry|FilesAppEntry>, names: MetadataKey[]):
      MetadataItem[] {
    // Check if the property name is correct or not.
    this.rawProvider_.checkPropertyNames(names);
    return this.cache_.get(entries, names);
  }

  /**
   * Obtains metadata cache for file URLs.
   * @param urls File URLs.
   * @param names Metadata property names to be obtained.
   */
  getCacheByUrls(urls: string[], names: MetadataKey[]): MetadataItem[] {
    // Check if the property name is correct or not.
    this.rawProvider_.checkPropertyNames(names);
    return this.cache_.getByUrls(urls, names);
  }

  /**
   * Clears old metadata for newly created entries.
   */
  notifyEntriesCreated(entries: Array<Entry|FilesAppEntry>) {
    this.cache_.clear(entriesToURLs(entries));
    if (this.stats_) {
      this.stats_.clearCacheCount += entries.length;
    }
  }

  /**
   * Clears metadata for deleted entries.
   * @param urls Note it is not an entry list because we cannot
   *     obtain entries after removing them from the file system.
   */
  notifyEntriesRemoved(urls: string[]) {
    this.cache_.clear(urls);
    if (this.stats_) {
      this.stats_.clearCacheCount += urls.length;
    }
  }

  /**
   * Invalidates metadata for updated entries.
   */
  notifyEntriesChanged(entries: Array<Entry|FilesAppEntry>) {
    this.cache_.invalidate(this.cache_.generateRequestId(), entries);
    if (this.stats_) {
      this.stats_.invalidateCount += entries.length;
    }
  }

  /**
   * Clears all cache.
   */
  clearAllCache() {
    this.cache_.clearAll();
    if (this.stats_) {
      this.stats_.clearAllCount++;
    }
  }

  getStats(): MetadataStats|null {
    return this.stats_;
  }

  /** Adds event listener to internal cache object. */
  addEventListener<K extends keyof MetadataModelMap>(
      type: K, listener: (event: MetadataModelMap[K]) => void): void {
    this.cache_.addEventListener(type, listener);
  }

  /** Removes event listener from internal cache object. */
  removeEventListener<K extends keyof MetadataModelMap>(
      type: K, listener: (event: MetadataModelMap[K]) => void): void {
    this.cache_.removeEventListener(type, listener);
  }
}

class MetadataProviderCallbackRequest {
  constructor(
      private entries_: Array<Entry|FilesAppEntry>,
      private names_: MetadataKey[], private cache_: MetadataCacheSet,
      private fulfill_: (items: MetadataItem[]) => void) {}

  /**
   * Stores properties to snapshot cache of the callback request.
   * If all the requested property are served, it invokes the callback.
   * @return Whether the callback is invoked or not.
   */
  storeProperties(
      requestId: number, entries: Array<Entry|FilesAppEntry>,
      objects: MetadataItem[]): boolean {
    this.cache_.storeProperties(requestId, entries, objects, this.names_);
    if (this.cache_.hasFreshCache(this.entries_, this.names_)) {
      this.fulfill_(this.cache_.get(this.entries_, this.names_));
      return true;
    }
    return false;
  }
}
