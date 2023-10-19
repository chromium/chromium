// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// @ts-nocheck

/**
 * Persistent cache storing images in an indexed database on the hard disk.
 */
export class ImageCache {
  constructor() {
    /**
     * IndexedDB database handle.
     * @type {IDBDatabase}
     * @private
     */
    this.db_ = null;
  }

  /**
   * Initializes the cache database.
   * @param {function()} callback Completion callback.
   */
  initialize(callback) {
    // Establish a connection to the database or (re)create it if not available
    // or not up to date. After changing the database's schema, increment
    // DB_VERSION to force database recreating.
    const openRequest = window.indexedDB.open(DB_NAME, DB_VERSION);

    openRequest.onsuccess = (e) => {
      this.db_ = e.target.result;
      callback();
    };

    openRequest.onerror = callback;

    openRequest.onupgradeneeded = (e) => {
      console.info('Cache database creating or upgrading.');
      const db = e.target.result;
      if (db.objectStoreNames.contains('metadata')) {
        db.deleteObjectStore('metadata');
      }
      if (db.objectStoreNames.contains('data')) {
        db.deleteObjectStore('data');
      }
      if (db.objectStoreNames.contains('settings')) {
        db.deleteObjectStore('settings');
      }
      db.createObjectStore('metadata', {keyPath: 'key'});
      db.createObjectStore('data', {keyPath: 'key'});
      db.createObjectStore('settings', {keyPath: 'key'});
    };
  }

  /**
   * Sets size of the cache.
   *
   * @param {number} size Size in bytes.
   * @param {IDBTransaction=} opt_transaction Transaction to be reused. If not
   *     provided, then a new one is created.
   * @private
   */
  setCacheSize_(size, opt_transaction) {
    const transaction =
        opt_transaction || this.db_.transaction(['settings'], 'readwrite');
    const settingsStore = transaction.objectStore('settings');

    settingsStore.put({key: 'size', value: size});  // Update asynchronously.
  }

  /**
   * Fetches current size of the cache.
   *
   * @param {function(number)} onSuccess Callback to return the size.
   * @param {function()} onFailure Failure callback.
   * @param {IDBTransaction=} opt_transaction Transaction to be reused. If not
   *     provided, then a new one is created.
   * @private
   */
  fetchCacheSize_(onSuccess, onFailure, opt_transaction) {
    const transaction = opt_transaction ||
        this.db_.transaction(['settings', 'metadata', 'data'], 'readwrite');
    const settingsStore = transaction.objectStore('settings');
    const sizeRequest = settingsStore.get('size');

    sizeRequest.onsuccess = (e) => {
      if (e.target.result) {
        onSuccess(e.target.result.value);
      } else {
        onSuccess(0);
      }
    };

    sizeRequest.onerror = () => {
      console.warn('Failed to fetch size from the database.');
      onFailure();
    };
  }

  /**
   * Evicts the least used elements in cache to make space for a new image and
   * updates size of the cache taking into account the upcoming item.
   *
   * @param {number} size Requested size.
   * @param {function()} onSuccess Success callback.
   * @param {function()} onFailure Failure callback.
   * @param {IDBTransaction=} opt_transaction Transaction to be reused. If not
   *     provided, then a new one is created.
   * @private
   */
  evictCache_(size, onSuccess, onFailure, opt_transaction) {
    const transaction = opt_transaction ||
        this.db_.transaction(['settings', 'metadata', 'data'], 'readwrite');

    // Check if the requested size is smaller than the cache size.
    if (size > MEMORY_LIMIT) {
      onFailure();
      return;
    }

    const onCacheSize = (cacheSize) => {
      if (size < MEMORY_LIMIT - cacheSize) {
        // Enough space, no need to evict.
        this.setCacheSize_(cacheSize + size, transaction);
        onSuccess();
        return;
      }

      let bytesToEvict = Math.max(size, EVICTION_CHUNK_SIZE);

      // Fetch all metadata.
      const metadataEntries = [];
      const metadataStore = transaction.objectStore('metadata');
      const dataStore = transaction.objectStore('data');

      const onEntriesFetched = () => {
        metadataEntries.sort((a, b) => {
          return b.lastLoadTimestamp - a.lastLoadTimestamp;
        });

        let totalEvicted = 0;
        while (bytesToEvict > 0) {
          const entry = metadataEntries.pop();
          totalEvicted += entry.size;
          bytesToEvict -= entry.size;
          metadataStore.delete(entry.key);  // Remove asynchronously.
          dataStore.delete(entry.key);      // Remove asynchronously.
        }

        this.setCacheSize_(cacheSize - totalEvicted + size, transaction);
      };

      metadataStore.openCursor().onsuccess = (e) => {
        const cursor = e.target.result;
        if (cursor) {
          metadataEntries.push(cursor.value);
          cursor.continue();
        } else {
          onEntriesFetched();
        }
      };
    };

    this.fetchCacheSize_(onCacheSize, onFailure, transaction);
  }

