// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {assert} from 'chrome://resources/js/assert.m.js';
// #import {AppWindowWrapper} from '../../file_manager/background/js/app_window_wrapper.m.js';
// #import {util} from '../../file_manager/common/js/util.m.js';
// #import {BackgroundBaseImpl} from '../../file_manager/background/js/background_base.m.js';
// #import {BackgroundBase} from '../../externs/background/background_base.m.js';
// clang-format on

/**
 * Use maximum size and let ash downsample the icon.
 *
 * @type {!string}
 * @const
 */
const ICON_IMAGE = 'images/icon/video-player-192.png';

/**
 * HTML source of the video player.
 * @type {!string}
 * @const
 */
const VIDEO_PLAYER_APP_URL = 'video_player.html';

/**
 * HTML source of the video player as JS module.
 * @type {!string}
 * @const
 */
const VIDEO_PLAYER_MODULE_APP_URL = 'video_player_module.html';

/**
 * Configuration of the video player panel.
 * @type {!Object}
 * @const
 */
const windowCreateOptions = {
  frame: {
    color: '#fafafa',
  },
  minWidth: 480,
  minHeight: 270,
};

/**
 * Backgound object.
 * @type {!BackgroundBase}
 */
window.background = new BackgroundBaseImpl();

/**
 * Creates a unique windowId string. Each call increments the sequence number
 * used to create the string. The first call returns "VIDEO_PLAYER_APP_0".
 * @return {string} windowId The windowId string.
 */
const generateWindowId = (function() {
  let seq = 0;
  return function() {
    return 'VIDEO_PLAYER_APP_' + seq++;
  }.wrap();
}.wrap())();

/**
 * Opens the video player window.
 * @param {!Array<string>} urls List of videos to play and index to start
 *     playing.
 * @return {!Promise} Promise to be fulfilled on success, or rejected on error.
 */
/* #export */ function openVideoPlayerWindow(urls) {
  let position = 0;
  const startUrl = (position < urls.length) ? urls[position] : '';
  let windowId = null;

  return new Promise(function(fulfill, reject) {
           util.URLsToEntries(urls)
               .then(function(result) {
                 fulfill(result.entries);
               }.wrap())
               .catch(reject);
         }.wrap())
      .then(function(entries) {
        if (entries.length === 0) {
          return Promise.reject('No file to open.');
        }

        // Adjusts the position to start playing.
        const maybePosition = util.entriesToURLs(entries).indexOf(startUrl);
        if (maybePosition !== -1) {
          position = maybePosition;
        }

        windowId = generateWindowId();

        // Opens the video player window.
        const urls = util.entriesToURLs(entries);
        const videoPlayerUrl = util.isVideoPlayerJsModulesEnabled() ?
            VIDEO_PLAYER_MODULE_APP_URL :
            VIDEO_PLAYER_APP_URL;
        const videoPlayer = new AppWindowWrapper(
            videoPlayerUrl, assert(windowId), windowCreateOptions);

        return videoPlayer.launch({items: urls, position: position}, false)
            .then(() => videoPlayer);
      }.wrap())
      .then(function(videoPlayer) {
        const appWindow = videoPlayer.rawAppWindow;

        appWindow.onClosed.addListener(function() {
          chrome.power.releaseKeepAwake();
        });

        if (chrome.test) {
          appWindow.contentWindow.loadMockCastExtensionForTest = true;
        }

        videoPlayer.setIcon(ICON_IMAGE);
        appWindow.focus();

        return windowId;
      }.wrap())
      .catch(function(error) {
        console.error('Launch failed: ' + (error.stack || error));
        return Promise.reject(error);
      }.wrap());
}

window.background.setLaunchHandler(openVideoPlayerWindow);
