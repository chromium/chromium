/**
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * @fileoverview Common APIs for media router performance tests.
 *
 */

var initialized = false;
var currentSession = null;
var currentMedia = null;


window['__onGCastApiAvailable'] = function(loaded, errorInfo) {
  if (loaded) {
    initializeCastApi();
  } else {
    console.log(errorInfo);
  }
}

/**
 * Initialize Cast APIs.
 */
function initializeCastApi() {
  // Load Cast APIs
  console.info('Initializing API');
  var sessionRequest = new chrome.cast.SessionRequest(
    chrome.cast.media.DEFAULT_MEDIA_RECEIVER_APP_ID);
  var apiConfig = new chrome.cast.ApiConfig(
    sessionRequest,
    null,  // session listener
    function(availability) {  // receiver listener
      console.info('Receiver listener: ' + JSON.stringify(availability));
      initialized = true;
  });
  chrome.cast.initialize(
    apiConfig,
    function() {  // Successful callback
      console.info('Initialize successfully');
    },
    function(error) {  // Error callback
      console.error('Initialize failed, error: ' + JSON.stringify(error));
  });
}

/**
 * Start a new session for flinging scenario.
 */
function startFlingingSession() {
  console.info('Starting Session');
  chrome.cast.requestSession(
    function(session) {  // Request session successful callback
      console.info('Request session successfully');
      currentSession = session;
    },
    function(error) {  // Request session Error callback
      console.error('Request session failed, error: ' + JSON.stringify(error));
  });
}

/**
 * Loads the specific video on Chromecast.
 *
 * @param {string} mediaUrl the url which points to a mp4 video.
 */
function loadMedia(mediaUrl) {
  if (!currentSession) {
    console.warn('Cannot load media without a live session');
  }
  console.info('loading ' + mediaUrl);
  var mediaInfo = new chrome.cast.media.MediaInfo(mediaUrl, 'video/mp4');
  var request = new chrome.cast.media.LoadRequest(mediaInfo);
  request.autoplay = true;
  request.currentTime = 0;
  currentSession.loadMedia(request,
    function(media) {
      console.info('Load media successfully');
      currentMedia = media;
    },
    function(error) {  // Error callback
      console.error('Load media failed, error: ' + JSON.stringify(error));
  });
}

/**
 * Stops current session.
 */
function stopSession() {
  if (currentSession) {
    currentSession.stop(
      function() {
        console.info('Stop session successfully');
        currentSession = null;
      },
      function(error) {  // Error callback
        console.error('Stop session failed, error: ' + JSON.stringify(error));
    });
  }
}
