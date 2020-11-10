// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Map of all currently open file dialogs. The key is an app ID.
 * @type {!Object<!chrome.app.window.AppWindow>}
 */
window.appWindows = window.appWindows || {};

/**
 * Gets similar windows, it means with the same initial url.
 * @param {string} url URL that the obtained windows have.
 * @return {Array<chrome.app.window.AppWindow>} List of similar windows.
 */
window.getSimilarWindows = url => {
  const result = [];
  for (const appID in window.appWindows) {
    if (window.appWindows[appID].contentWindow.appInitialURL === url) {
      result.push(window.appWindows[appID]);
    }
  }
  return result;
};
