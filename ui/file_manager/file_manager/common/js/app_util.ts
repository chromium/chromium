// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {storage} from './storage.js';

/**
 * Save app launch data to the local storage.
 */
export function saveAppState() {
  if (!window.appState) {
    return;
  }

  // Maps the appId to JSON serialized AppState.
  const items: Record<string, string> = {};

  items[window.appID] = JSON.stringify(window.appState);
  storage.local.setAsync(items);
}

/**
 * Updates the app state.
 *
 * @param currentDirectoryURL Currently opened directory as an URL.qq
 *     If null the value is left unchanged.
 * @param selectionURL Currently selected entry as an URL. If null the
 *     value is left unchanged.
 */
export function updateAppState(
    currentDirectoryURL: null|string, selectionURL: null|string) {
  window.appState = window.appState || {};
  if (currentDirectoryURL !== null) {
    window.appState.currentDirectoryURL = currentDirectoryURL;
  }
  if (selectionURL !== null) {
    window.appState.selectionURL = selectionURL;
  }
  saveAppState();
}
