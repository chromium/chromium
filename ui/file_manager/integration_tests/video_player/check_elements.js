// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Checks the initial elements in the video player.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.checkInitialElements = function() {
  const test = openVideos('local', 'downloads', [ENTRIES.world]);
  return test.then(function(args) {
    const appId = args[0];
    return Promise.all([
      remoteCallVideoPlayer.waitForElement(appId, 'div#video-player'),
      remoteCallVideoPlayer.waitForElement(
          appId, '#video-container > video[autopictureinpicture]'),
    ]);
  });
};
