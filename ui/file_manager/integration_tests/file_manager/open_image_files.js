// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(() => {
  /**
   * Tests opening (then closing) the image Gallery from Files app.
   *
   * @param {string} path Directory path (Downloads or Drive).
   */
  async function imageOpen(path) {
    await sendTestMessage({
      name: 'expectFileTask',
      fileNames: [ENTRIES.image3.targetPath],
      openType: 'launch'
    });

    // Open Files.App on |path|, add image3 to Downloads and Drive.
    const appId =
        await setupAndWaitUntilReady(path, [ENTRIES.image3], [ENTRIES.image3]);

    // Open the image file in Files app.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'openFile', appId, [ENTRIES.image3.targetPath]));

    // Check: the Gallery window should open.
    const galleryAppId = await galleryApp.waitForWindow('gallery.html');

    // Check: the image should appear in the Gallery window.
    await galleryApp.waitForSlideImage(galleryAppId, 640, 480, 'image3');

    // Close the Gallery window.
    chrome.test.assertTrue(
        await galleryApp.closeWindowAndWait(galleryAppId),
        'Failed to close Gallery window');
  }

  /**
   * Tests opening the image Gallery from Files app: once the Gallery opens and
   * shows the initial image, open a different image from FilesApp.
   *
   * @param {string} path Directory path (Downloads or Drive).
   */
  async function imageOpenGalleryOpen(path) {
    await sendTestMessage({
      name: 'expectFileTask',
      fileNames: [ENTRIES.image3.targetPath, ENTRIES.desktop.targetPath],
      openType: 'launch'
    });

    const testImages = [ENTRIES.image3, ENTRIES.desktop];

    // Open Files.App on |path|, add test images to Downloads and Drive.
    const appId = await setupAndWaitUntilReady(path, testImages, testImages);

    // Open an image file in Files app.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'openFile', appId, [ENTRIES.image3.targetPath]));

    // Wait for the expected 3
    const caller = getCaller();
    await repeatUntil(async () => {
      const a11yMessages =
          await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);

      if (a11yMessages.length === 3) {
        return true;
      }

      return pending(
          caller,
          'Waiting for 3 a11y messages, got: ' + JSON.stringify(a11yMessages));
    });

    // Fetch A11y messages.
    const a11yMessages =
        await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);

    // Check that opening the file was announced to screen reader.
    chrome.test.assertEq(3, a11yMessages.length);
    chrome.test.assertEq('Opening file image3.jpg.', a11yMessages[2]);

    // Check: the Gallery window should open.
    const galleryAppId = await galleryApp.waitForWindow('gallery.html');

    // Check: the image should appear in the Gallery window.
    await galleryApp.waitForSlideImage(galleryAppId, 640, 480, 'image3');

    // Now open a different image file in Files app.
    await remoteCall.callRemoteTestUtil(
        'openFile', appId, [ENTRIES.desktop.targetPath]);

    // Check: the new image should appear in the Gallery window.
    await galleryApp.waitForSlideImage(
        galleryAppId, 800, 600, 'My Desktop Background');
  }

  testcase.imageOpenDownloads = () => {
    return imageOpen(RootPath.DOWNLOADS);
  };

  testcase.imageOpenDrive = () => {
    return imageOpen(RootPath.DRIVE);
  };

  testcase.imageOpenGalleryOpenDownloads = () => {
    return imageOpenGalleryOpen(RootPath.DOWNLOADS);
  };

  testcase.imageOpenGalleryOpenDrive = () => {
    return imageOpenGalleryOpen(RootPath.DRIVE);
  };
})();
