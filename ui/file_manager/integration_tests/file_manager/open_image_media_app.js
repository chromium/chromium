// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(() => {
  /**
   * Tests opening an image opens MediaApp/Backlight.
   * @param {string} path Directory path (Downloads or Drive).
   */
  async function imageOpen(path) {
    await sendTestMessage({
      name: 'expectFileTask',
      fileNames: [ENTRIES.image3.targetPath],
      openType: 'launch'
    });

    // Open Files.App on Downloads, add image3 to Downloads.
    const filesAppId =
        await setupAndWaitUntilReady(path, [ENTRIES.image3], [ENTRIES.image3]);

    // Open the image file in Files app.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'openFile', filesAppId, [ENTRIES.image3.targetPath]));

    // Wait for the expected 1 a11y announce.
    const caller = getCaller();
    await repeatUntil(async () => {
      const a11yMessages = await remoteCall.callRemoteTestUtil(
          'getA11yAnnounces', filesAppId, []);

      if (a11yMessages.length === 1) {
        return true;
      }

      return pending(
          caller,
          'Waiting for 1 a11y message, got: ' + JSON.stringify(a11yMessages));
    });

    // Fetch A11y messages.
    const a11yMessages =
        await remoteCall.callRemoteTestUtil('getA11yAnnounces', filesAppId, []);

    // Check that opening the file was announced to screen reader.
    chrome.test.assertEq(1, a11yMessages.length);
    chrome.test.assertEq('Opening file image3.jpg.', a11yMessages[0]);

    // Wait for MediaApp to open.
    await waitForMediaApp();
  }


  testcase.imageOpenMediaAppDownloads = () => {
    return imageOpen(RootPath.DOWNLOADS);
  };

  testcase.imageOpenMediaAppDrive = () => {
    return imageOpen(RootPath.DRIVE);
  };
})();
