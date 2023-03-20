// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Tests opening hosted files in the browser.
 */

import {ENTRIES, getBrowserWindows, RootPath, sendTestMessage, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {remoteCall, setupAndWaitUntilReady} from './background.js';

/**
 * Tests opening a hosted file from Files app.
 *
 * @param {Object<TestEntryInfo>} entry FileSystem entry to use.
 */
async function hostedFileOpen(entry) {
  await sendTestMessage({
    name: 'expectFileTask',
    fileNames: [entry.targetPath],
    openType: 'launch',
  });
  // Open Files.App on drive path, add entry to Drive.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE, [], [entry]);

  const path = entry.nameText;
  // Open the file from Files app.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('openFile', appId, [path]));

  // The SWA window itself is detected by getBrowserWindows().
  const initialWindowCount = 1;
  // Wait for a new browser window to appear.
  const browserWindows = await getBrowserWindows(initialWindowCount);

  // Find the main (normal) browser window.
  let normalWindow = undefined;
  for (let i = 0; i < browserWindows.length; ++i) {
    if (browserWindows[i].type === 'normal') {
      normalWindow = browserWindows[i];
      break;
    }
  }
  // Check we have found a 'normal' browser window.
  chrome.test.assertTrue(normalWindow !== undefined);
  // Check we have only one tab opened from trying to open the file.
  const tabs = normalWindow.tabs;
  chrome.test.assertTrue(tabs.length === 1);
  // Get the url of the tab, which may still be pending.
  const url = new URL(tabs[0].pendingUrl || tabs[0].url);
  // Check the end of the URL matches the file we tried to open.
  chrome.test.assertEq(url.hostname, 'document_alternate_link');
  chrome.test.assertEq(
      url.pathname, '/' + encodeURIComponent(entry.targetPath));
}

testcase.hostedOpenDrive = () => {
  return hostedFileOpen(ENTRIES.testDocument);
};

testcase.encryptedHostedOpenDrive = () => {
  return hostedFileOpen(ENTRIES.testCSEDocument);
};
