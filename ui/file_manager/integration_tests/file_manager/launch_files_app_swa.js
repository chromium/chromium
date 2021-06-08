// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(() => {
  /**
   * Tests that files app SWA launches.
   */
  testcase.launchFilesAppSwa = async () => {
    const caller = getCaller();

    const swaAppId = await sendTestMessage({
      name: 'launchFileManagerSwa',
    });

    console.log('file_manager_swa_id: ' + swaAppId);

    const element = await remoteCall.waitForElement(swaAppId, 'body');
    chrome.test.assertEq('Files', element.attributes['aria-label']);
    chrome.test.assertEq('files-ng', element.attributes['class']);
    return IGNORE_APP_ERRORS;
  };
})();
