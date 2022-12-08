// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {VolumeManager} from '../../externs/volume_manager.js';

import {storage} from './storage.js';

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
  storage.local.setAsync(items);
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

export {appUtil};
