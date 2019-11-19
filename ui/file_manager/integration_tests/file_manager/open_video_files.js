// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

(() => {
  /**
   * Waits until a window having the given filename appears.
   * @param {string} filename Filename of the requested window.
   * @param {Promise} promise Promise to be fulfilled with a found window's ID.
   */
  function waitForPlaying(filename) {
    const caller = getCaller();
    return repeatUntil(async () => {
      if (await videoPlayerApp.callRemoteTestUtil(
              'isPlaying', null, [filename])) {
        return true;
      }
      return pending(
          caller, 'Window with the prefix %s is not found.', filename);
    });
  }

  /**
   * Tests if the video player shows up for the selected movie and that it is
   * loaded successfully.
   *
   * @param {string} path Directory path to be tested.
   */
  async function videoOpen(path) {
    await sendTestMessage({
      name: 'expectFileTask',
      fileNames: ['world.ogv'],
      openType: 'launch',
    });

    const appId = await setupAndWaitUntilReady(path);

    // Open the video.
    chrome.test.assertTrue(
        await remoteCall.callRemoteTestUtil('openFile', appId, ['world.ogv']));

    // Wait for the video to start playing.
    chrome.test.assertTrue(await waitForPlaying('world.ogv'));

    chrome.test.assertEq(
        0, await videoPlayerApp.callRemoteTestUtil('getErrorCount', null, []));
  }

  // Exports test functions.
  testcase.videoOpenDrive = () => {
    return videoOpen(RootPath.DRIVE);
  };

  testcase.videoOpenDownloads = () => {
    return videoOpen(RootPath.DOWNLOADS);
  };
})();
