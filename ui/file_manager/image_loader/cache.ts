// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

interface MetadataEntry {
  key: string;

  /** Last modification timestamp. */
  timestamp: number;
  width: number;
  height: number;
  ifd?: string;
  size: number;               // data.length.
  lastLoadTimestamp: number;  // from Date.now().
  data?: string;
}

/**
 * Persistent cache storing images in an indexed database on the hard disk.
 */
export class ImageCache {
  /**
   * IndexedDB database handle.
   */
  private db_: null|IDBDatabase = null;

  /**
   * Initializes the cache database.
   * @param callback Completion callback.
   */
  initialize(callback: VoidCallback) {
    // Establish a connection to the database or (re)create it if not available
    // or not up to date. After changing the database's schema, increment
    // DB_VERSION to force database recreating.
    const openRequest = indexedDB.open(DB_NAME, DB_VERSION);

    openRequest.onsuccess = () => {
      this.db_ = openRequest.result;
      callback();
    };

    openRequest.onerror = callback;

    openRequest.onupgradeneeded = () => {
      console.info('Cache database creating or upgrading.');
      const db = openRequest.result;
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
   * @param size Size in bytes.
   * @param transaction Transaction to be reused. If not provided, then a new
   *     one is created.
   */
  private setCacheSize_(size: number, transaction?: IDBTransaction) {
    transaction =
        transaction || this.db_!.transaction(['settings'], 'readwrite');
    const settingsStore = transaction.objectStore('settings');

    settingsStore.put({key: 'size', value: size});  // Update asynchronously.
  }

  /**
   * Fetches current size of the cache.
   *
   * @param onSuccess Callback to return the size.
   * @param onFailure Failure callback.
   * @param transaction Transaction to be reused. If not
   *     provided, then a new one is created.
   */
  private fetchCacheSize_(
      onSuccess: (a: number) => void, onFailure: VoidCallback,
      transaction?: IDBTransaction) {
    transaction = transaction ||
        this.db_!.transaction(['settings', 'metadata', 'data'], 'readwrite');
    const settingsStore = transaction.objectStore('settings');
    const sizeRequest = settingsStore.get('size');

    sizeRequest.onsuccess = () => {
      const result = sizeRequest.result;
      if (result) {
        onSuccess(result.value);
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
   * @param size Requested size.
   * @param onSuccess Success callback.
   * @param onFailure Failure callback.
   * @param dbTransaction Transaction to be reused. If not provided, then a new
   *     one is created.
   */
  private evictCache_(
      size: number, onSuccess: VoidCallback, onFailure: VoidCallback,
      dbTransaction?: IDBTransaction) {
    const transaction = dbTransaction ||
        this.db_!.transaction(['settings', 'metadata', 'data'], 'readwrite');

    // Check if the requested size is smaller than the cache size.
    if (size > MEMORY_LIMIT) {
      onFailure();
      return;
    }

    const onCacheSize = (cacheSize: number) => {
      if (size < MEMORY_LIMIT - cacheSize) {
        // Enough space, no need to evict.
        this.setCacheSize_(cacheSize + size, transaction);
        onSuccess();
        return;
      }

      let bytesToEvict = Math.max(size, EVICTION_CHUNK_SIZE);

      // Fetch all metadata.
      const metadataEntries: MetadataEntry[] = [];
      const metadataStore = transaction.objectStore('metadata');
      const dataStore = transaction.objectStore('data');

      const onEntriesFetched = () => {
        metadataEntries.sort((a, b) => {
          return b.lastLoadTimestamp - a.lastLoadTimestamp;
        });

        let totalEvicted = 0;
        while (bytesToEvict > 0) {
          const entry = metadataEntries.pop()!;
          totalEvicted += entry.size;
          bytesToEvict -= entry.size;
          metadataStore.delete(entry.key);  // Remove asynchronously.
          dataStore.delete(entry.key);      // Remove asynchronously.
        }

        this.setCacheSize_(cacheSize - totalEvicted + size, transaction);
      };

      const cursor = metadataStore.openCursor();
      cursor.onsuccess = () => {
        const result = cursor.result;
        if (result) {
          metadataEntries.push(result.value);
          result.continue();
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
   * @param key Cache key.
   * @param timestamp Last modification timestamp. Used to detect if the image
   *     cache entry is out of date.
   * @param width Image width.
   * @param height Image height.
   * @param ifd Image ifd, null if none.
   * @param data Image data.
   */
  saveImage(
      key: string, timestamp: number, width: number, height: number,
      ifd: string|undefined, data: string) {
    if (!this.db_) {
      console.warn('Cache database not available.');
      return;
    }

    const onNotFoundInCache = () => {
      const metadataEntry: MetadataEntry = {
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
          this.db_!.transaction(['settings', 'metadata', 'data'], 'readwrite');
      const metadataStore = transaction.objectStore('metadata');
      const dataStore = transaction.objectStore('data');

      const onCacheEvicted = () => {
        metadataStore.put(metadataEntry);  // Add asynchronously.
        dataStore.put(dataEntry);          // Add asynchronously.
      };

      // Make sure there is enough space in the cache.
      this.evictCache_(data.length, onCacheEvicted, () => {}, transaction);
    };

    // Check if the image is already in cache. If not, then save it to
    // cache.
    this.loadImage(key, timestamp, () => {}, onNotFoundInCache);
  }

  /**
   * Loads an image from the cache.
   *
   * @param key Cache key.
   * @param timestamp Last modification timestamp. If different than the one in
   *     cache, then the entry will be invalidated.
   * @param onSuccess Success callback.
   * @param onFailure Failure callback.
   */
  loadImage(
      key: string, timestamp: number,
      onSuccess:
          (width: number, height: number, ifd?:|string, data?: string) => void,
      onFailure: VoidCallback) {
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

    let metadataEntry: MetadataEntry|null = null;
    let metadataReceived = false;
    let dataEntry: MetadataEntry|null = null;
    let dataReceived = false;

    const onPartialSuccess = () => {
      // Check if all sub-requests have finished.
      if (!metadataReceived || !dataReceived) {
        return;
      }

      // Check if both entries are available or both unavailable.
      if (!!metadataEntry !== !!dataEntry) {
        console.warn('Inconsistent cache database.');
        onFailure();
        return;
      }

      // Process the responses.
      if (!metadataEntry) {
        // The image not found.
        onFailure();
      } else if (metadataEntry.timestamp !== timestamp) {
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
            dataEntry!.data);
      }
    };

    metadataRequest.onsuccess = () => {
      if (metadataRequest.result) {
        metadataEntry = metadataRequest.result;
      }
      metadataReceived = true;
      onPartialSuccess();
    };

    dataRequest.onsuccess = () => {
      if (dataRequest.result) {
        dataEntry = dataRequest.result;
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
   * @param key Cache key.
   * @param onSuccess Success callback.
   * @param onFailure Failure callback.
   * @param transaction Transaction to be reused. If not provided, then a new
   *     one is created.
   */
  removeImage(
      key: string, onSuccess?: VoidCallback, onFailure?: VoidCallback,
      transaction?: IDBTransaction) {
    if (!this.db_) {
      console.warn('Cache database not available.');
      return;
    }

    transaction = transaction ||
        this.db_.transaction(['settings', 'metadata', 'data'], 'readwrite');
    const metadataStore = transaction.objectStore('metadata');
    const dataStore = transaction.objectStore('data');

    let cacheSize: number|null = null;
    let cacheSizeReceived = false;
    let metadataEntry: MetadataEntry|null = null;
    let metadataReceived = false;

    const onPartialSuccess = () => {
      if (!cacheSizeReceived || !metadataReceived) {
        return;
      }

      // If either cache size or metadata entry is not available, then it is an
      // error.
      if (cacheSize === null || !metadataEntry) {
        if (onFailure) {
          onFailure();
        }
        return;
      }

      if (onSuccess) {
        onSuccess();
      }

      this.setCacheSize_(cacheSize - metadataEntry.size, transaction);
      metadataStore.delete(key);  // Delete asynchronously.
      dataStore.delete(key);      // Delete asynchronously.
    };

    const onCacheSizeFailure = () => {
      cacheSizeReceived = true;
    };

    const onCacheSizeSuccess = (result: number) => {
      cacheSize = result;
      cacheSizeReceived = true;
      onPartialSuccess();
    };

    // Fetch the current cache size.
    this.fetchCacheSize_(onCacheSizeSuccess, onCacheSizeFailure, transaction);

    // Receive image's metadata.
    const metadataRequest = metadataStore.get(key);

    metadataRequest.onsuccess = () => {
      if (metadataRequest.result) {
        metadataEntry = metadataRequest.result;
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
 */
const DB_NAME = 'image-loader';

/**
 * Cache database version.
 */
const DB_VERSION = 16;

/**
 * Memory limit for images data in bytes.
 */
const MEMORY_LIMIT = 250 * 1024 * 1024;  // 250 MB.

/**
 * Minimal amount of memory freed per eviction. Used to limit number
 * of evictions which are expensive.
 */
const EVICTION_CHUNK_SIZE = 50 * 1024 * 1024;  // 50 MB.
