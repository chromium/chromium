// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {openVideoPlayerWindow} from './background.m.js';
// #import {test} from '../../file_manager/background/js/test_util_base.m.js';

/**
 * Returns if a video element playing the specified file meet the condition
 * which is given by a parameter.
 * @param {string} filename Name of video file to be checked. This must be same
 *     as entry.name() of the video file.
 * @param {function(!Element):boolean} testFunction
 */
function testElement(filename, testFunction) {
  for (const appId in window.appWindows) {
    const contentWindow = window.appWindows[appId].contentWindow;
    if (contentWindow &&
        contentWindow.document.title === filename) {
      const element = contentWindow.document.querySelector('video');
      if (element && testFunction(element)) {
        return true;
      }
    }
  }
  return false;
}

/**
 * Returns if the specified file is being played.
 *
 * @param {string} filename Name of video file to be checked. This must be same
 *     as entry.name() of the video file.
 * @return {boolean} True if the video is playing, false otherwise.
 */
test.util.sync.isPlaying = function(filename) {
  return testElement(filename, element => !element.paused);
};

/**
 * Returns if the specified file is being played.
 *
 * @param {string} filename Name of video file to be checked. This must be same
 *     as entry.name() of the video file.
 * @return {boolean} True if the video is playing, false otherwise.
 */
test.util.sync.isMuted = function(filename) {
  return testElement(filename, element => element.volume === 0);
};

/**
 * Returns if the video has subtitle attached.
 *
 * @param {string} filename Name of video file to be checked. This must be same
 *     as entry.name() of the video file.
 * @return {boolean} True if the video has subtitle, false otherwise.
 */
test.util.sync.hasSubtitle = function(filename) {
  return testElement(filename, element => element.textTracks.length > 0);
};

/**
 * Loads the mock of the cast extension.
 *
 * @param {Window} contentWindow Video player window to be chacked toOB.
 */
test.util.sync.loadMockCastExtension = function(contentWindow) {
  const script = contentWindow.document.createElement('script');
  script.src =
      'chrome-extension://ljoplibgfehghmibaoaepfagnmbbfiga/' +
      'cast_extension_mock/load.js';
  contentWindow.document.body.appendChild(script);
};

/**
 * Opens the main Files app's window and waits until it is ready.
 *
 * @param {!Array<string>} urls URLs to be opened.
 * @param {function(string)} callback Completion callback with the new window's
 *     App ID.
 */
test.util.async.openVideoPlayer = function(urls, callback) {
  openVideoPlayerWindow(urls).then(callback);
};

// Register the test utils.
test.util.registerRemoteTestUtils();
