// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// namespace
export const storage = {};

/**
 * StorageAreaAsync is a wrapper for existing storage implementations to
 * include async/await compatible version of get/set.
 * @extends {StorageArea}
 */
class StorageAreaAsync {
  /**
   * @param {?StorageArea} storageArea chrome.storage.{local,sync} or null
   */
  constructor(storageArea) {
    this.storageArea_ = (storageArea) ? storageArea : this;

    this.get = this.storageArea_.get.bind(this.storageArea_);
    this.set = this.storageArea_.set.bind(this.storageArea_);
    this.remove = this.storageArea_.remove.bind(this.storageArea_);
    this.clear = this.storageArea_.clear.bind(this.storageArea_);
  }

  /**
   * Convert the storage.{local,sync}.get method to return a Promise.
   * @param {string|!Array<string>} keys
   * @returns {!Promise<!Object<string, *>>}
   */
  async getAsync(keys) {
    return new Promise((resolve, reject) => {
      this.get(keys, (values) => {
        if (chrome && chrome.runtime && chrome.runtime.lastError) {
          const keysString = keys && keys.join(', ');
          reject(`Failed to retrieve keys [${keysString}] from browser storage:
              ${chrome.runtime.lastError.message}`);
          return;
        }
        resolve(values);
      });
    });
  }

  /**
   * Convert the storage.{local,sync}.set method to return a Promise.
   * @param {!Object<string, *>} values
   * @returns {!Promise<void>}
   */
  async setAsync(values) {
    return new Promise((resolve, reject) => {
      this.set(values, () => {
        if (chrome && chrome.runtime && chrome.runtime.lastError) {
          const keysString = values && Object.keys(values).join(', ');
          reject(`Failed to update browser storage keys
              [${keysString}] with supplied values:
              ${chrome.runtime.lastError.message}`);
          return;
        }
        resolve();
      });
    });
  }
}

/**
 * If localStorage hasn't been loaded, read it and populate the storage
 * for the specified type ('sync' or 'local').
 * @param {string} type
 */
function getFromLocalStorage(type) {
  const localData = window.localStorage.getItem(type);
  return localData ? JSON.parse(localData) : {};
}

/**
 * Write out the 'sync' and 'local' stores into localStorage.
 * @param {string} type
 * @param {Object} data
 */
function flushIntoLocalStorage(type, data) {
  window.localStorage.setItem(type, JSON.stringify(data));
}

/**
 * @type {{
 *   addListener: function(Object),
 * }}
 */
storage.onChanged = (window.isSWA) ? {addListener(callback) {}} : {
  addListener(callback) {
    chrome.storage.onChanged.addListener(callback);
  }
};

/**
 * StorageAreaSWAImpl enables the SWA version of Files app to continue using the
 * xfm.storage.* APIs by transparently switching them to use window.localStorage
 * instead of the chrome.storage APIs.
 */
class StorageAreaSWAImpl extends StorageAreaAsync {
  /**
   * @param {string} type
   */
  constructor(type) {
    super(/** storageArea */ null);

    /** @private {boolean} */
    this.loaded_ = false;
    /** @private {!Object} */
    this.store_ = {};
    /** @private {string} */
    this.type_ = type;
  }

  /**
   * @override
   */
  get(keys, callback) {
    this.load_();
    const inKeys = Array.isArray(keys) ? keys : [keys];
    const result = {};
    for (const key of inKeys) {
      result[key] = this.store_[key];
    }
    callback(result);
  }

  /**
   * @override
   */
  set(items, opt_callback) {
    this.load_();
    for (const key in items) {
      this.store_[key] = items[key];
    }
    flushIntoLocalStorage(this.type_, this.store_);
    if (opt_callback) {
      opt_callback();
    }
  }

  /**
   * @override
   */
  remove(keys, callback) {
    this.load_();
    const keyList = Array.isArray(keys) ? keys : [keys];
    for (const key of keyList) {
      delete this.store_[key];
    }
    flushIntoLocalStorage(this.type_, this.store_);
  }

  /**
   * @override
   */
  clear(callback) {
    this.load_();
    this.store_ = {};
    flushIntoLocalStorage(this.type_, this.store_);
  }

  load_() {
    if (!this.loaded_) {
      this.store_ = getFromLocalStorage(this.type_);
      this.loaded_ = true;
    }
  }
}

/**
 * @type {!StorageAreaAsync}
 */
storage.sync;

/**
 * @type {!StorageAreaAsync}
 */
storage.local;

if (window.isSWA) {
  storage.sync = new StorageAreaSWAImpl('sync');
  storage.local = new StorageAreaSWAImpl('local');
} else if (chrome && chrome.storage) {
  storage.sync = new StorageAreaAsync(chrome.storage.sync);
  storage.local = new StorageAreaAsync(chrome.storage.local);
} else {
  console.warn('Creating sync and local stubs for tests');
  storage.sync = new StorageAreaSWAImpl('test-sync');
  storage.local = new StorageAreaSWAImpl('test-local');
}
