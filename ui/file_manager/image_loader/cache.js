// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Persistent cache storing images in an indexed database on the hard disk.
 * @constructor
 */
export function ImageCache() {
  /**
   * IndexedDB database handle.
   * @type {IDBDatabase}
   * @private
   */
  this.db_ = null;
}

/**
 * Cache database name.
 * @type {string}
 * @const
 */
ImageCache.DB_NAME = 'image-loader';

/**
 * Cache database version.
 * @type {number}
 * @const
 */
ImageCache.DB_VERSION = 15;

/**
 * Memory limit for images data in bytes.
 *
 * @const
 * @type {number}
 */
ImageCache.MEMORY_LIMIT = 250 * 1024 * 1024;  // 250 MB.

/**
 * Minimal amount of memory freed per eviction. Used to limit number of
 * evictions which are expensive.
 *
 * @const
 * @type {number}
 */
ImageCache.EVICTION_CHUNK_SIZE = 50 * 1024 * 1024;  // 50 MB.

/**
 * Initializes the cache database.
 * @param {function()} callback Completion callback.
 */
ImageCache.prototype.initialize = function(callback) {
  // Establish a connection to the database or (re)create it if not available
  // or not up to date. After changing the database's schema, increment
  // ImageCache.DB_VERSION to force database recreating.
  const openRequest =
      window.indexedDB.open(ImageCache.DB_NAME, ImageCache.DB_VERSION);

  openRequest.onsuccess = function(e) {
    this.db_ = e.target.result;
    callback();
  }.bind(this);

  openRequest.onerror = callback;

  openRequest.onupgradeneeded = function(e) {
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
};

/**
 * Sets size of the cache.
 *
 * @param {number} size Size in bytes.
 * @param {IDBTransaction=} opt_transaction Transaction to be reused. If not
 *     provided, then a new one is created.
 * @private
 */
ImageCache.prototype.setCacheSize_ = function(size, opt_transaction) {
  const transaction =
      opt_transaction || this.db_.transaction(['settings'], 'readwrite');
  const settingsStore = transaction.objectStore('settings');

  settingsStore.put({key: 'size', value: size});  // Update asynchronously.
};

/**
 * Fetches current size of the cache.
 *
 * @param {function(number)} onSuccess Callback to return the size.
 * @param {function()} onFailure Failure callback.
 * @param {IDBTransaction=} opt_transaction Transaction to be reused. If not
 *     provided, then a new one is created.
 * @private
 */
ImageCache.prototype.fetchCacheSize_ = function(
    onSuccess, onFailure, opt_transaction) {
  const transaction = opt_transaction ||
      this.db_.transaction(['settings', 'metadata', 'data'], 'readwrite');
  const settingsStore = transaction.objectStore('settings');
  const sizeRequest = settingsStore.get('size');

  sizeRequest.onsuccess = function(e) {
    if (e.target.result) {
      onSuccess(e.target.result.value);
    } else {
      onSuccess(0);
    }
  };

  sizeRequest.onerror = function() {
    console.warn('Failed to fetch size from the database.');
    onFailure();
  };
};

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
ImageCache.prototype.evictCache_ = function(
    size, onSuccess, onFailure, opt_transaction) {
  const transaction = opt_transaction ||
      this.db_.transaction(['settings', 'metadata', 'data'], 'readwrite');

  // Check if the requested size is smaller than the cache size.
  if (size > ImageCache.MEMORY_LIMIT) {
    onFailure();
    return;
  }

  const onCacheSize = function(cacheSize) {
    if (size < ImageCache.MEMORY_LIMIT - cacheSize) {
      // Enough space, no need to evict.
      this.setCacheSize_(cacheSize + size, transaction);
      onSuccess();
      return;
    }

    let bytesToEvict = Math.max(size, ImageCache.EVICTION_CHUNK_SIZE);

    // Fetch all metadata.
    const metadataEntries = [];
    const metadataStore = transaction.objectStore('metadata');
    const dataStore = transaction.objectStore('data');

    const onEntriesFetched = function() {
      metadataEntries.sort(function(a, b) {
        return b.lastLoadTimestamp - a.lastLoadTimestamp;
      });

      let totalEvicted = 0;
      while (bytesToEvict > 0) {
        const entry = metadataEntries.pop();
        totalEvicted += entry.size;
        bytesToEvict -= entry.size;
        metadataStore.delete(entry.key);  // Remove asynchronously.
        dataStore.delete(entry.key);  // Remove asynchronously.
      }

      this.setCacheSize_(cacheSize - totalEvicted + size, transaction);
    }.bind(this);

    metadataStore.openCursor().onsuccess = function(e) {
      const cursor = e.target.result;
      if (cursor) {
        metadataEntries.push(cursor.value);
        cursor.continue();
      } else {
        onEntriesFetched();
      }
    };
  }.bind(this);

  this.fetchCacheSize_(onCacheSize, onFailure, transaction);
};

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
ImageCache.prototype.saveImage = function(
    key, timestamp, width, height, ifd, data) {
  if (!this.db_) {
    console.warn('Cache database not available.');
    return;
  }

  const onNotFoundInCache = function() {
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

    const onCacheEvicted = function() {
      metadataStore.put(metadataEntry);  // Add asynchronously.
      dataStore.put(dataEntry);  // Add asynchronously.
    };

    // Make sure there is enough space in the cache.
    this.evictCache_(data.length, onCacheEvicted, function() {}, transaction);
  }.bind(this);

  // Check if the image is already in cache. If not, then save it to cache.
  this.loadImage(key, timestamp, function() {}, onNotFoundInCache);
};

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
ImageCache.prototype.loadImage = function(
    key, timestamp, onSuccess, onFailure) {
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

  const onPartialSuccess = function() {
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
      this.removeImage(key, function() {}, function() {}, transaction);
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
  }.bind(this);

  metadataRequest.onsuccess = function(e) {
    if (e.target.result) {
      metadataEntry = e.target.result;
    }
    metadataReceived = true;
    onPartialSuccess();
  };

  dataRequest.onsuccess = function(e) {
    if (e.target.result) {
      dataEntry = e.target.result;
    }
    dataReceived = true;
    onPartialSuccess();
  };

  metadataRequest.onerror = function() {
    console.warn('Failed to fetch metadata from the database.');
    metadataReceived = true;
    onPartialSuccess();
  };

  dataRequest.onerror = function() {
    console.warn('Failed to fetch image data from the database.');
    dataReceived = true;
    onPartialSuccess();
  };
};

/**
 * Removes the image from the cache.
 *
 * @param {string} key Cache key.
 * @param {function()=} opt_onSuccess Success callback.
 * @param {function()=} opt_onFailure Failure callback.
 * @param {IDBTransaction=} opt_transaction Transaction to be reused. If not
 *     provided, then a new one is created.
 */
ImageCache.prototype.removeImage = function(
    key, opt_onSuccess, opt_onFailure, opt_transaction) {
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

  const onPartialSuccess = function() {
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
    dataStore.delete(key);  // Delete asynchronously.
  }.bind(this);

  const onCacheSizeFailure = function() {
    cacheSizeReceived = true;
  };

  const onCacheSizeSuccess = function(result) {
    cacheSize = result;
    cacheSizeReceived = true;
    onPartialSuccess();
  };

  // Fetch the current cache size.
  this.fetchCacheSize_(onCacheSizeSuccess, onCacheSizeFailure, transaction);

  // Receive image's metadata.
  const metadataRequest = metadataStore.get(key);

  metadataRequest.onsuccess = function(e) {
    if (e.target.result) {
      metadataEntry = e.target.result;
    }
    metadataReceived = true;
    onPartialSuccess();
  };

  metadataRequest.onerror = function() {
    console.warn('Failed to remove an image.');
    metadataReceived = true;
    onPartialSuccess();
  };
};