  /**
   * Saves an image in the cache.
   *
   * @param {string} key Cache key.
   * @param {number} timestamp Last modification timestamp. Used to detect
   *     if the image cache entry is out of date.
   * @param {number} width Image width.
   * @param {number} height Image height.
   * @param {?string} ifd Image ifd, null if none.
   * @param {string} data Image data.
   */
  saveImage(key, timestamp, width, height, ifd, data) {
    if (!this.db_) {
      console.warn('Cache database not available.');
      return;
    }

    const onNotFoundInCache = () => {
      const metadataEntry = {
        key: key,
        timestamp: timestamp,
        width: width,
        height: height,
        ifd: ifd,
        size: data.length,
        lastLoadTimestamp: Date.now(),
      };

      const dataEntry = {key: key, data: data};

      const transaction =
          this.db_.transaction(['settings', 'metadata', 'data'], 'readwrite');
      const metadataStore = transaction.objectStore('metadata');
      const dataStore = transaction.objectStore('data');

      const onCacheEvicted = () => {
        metadataStore.put(metadataEntry);  // Add asynchronously.
        dataStore.put(dataEntry);          // Add asynchronously.
      };

      // Make sure there is enough space in the cache.
      this.evictCache_(data.length, onCacheEvicted, () => {}, transaction);
    };

    // Check if the image is already in cache. If not, then save it to cache.
    this.loadImage(key, timestamp, () => {}, onNotFoundInCache);
  }

  /**
   * Loads an image from the cache.
   *
   * @param {string} key Cache key.
   * @param {number} timestamp Last modification timestamp. If different
   *     than the one in cache, then the entry will be invalidated.
   * @param {function(number, number, ?string, string)} onSuccess Success
   *     callback with the image width, height, ?ifd, and data.
   * @param {function()} onFailure Failure callback.
   */
  loadImage(key, timestamp, onSuccess, onFailure) {
    if (!this.db_) {
      console.warn('Cache database not available.');
      onFailure();
      return;
    }

    const transaction =
        this.db_.transaction(['settings', 'metadata', 'data'], 'readwrite');
    const metadataStore = transaction.objectStore('metadata');
    const dataStore = transaction.objectStore('data');
    const metadataRequest = metadataStore.get(key);
    const dataRequest = dataStore.get(key);

    let metadataEntry = null;
    let metadataReceived = false;
    let dataEntry = null;
    let dataReceived = false;

    const onPartialSuccess = () => {
      // Check if all sub-requests have finished.
      if (!metadataReceived || !dataReceived) {
        return;
      }

      // Check if both entries are available or both unavailable.
      if (!!metadataEntry != !!dataEntry) {
        console.warn('Inconsistent cache database.');
        onFailure();
        return;
      }

      // Process the responses.
      if (!metadataEntry) {
        // The image not found.
        onFailure();
      } else if (metadataEntry.timestamp != timestamp) {
        // The image is not up to date, so remove it.
        this.removeImage(key, () => {}, () => {}, transaction);
        onFailure();
      } else {
        // The image is available. Update the last load time and return the
        // image data.
        metadataEntry.lastLoadTimestamp = Date.now();
        metadataStore.put(metadataEntry);  // Added asynchronously.
        onSuccess(
            metadataEntry.width, metadataEntry.height, metadataEntry.ifd,
            dataEntry.data);
      }
    };

    metadataRequest.onsuccess = (e) => {
      if (e.target.result) {
        metadataEntry = e.target.result;
      }
      metadataReceived = true;
      onPartialSuccess();
    };

    dataRequest.onsuccess = (e) => {
      if (e.target.result) {
        dataEntry = e.target.result;
      }
      dataReceived = true;
      onPartialSuccess();
    };

    metadataRequest.onerror = () => {
      console.warn('Failed to fetch metadata from the database.');
      metadataReceived = true;
      onPartialSuccess();
    };

    dataRequest.onerror = () => {
      console.warn('Failed to fetch image data from the database.');
      dataReceived = true;
      onPartialSuccess();
    };
  }

