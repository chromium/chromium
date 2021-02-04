// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @suppress {checkTypes} */
/* #export */ class StorageAdapter {
  constructor() {
    this.onChanged = {
      addListener() {},
    };

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
        if (opt_callback) {
          setTimeout(opt_callback);
        }
      }
    };
  }
}
