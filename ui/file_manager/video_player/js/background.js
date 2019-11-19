// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Use maximum size and let ash downsample the icon.
 *
 * @type {!string}
 * @const
 */
var ICON_IMAGE = 'images/icon/video-player-192.png';

/**
 * Configuration of the video player panel.
 * @type {!Object}
 * @const
 */
var windowCreateOptions = {
  frame: {
    color: '#fafafa'
  },
  minWidth: 480,
  minHeight: 270
};

/**
 * Backgound object.
 * @type {!BackgroundBase}
 */
var background = new BackgroundBase();

/**
 * Creates a unique windowId string. Each call increments the sequence number
 * used to create the string. The first call returns "VIDEO_PLAYER_APP_0".
 * @return {string} windowId The windowId string.
 */
var generateWindowId = (function() {
  var seq = 0;
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
function openVideoPlayerWindow(urls) {
  var position = 0;
  var startUrl = (position < urls.length) ? urls[position] : '';
  var windowId = null;

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
        var maybePosition = util.entriesToURLs(entries).indexOf(startUrl);
        if (maybePosition !== -1) {
          position = maybePosition;
        }

        windowId = generateWindowId();

        // Opens the video player window.
        return new Promise(function(fulfill, reject) {
          var urls = util.entriesToURLs(entries);
          var videoPlayer = new AppWindowWrapper(
              'video_player.html', assert(windowId), windowCreateOptions);

          videoPlayer.launch(
              {items: urls, position: position}, false,
              fulfill.bind(null, videoPlayer));
        }.wrap());
      }.wrap())
      .then(function(videoPlayer) {
        var appWindow = videoPlayer.rawAppWindow;

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

background.setLaunchHandler(openVideoPlayerWindow);
