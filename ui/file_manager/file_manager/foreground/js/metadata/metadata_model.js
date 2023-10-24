// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {entriesToURLs} from '../../../common/js/entry_utils.js';
import {VolumeManager} from '../../../externs/volume_manager.js';
import {getStore} from '../../../state/store.js';

import {ContentMetadataProvider} from './content_metadata_provider.js';
import {DlpMetadataProvider} from './dlp_metadata_provider.js';
import {ExternalMetadataProvider} from './external_metadata_provider.js';
import {FileSystemMetadataProvider} from './file_system_metadata_provider.js';
import {MetadataCacheSet} from './metadata_cache_set.js';
import {MetadataItem} from './metadata_item.js';
import {MetadataProvider} from './metadata_provider.js';
import {MultiMetadataProvider} from './multi_metadata_provider.js';

/**
 * Stats collected about Metadata handling for tests.
 * @final
 */
export class MetadataStats {
  constructor() {
    /** @public @type {number} Total of entries fulfilled from cache. */
    this.fromCache = 0;

    /** @public @type {number} Total of entries that requested to backends. */
    this.fullFetch = 0;

    /** @public @type {number} Total of entries that called to invalidate. */
    this.invalidateCount = 0;

    /** @public @type {number} Total of entries that called to clear. */
    this.clearCacheCount = 0;

    /** @public @type {number} Total of calls to function clearAllCache. */
    this.clearAllCount = 0;
  }
}

export class MetadataModel {
  /**
   * @param {!MetadataProvider} rawProvider
   */
  constructor(rawProvider) {
    /** @private @const @type {!MetadataProvider} */
    this.rawProvider_ = rawProvider;

    /** @private @const @type {!MetadataCacheSet} */
    this.cache_ = new MetadataCacheSet();

    /** @private @const @type {!Array<!MetadataProviderCallbackRequest>} */
    this.callbackRequests_ = [];

    /**
     * @private @const @type {?MetadataStats} record stats about Metadata when
     *     in tests.
     */
    this.stats_ = window.IN_TEST ? new MetadataStats() : null;
  }

  /**
   * @param {!VolumeManager} volumeManager
   * @return {!MetadataModel}
   */
  static create(volumeManager) {
    return new MetadataModel(new MultiMetadataProvider(
        new FileSystemMetadataProvider(), new ExternalMetadataProvider(),
        new ContentMetadataProvider(), new DlpMetadataProvider(),
        volumeManager));
  }

  /**
   * @return {!MetadataProvider}
   */
  getProvider() {
    return this.rawProvider_;
  }

