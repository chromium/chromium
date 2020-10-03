// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const nativePlayer = new NativeControlsVideoPlayer();

/**
 * Unloads the player.
 */
function unload() {
  // Releases keep awake just in case (should be released on unloading video).
  chrome.power.releaseKeepAwake();

  nativePlayer.savePosition(true);
}

/**
 * Initializes the load time data.
 * @param {function()} callback Called when the load time data is ready.
 */
function initStrings(callback) {
  chrome.fileManagerPrivate.getStrings(function(strings) {
    loadTimeData.data = strings;
    callback();
  }.wrap(null));
}

/**
 * Initializes the volume manager.
 * @param {function()} callback Called when the volume manager is ready.
 */
function initVolumeManager(callback) {
  const volumeManager = new FilteredVolumeManager(AllowedPaths.ANY_PATH, false);
  volumeManager.ensureInitialized(callback);
}

/**
 * Promise to initialize both the volume manager and the load time data.
 * @type {!Promise}
 */
const initPromise = Promise.all([
  new Promise(initStrings.wrap(null)),
  new Promise(initVolumeManager.wrap(null)),
  new Promise(resolve => window.HTMLImports.whenReady(resolve)),
]);

/**
 * Initialize the video player.
 */
initPromise
    .then(function() {
      if (document.readyState !== 'loading') {
        return;
      }
      return new Promise(function(fulfill, reject) {
        document.addEventListener('DOMContentLoaded', fulfill);
      }.wrap());
    }.wrap())
    .then(function() {
      const isReady = document.readyState !== 'loading';
      assert(isReady, 'VideoPlayer DOM document is still loading');
      return new Promise(function(fulfill, reject) {
        util.URLsToEntries(window.appState.items, function(entries) {
          metrics.recordOpenVideoPlayerAction();
          metrics.recordNumberOfOpenedFiles(entries.length);

          nativePlayer.prepare(entries);
          nativePlayer.playFirstVideo();
        }.wrap());
      }.wrap());
    }.wrap());
