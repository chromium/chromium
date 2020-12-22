// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import './error_util.m.js';
// #import {util} from '../../file_manager/common/js/util.m.js';
// #import {assert} from 'chrome://resources/js/assert.m.js';
// #import {appUtil} from '../../base/js/app_util.m.js';
// #import {AllowedPaths} from '../../base/js/volume_manager_types.m.js';
// #import {FilteredVolumeManager} from '../../base/js/filtered_volume_manager.m.js';
// #import {NativeControlsVideoPlayer} from './video_player_native_controls.m.js';
// #import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// #import {metrics} from './video_player_metrics.m.js';
// clang-format on

const nativePlayer = new NativeControlsVideoPlayer();

/**
 * Unloads the player.
 */
/* #export */ function unload() {
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
  const volumeManager = new FilteredVolumeManager(
      AllowedPaths.ANY_PATH, false, appUtil.getVolumeManager());
  volumeManager.ensureInitialized(callback);
}

/**
 * Promise to initialize both the volume manager and the load time data.
 * @type {!Promise}
 */
const initPromise = Promise.all([
  new Promise(initStrings.wrap(null)),
  new Promise(initVolumeManager.wrap(null)),
  /* #ignore */ new Promise(resolve => window.HTMLImports.whenReady(resolve)),
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