  /**
   * Obtains metadata for entries.
   * @param {!Array<!Entry>} entries Entries.
   * @param {!Array<string>} names Metadata property names to be obtained.
   * @return {!Promise<!Array<!MetadataItem>>}
   */
  get(entries, names) {
    this.rawProvider_.checkPropertyNames(names);

    // Check if the results are cached or not.
    if (this.cache_.hasFreshCache(entries, names)) {
      if (window.IN_TEST) {
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        this.stats_.fromCache += entries.length;
      }
      return Promise.resolve(this.getCache(entries, names));
    }

    if (window.IN_TEST) {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
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
    const promise = new Promise(fulfill => {
      this.callbackRequests_.push(new MetadataProviderCallbackRequest(
          // @ts-ignore: error TS2345: Argument of type '(value: any) => void'
          // is not assignable to parameter of type '(arg0: MetadataItem[]) =>
          // undefined'.
          entries, names, snapshot, fulfill));
    });

    // If the requests are not empty, call the requests.
    if (requests.length) {
      this.rawProvider_.get(requests).then(list => {
        // Obtain requested entries and ensure all the requested properties are
        // contained in the result.
        const requestedEntries = [];
        for (let i = 0; i < requests.length; i++) {
          // @ts-ignore: error TS2532: Object is possibly 'undefined'.
          requestedEntries.push(requests[i].entry);
          // @ts-ignore: error TS2532: Object is possibly 'undefined'.
          for (let j = 0; j < requests[i].names.length; j++) {
            // @ts-ignore: error TS2532: Object is possibly 'undefined'.
            const name = requests[i].names[j];
            // @ts-ignore: error TS2532: Object is possibly 'undefined'.
            if (!(name in list[i])) {
              // @ts-ignore: error TS2538: Type 'undefined' cannot be used as an
              // index type.
              list[i][name] = undefined;
            }
          }
        }

        // Store cache.
        this.cache_.storeProperties(requestId, requestedEntries, list, names);

        // Invoke callbacks.
        let i = 0;
        while (i < this.callbackRequests_.length) {
          // @ts-ignore: error TS2532: Object is possibly 'undefined'.
          if (this.callbackRequests_[i].storeProperties(
                  requestId, requestedEntries, list)) {
            // Callback was called.
            this.callbackRequests_.splice(i, 1);
          } else {
            i++;
          }
        }
      });
    }

    return promise;
  }

  /**
   * Updates the metadata of the given fileUrls with the provided values for
   * each specified metadata name.
   * @param {!Array<!string>} fileUrls FileURLs to have their metadata updated
   * @param {!Array<string>} names Metadata property names to be updated.
   * @param {!Array<!Array<string|number|boolean>>} values
   */
  update(fileUrls, names, values) {
    const {allEntries} = getStore().getState();

    // Only update corresponding entries that are available in the store.
    const itemsToUpdate = [];
    const entriesToUpdate = [];
    for (let i = 0; i < fileUrls.length; i++) {
      const url = fileUrls[i];
      // @ts-ignore: error TS2538: Type 'undefined' cannot be used as an index
      // type.
      const entry = allEntries[url]?.entry;
      if (!entry) {
        continue;
      }
      entriesToUpdate.push(entry);
      const item = new MetadataItem();
      // @ts-ignore: error TS2532: Object is possibly 'undefined'.
      names.forEach((key, j) => item[key] = values[i][j]);
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
   * @param {!Array<!Entry>} entries Entries.
   * @param {!Array<string>} names Metadata property names to be obtained.
   * @return {!Array<!MetadataItem>}
   */
  getCache(entries, names) {
    // Check if the property name is correct or not.
    this.rawProvider_.checkPropertyNames(names);
    return this.cache_.get(entries, names);
  }

  /**
   * Obtains metadata cache for file URLs.
   * @param {!Array<!string>} urls File URLs.
   * @param {!Array<string>} names Metadata property names to be obtained.
   * @return {!Array<!MetadataItem>}
   */
  getCacheByUrls(urls, names) {
    // Check if the property name is correct or not.
    this.rawProvider_.checkPropertyNames(names);
    return this.cache_.getByUrls(urls, names);
  }

  /**
   * Clears old metadata for newly created entries.
   * @param {!Array<!Entry>} entries
   */
  notifyEntriesCreated(entries) {
    this.cache_.clear(entriesToURLs(entries));
    if (window.IN_TEST) {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.stats_.clearCacheCount += entries.length;
    }
  }

  /**
   * Clears metadata for deleted entries.
   * @param {!Array<string>} urls Note it is not an entry list because we cannot
   *     obtain entries after removing them from the file system.
   */
  notifyEntriesRemoved(urls) {
    this.cache_.clear(urls);
    if (window.IN_TEST) {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.stats_.clearCacheCount += urls.length;
    }
  }

  /**
   * Invalidates metadata for updated entries.
   * @param {!Array<!Entry>} entries
   */
  notifyEntriesChanged(entries) {
    this.cache_.invalidate(this.cache_.generateRequestId(), entries);
    if (window.IN_TEST) {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.stats_.invalidateCount += entries.length;
    }
  }

  /**
   * Clears all cache.
   */
  clearAllCache() {
    this.cache_.clearAll();
    if (window.IN_TEST) {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.stats_.clearAllCount++;
    }
  }

  /** @return {MetadataStats} */
  getStats() {
    // @ts-ignore: error TS2322: Type 'MetadataStats | null' is not assignable
    // to type 'MetadataStats'.
    return this.stats_;
  }

  /**
   * Adds event listener to internal cache object.
   * @param {string} type
   * @param {function(Event):void} callback
   */
  addEventListener(type, callback) {
    this.cache_.addEventListener(type, callback);
  }

  /**
   * Removes event listener from internal cache object.
   * @param {string} type Name of the event to removed.
   * @param {function(Event):void} callback Event listener.
   */
  removeEventListener(type, callback) {
    this.cache_.removeEventListener(type, callback);
  }
}

/** @final */
class MetadataProviderCallbackRequest {
  /**
   * @param {!Array<!Entry>} entries
   * @param {!Array<string>} names
   * @param {!MetadataCacheSet} cache
   * @param {function(!Array<MetadataItem>):undefined} fulfill
   */
  constructor(entries, names, cache, fulfill) {
    /**
     * @private @type {!Array<!Entry>}
     * @const
     */
    this.entries_ = entries;

    /**
     * @private @type {!Array<string>}
     * @const
     */
    this.names_ = names;

    /**
     * @private @type {!MetadataCacheSet}
     * @const
     */
    this.cache_ = cache;

    /**
     * @private @type {function(!Array<MetadataItem>):undefined}
     * @const
     */
    this.fulfill_ = fulfill;
  }

  /**
   * Stores properties to snapshot cache of the callback request.
   * If all the requested property are served, it invokes the callback.
   * @param {number} requestId
   * @param {!Array<!Entry>} entries
   * @param {!Array<!MetadataItem>} objects
   * @return {boolean} Whether the callback is invoked or not.
   */
  storeProperties(requestId, entries, objects) {
    this.cache_.storeProperties(requestId, entries, objects, this.names_);
    if (this.cache_.hasFreshCache(this.entries_, this.names_)) {
      this.fulfill_(this.cache_.get(this.entries_, this.names_));
      return true;
    }
    return false;
  }
}
