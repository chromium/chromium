// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BackgroundBase} from '../../externs/background/background_base.js';
import {VolumeManager} from '../../externs/volume_manager.js';
import {xfm} from './xfm.js';

const appUtil = {};

/**
 * Save app launch data to the local storage.
 */
appUtil.saveAppState = () => {
  if (!window.appState) {
    return;
  }
  const items = {};

  items[window.appID] = JSON.stringify(window.appState);
  xfm.storage.local.setAsync(items);
};

/**
 * Updates the app state.
 *
 * @param {?string} currentDirectoryURL Currently opened directory as an URL.qq
 *     If null the value is left unchanged.
 * @param {?string} selectionURL Currently selected entry as an URL. If null the
 *     value is left unchanged.
 */
appUtil.updateAppState = (currentDirectoryURL, selectionURL) => {
  window.appState = window.appState || {};
  if (currentDirectoryURL !== null) {
    window.appState.currentDirectoryURL = currentDirectoryURL;
  }
  if (selectionURL !== null) {
    window.appState.selectionURL = selectionURL;
  }
  appUtil.saveAppState();
};


/**
 *  AppCache is a persistent timestamped key-value storage backed by
 *  HTML5 local storage.
 *
 *  It is not designed for frequent access. In order to avoid costly
 *  localStorage iteration all data is kept in a single localStorage item.
 *  There is no in-memory caching, so concurrent access is _almost_ safe.
 *
 *  TODO(kaznacheev) Reimplement this based on Indexed DB.
 */
appUtil.AppCache = () => {};

/**
 * Local storage key.
 */
appUtil.AppCache.KEY = 'AppCache';

/**
 * Max number of items.
 */
appUtil.AppCache.CAPACITY = 100;

/**
 * Default lifetime.
 */
appUtil.AppCache.LIFETIME = 30 * 24 * 60 * 60 * 1000;  // 30 days.

/**
 * @param {string} key Key.
 * @param {function(number)} callback Callback accepting a value.
 */
appUtil.AppCache.getValue = (key, callback) => {
  appUtil.AppCache.read_(map => {
    const entry = map[key];
    callback(entry && entry.value);
  });
};

/**
 * Updates the cache.
 *
 * @param {string} key Key.
 * @param {?(string|number)} value Value. Remove the key if value is null.
 * @param {number=} opt_lifetime Maximum time to keep an item (in milliseconds).
 */
appUtil.AppCache.update = (key, value, opt_lifetime) => {
  appUtil.AppCache.read_(map => {
    if (value != null) {
      map[key] = {
        value: value,
        expire: Date.now() + (opt_lifetime || appUtil.AppCache.LIFETIME)
      };
    } else if (key in map) {
      delete map[key];
    } else {
      return;  // Nothing to do.
    }
    appUtil.AppCache.cleanup_(map);
    appUtil.AppCache.write_(map);
  });
};

/**
 * @param {function(Object)} callback Callback accepting a map of timestamped
 *   key-value pairs.
 * @private
 */
appUtil.AppCache.read_ = async (callback) => {
  const values = await xfm.storage.local.getAsync(appUtil.AppCache.KEY);

  const json = /** @type {string} */ (values[appUtil.AppCache.KEY]);
  if (json) {
    try {
      callback(/** @type {Object} */ (JSON.parse(json)));
    } catch (e) {
      // The local storage item somehow got messed up, start fresh.
    }
  }
  callback({});
};

/**
 * @param {Object} map A map of timestamped key-value pairs.
 * @private
 */
appUtil.AppCache.write_ = map => {
  const items = {};
  items[appUtil.AppCache.KEY] = JSON.stringify(map);
  xfm.storage.local.setAsync(items);
};

/**
 * Remove over-capacity and obsolete items.
 *
 * @param {Object} map A map of timestamped key-value pairs.
 * @private
 */
appUtil.AppCache.cleanup_ = map => {
  // Sort keys by ascending timestamps.
  const keys = [];
  for (const key in map) {
    if (map.hasOwnProperty(key)) {
      keys.push(key);
    }
  }
  keys.sort((a, b) => {
    return map[a].expire - map[b].expire;
  });

  const cutoff = Date.now();

  let obsolete = 0;
  while (obsolete < keys.length && map[keys[obsolete]].expire < cutoff) {
    obsolete++;
  }

  const overCapacity = Math.max(0, keys.length - appUtil.AppCache.CAPACITY);

  const itemsToDelete = Math.max(obsolete, overCapacity);
  for (let i = 0; i != itemsToDelete; i++) {
    delete map[keys[i]];
  }
};

/**
 * Fetches the VolumeManager from the background page.
 * @return {!Promise<!VolumeManager>}.
 */
appUtil.getVolumeManager = async () => {
  const backgroundWindow = new Promise(resolve => {
    if (window.isSWA) {
      resolve(window);
    } else {
      chrome.runtime.getBackgroundPage(resolve);
    }
  });

  /** @type {!BackgroundBase} */
  const backgroundPage = (await backgroundWindow).background;
  return backgroundPage.getVolumeManager();
};

export {appUtil};