  /**
   * Removes the image from the cache.
   *
   * @param {string} key Cache key.
   * @param {function()=} opt_onSuccess Success callback.
   * @param {function()=} opt_onFailure Failure callback.
   * @param {IDBTransaction=} opt_transaction Transaction to be reused. If not
   *     provided, then a new one is created.
   */
  removeImage(key, opt_onSuccess, opt_onFailure, opt_transaction) {
    if (!this.db_) {
      console.warn('Cache database not available.');
      return;
    }

    const transaction = opt_transaction ||
        this.db_.transaction(['settings', 'metadata', 'data'], 'readwrite');
    const metadataStore = transaction.objectStore('metadata');
    const dataStore = transaction.objectStore('data');

    let cacheSize = null;
    let cacheSizeReceived = false;
    let metadataEntry = null;
    let metadataReceived = false;

    const onPartialSuccess = () => {
      if (!cacheSizeReceived || !metadataReceived) {
        return;
      }

      // If either cache size or metadata entry is not available, then it is
      // an error.
      if (cacheSize === null || !metadataEntry) {
        if (opt_onFailure) {
          opt_onFailure();
        }
        return;
      }

      if (opt_onSuccess) {
        opt_onSuccess();
      }

      this.setCacheSize_(cacheSize - metadataEntry.size, transaction);
      metadataStore.delete(key);  // Delete asynchronously.
      dataStore.delete(key);      // Delete asynchronously.
    };

    const onCacheSizeFailure = () => {
      cacheSizeReceived = true;
    };

    const onCacheSizeSuccess = (result) => {
      cacheSize = result;
      cacheSizeReceived = true;
      onPartialSuccess();
    };

    // Fetch the current cache size.
    this.fetchCacheSize_(onCacheSizeSuccess, onCacheSizeFailure, transaction);

    // Receive image's metadata.
    const metadataRequest = metadataStore.get(key);

    metadataRequest.onsuccess = (e) => {
      if (e.target.result) {
        metadataEntry = e.target.result;
      }
      metadataReceived = true;
      onPartialSuccess();
    };

    metadataRequest.onerror = () => {
      console.warn('Failed to remove an image.');
      metadataReceived = true;
      onPartialSuccess();
    };
  }
}

/**
 * Cache database name.
 * @type {string}
 * @const
 */
const DB_NAME = 'image-loader';

/**
 * Cache database version.
 * @type {number}
 * @const
 */
const DB_VERSION = 16;

/**
 * Memory limit for images data in bytes.
 *
 * @const
 * @type {number}
 */
const MEMORY_LIMIT = 250 * 1024 * 1024;  // 250 MB.

/**
 * Minimal amount of memory freed per eviction. Used to limit number of
 * evictions which are expensive.
 *
 * @const
 * @type {number}
 */
const EVICTION_CHUNK_SIZE = 50 * 1024 * 1024;  // 50 MB.
