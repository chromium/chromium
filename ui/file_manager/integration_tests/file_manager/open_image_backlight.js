// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(() => {
  const BACKLIGHT_APP_ID = 'jhdjimmaggjajfjphpljagpgkidjilnj';

  /** Tests opening an image opens Backlight. */
  testcase.imageOpenBacklight = async () => {
    await sendTestMessage({
      name: 'expectFileTask',
      fileNames: [ENTRIES.image3.targetPath],
      openType: 'launch'
    });

    // Open Files.App on Downloads, add image3 to Downloads.
    const filesAppId =
        await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.image3], []);

    // Open the image file in Files app.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'openFile', filesAppId, [ENTRIES.image3.targetPath]));

    // Wait for Backlight to open.
    const caller = getCaller();
    await repeatUntil(async () => {
      const result = await sendTestMessage({
        name: 'hasSwaStarted',
        swaAppId: BACKLIGHT_APP_ID,
      });

      if (result !== 'true') {
        return pending(caller, 'Waiting for Backlight to open');
      }
    });
  };
})();
