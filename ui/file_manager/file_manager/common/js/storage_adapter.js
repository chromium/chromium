// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// namespace
/* #export */ const storage = {};

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
 * @extends {StorageArea}
 */
class StorageAreaSWAImpl {
  /**
   * @param {string} type
   */
  constructor(type) {
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

  load_() {
    if (!this.loaded_) {
      this.store_ = getFromLocalStorage(this.type_);
      this.loaded_ = true;
    }
  }
}

/**
 * @type {!StorageArea}
 */
storage.sync;

/**
 * @type {!StorageArea}
 */
storage.local;

if (window.isSWA) {
  storage.sync = new StorageAreaSWAImpl('sync');
  storage.local = new StorageAreaSWAImpl('local');
} else if (chrome && chrome.storage) {
  storage.sync = chrome.storage.sync;
  storage.local = chrome.storage.local;
} else {
  console.warn('Creating sync and local stubs for tests');
  storage.sync = new StorageAreaSWAImpl('test-sync');
  storage.local = new StorageAreaSWAImpl('test-local');
}
