// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import {getCaller, pending, repeatUntil, sendTestMessage, type TestEntryInfo, waitForMediaApp} from '../test_util.js';

import {remoteCall} from './background.js';


/**
 * Tests if the media app shows up for the selected file entry and that it has
 * loaded successfully.
 * @param path Directory path (Downloads or Drive).
 * @param entry Selected file entry to open.
 */
export async function opensInMediaApp(path: string, entry: TestEntryInfo) {
  await sendTestMessage({
    name: 'expectFileTask',
    fileNames: [entry.targetPath],
    openType: 'launch',
  });

  // Open Files.App on Downloads/Drive, add `entry` to Downloads/Drive.
  const filesAppId =
      await remoteCall.setupAndWaitUntilReady(path, [entry], [entry]);

  // Open the file in Files app.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'openFile', filesAppId, [entry.targetPath]));

  // Wait for the expected 1 a11y announce.
  const caller = getCaller();
  await repeatUntil(async () => {
    const a11yMessages = await remoteCall.callRemoteTestUtil<string[]>(
        'getA11yAnnounces', filesAppId, []);
    return a11yMessages.length === 1 ?
        true :
        pending(
            caller,
            'Waiting for 1 a11y message, got: ' + JSON.stringify(a11yMessages));
  });

  // Fetch A11y messages.
  const a11yMessages = await remoteCall.callRemoteTestUtil<string[]>(
      'getA11yAnnounces', filesAppId, []);

  // Check that opening the file was announced to screen reader.
  chrome.test.assertEq(1, a11yMessages.length);
  chrome.test.assertEq(`Opening file ${entry.nameText}.`, a11yMessages[0]);

  // Wait for MediaApp to open.
  await waitForMediaApp();
}
