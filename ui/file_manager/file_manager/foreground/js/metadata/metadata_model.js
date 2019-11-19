// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Stats collected about Metadata handling for tests.
 * @final
 */
class MetadataStats {
  constructor() {
    /** @public {number} Total of entries fulfilled from cache. */
    this.fromCache = 0;

    /** @public {number} Total of entries that requested to backends. */
    this.fullFetch = 0;

    /** @public {number} Total of entries that called to invalidate. */
    this.invalidateCount = 0;

    /** @public {number} Total of entries that called to clear. */
    this.clearCacheCount = 0;

    /** @public {number} Total of calls to function clearAllCache. */
    this.clearAllCount = 0;
  }
}

class MetadataModel {
  /**
   * @param {!MetadataProvider} rawProvider
   */
  constructor(rawProvider) {
    /** @private @const {!MetadataProvider} */
    this.rawProvider_ = rawProvider;

    /** @private @const {!MetadataProviderCache} */
    this.cache_ = new MetadataProviderCache();

    /** @private @const {!Array<!MetadataProviderCallbackRequest<T>>} */
    this.callbackRequests_ = [];

    /**
     * @private @const {?MetadataStats} record stats about Metadata when in
     *     tests.
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
        new ContentMetadataProvider(), volumeManager));
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
        this.stats_.fromCache += entries.length;
      }
      return Promise.resolve(this.getCache(entries, names));
    }

    if (window.IN_TEST) {
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
          entries, names, snapshot, fulfill));
    });

    // If the requests are not empty, call the requests.
    if (requests.length) {
      this.rawProvider_.get(requests).then(list => {
        // Obtain requested entries and ensure all the requested properties are
        // contained in the result.
        const requestedEntries = [];
        for (let i = 0; i < requests.length; i++) {
          requestedEntries.push(requests[i].entry);
          for (let j = 0; j < requests[i].names.length; j++) {
            const name = requests[i].names[j];
            if (!(name in list[i])) {
              list[i][name] = undefined;
            }
          }
        }

        // Store cache.
        this.cache_.storeProperties(requestId, requestedEntries, list, names);

        // Invoke callbacks.
        let i = 0;
        while (i < this.callbackRequests_.length) {
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
   * Clears old metadata for newly created entries.
   * @param {!Array<!Entry>} entries
   */
  notifyEntriesCreated(entries) {
    this.cache_.clear(util.entriesToURLs(entries));
    if (window.IN_TEST) {
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
      this.stats_.invalidateCount += entries.length;
    }
  }

  /**
   * Clears all cache.
   */
  clearAllCache() {
    this.cache_.clearAll();
    if (window.IN_TEST) {
      this.stats_.clearAllCount++;
    }
  }

  /** @return {MetadataStats} */
  getStats() {
    return this.stats_;
  }

  /**
   * Adds event listener to internal cache object.
   * @param {string} type
   * @param {function(Event):undefined} callback
   */
  addEventListener(type, callback) {
    this.cache_.addEventListener(type, callback);
  }

  /**
   * Removes event listener from internal cache object.
   * @param {string} type Name of the event to removed.
   * @param {function(Event):undefined} callback Event listener.
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
   * @param {function(!MetadataItem):undefined} fulfill
   */
  constructor(entries, names, cache, fulfill) {
    /**
     * @private {!Array<!Entry>}
     * @const
     */
    this.entries_ = entries;

    /**
     * @private {!Array<string>}
     * @const
     */
    this.names_ = names;

    /**
     * @private {!MetadataCacheSet}
     * @const
     */
    this.cache_ = cache;

    /**
     * @private {function(!MetadataItem):undefined}
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

/**
 * Helper wrapper for LRUCache.
 * @final
 */
class MetadataProviderCache extends MetadataCacheSet {
  constructor() {
    super(new MetadataCacheSetStorageForObject({}));

    /**
     * @private {number}
     */
    this.requestIdCounter_ = 0;
  }

  /**
   * Generates a unique request ID every time when it is called.
   * @return {number}
   */
  generateRequestId() {
    return this.requestIdCounter_++;
  }
}
