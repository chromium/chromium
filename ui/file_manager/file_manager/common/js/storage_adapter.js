// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @suppress {checkTypes} */
/* #export */ class StorageAdapter {
  constructor() {
    /** @private {boolean} */
    this.storageHasBeenLoaded_ = false;

    this.onChanged = {addListener() {}};

    this.sync = {
      /** @private {!Object} store */
      store_: {},
      /**
       * @param {!Array<string>|string} keys
       * @param {function(!Object)} callback
       */
      get(keys, callback) {
        const inKeys = Array.isArray(keys) ? keys : [keys];
        const result = {};
        chrome.storage.loadStorageIfNeeded_();
        inKeys.forEach(key => {
          if (key in chrome.storage.sync.store_) {
            result[key] = chrome.storage.sync.store_[key];
          }
        });
        setTimeout(callback, 0, result);
      },
      /**
       * @param {!Object<string>} items
       * @param {function()=} opt_callback
       */
      set(items, opt_callback) {
        for (const key in items) {
          chrome.storage.sync.store_[key] = items[key];
        }
        chrome.storage.flushIntoLocalStorage_('sync');
        if (opt_callback) {
          setTimeout(opt_callback);
        }
      }
    };

    this.local = {
      /** @private {!Object} store */
      store_: {},
      /**
       * @param {!Array<string>|string} keys
       * @param {function(!Object)} callback
       */
      get(keys, callback) {
        const inKeys = Array.isArray(keys) ? keys : [keys];
        const result = {};
        chrome.storage.loadStorageIfNeeded_();
        inKeys.forEach(key => {
          if (key in chrome.storage.local.store_) {
            result[key] = chrome.storage.local.store_[key];
          }
        });
        setTimeout(callback, 0, result);
      },
      /**
       * @param {!Object<string>} items
       * @param {function()=} opt_callback
       */
      set(items, opt_callback) {
        for (const key in items) {
          chrome.storage.local.store_[key] = items[key];
        }
        chrome.storage.flushIntoLocalStorage_('local');
        if (opt_callback) {
          setTimeout(opt_callback);
        }
      }
    };
  }

  /**
   * If localStorage hasn't been loaded, read it and populate the
   * 'sync' and 'local' stores so they can be read.
   * @private
   */
  loadStorageIfNeeded_() {
    if (this.storageHasBeenLoaded_ === false) {
      const localData = window.localStorage.getItem('local');
      if (localData) {
        this.local.store_ = JSON.parse(localData);
      }
      const syncData = window.localStorage.getItem('sync');
      if (syncData) {
        this.sync.store_ = JSON.parse(syncData);
      }
      // Only do this once.
      this.storageHasBeenLoaded_ = true;
    }
  }

  /**
   * Write out the 'sync' and 'local' stores into localStorage.
   * @param {string} which Which store key we're writing to.
   * @private
   */
  flushIntoLocalStorage_(which) {
    let storeObjectString = null;
    if (which === 'local') {
      storeObjectString = JSON.stringify(this.local.store_);
    } else {
      storeObjectString = JSON.stringify(this.sync.store_);
    }
    window.localStorage.setItem(which, storeObjectString);
  }
}
