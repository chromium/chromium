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
 * Tests context menu default task label for Google Drive items.
 *
 * @param {Record<string, TestEntryInfo>} entry FileSystem entry to use.
 * @param {string} expected_label Label of the context menu item.
 */
async function checkDefaultTaskLabel(entry, expected_label) {
  // Open Files.App on drive path, add entry to Drive.
  // @ts-ignore: error TS2740: Type 'Record<string, TestEntryInfo>' is missing
  // the following properties from type 'TestEntryInfo': type, sourceFileName,
  // thumbnailFileName, targetPath, and 19 more.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE, [], [entry]);

  // Select the file.
  await remoteCall.waitAndClickElement(
      // @ts-ignore: error TS4111: Property 'nameText' comes from an index
      // signature, so it must be accessed with ['nameText'].
      appId, `#file-list [file-name="${entry.nameText}"]`);

  // Right-click the selected file.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil(
          'fakeMouseRightClick', appId, ['.table-row[selected]']),
      'fakeMouseRightClick failed');

  // Wait for the context menu to appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Get the default task context menu item.
  const element =
      await remoteCall.waitForElement(appId, '[command="#default-task"]');

  // Verify the context menu item's label.
  // @ts-ignore: error TS2339: Property 'innerText' does not exist on type
  // 'ElementObject'.
  chrome.test.assertEq(expected_label, element.innerText);
}

// @ts-ignore: error TS4111: Property 'hostedHasDefaultTask' comes from an index
// signature, so it must be accessed with ['hostedHasDefaultTask'].
testcase.hostedHasDefaultTask = () => {
  // @ts-ignore: error TS4111: Property 'testDocument' comes from an index
  // signature, so it must be accessed with ['testDocument'].
  return checkDefaultTaskLabel(ENTRIES.testDocument, 'Google Docs');
};

// @ts-ignore: error TS4111: Property 'encryptedHasDefaultTask' comes from an
// index signature, so it must be accessed with ['encryptedHasDefaultTask'].
testcase.encryptedHasDefaultTask = () => {
  // @ts-ignore: error TS4111: Property 'testCSEFile' comes from an index
  // signature, so it must be accessed with ['testCSEFile'].
  return checkDefaultTaskLabel(ENTRIES.testCSEFile, 'Google Drive');
};

/**
 * Tests opening a file from Files app in Web Drive.
 *
 * @param {Record<string, TestEntryInfo>} entry FileSystem entry to use.
 * @param {string} expected_hostname Hostname of the resource the browser is
 * supposed to be navigated to.
 */
async function webDriveFileOpen(entry, expected_hostname) {
  await sendTestMessage({
    name: 'expectFileTask',
    // @ts-ignore: error TS4111: Property 'targetPath' comes from an index
    // signature, so it must be accessed with ['targetPath'].
    fileNames: [entry.targetPath],
    openType: 'launch',
  });
  // Open Files.App on drive path, add entry to Drive.
  // @ts-ignore: error TS2322: Type 'Record<string, TestEntryInfo>' is not
  // assignable to type 'TestEntryInfo'.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE, [], [entry]);

  // @ts-ignore: error TS4111: Property 'nameText' comes from an index
  // signature, so it must be accessed with ['nameText'].
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
  // @ts-ignore: error TS2339: Property 'length' does not exist on type
  // 'Object'.
  for (let i = 0; i < browserWindows.length; ++i) {
    // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
    // expression of type 'number' can't be used to index type 'Object'.
    if (browserWindows[i].type === 'normal') {
      // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
      // expression of type 'number' can't be used to index type 'Object'.
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
  chrome.test.assertEq(expected_hostname, url.hostname);
  chrome.test.assertEq(
      // @ts-ignore: error TS4111: Property 'targetPath' comes from an index
      // signature, so it must be accessed with ['targetPath'].
      url.pathname, '/' + encodeURIComponent(entry.targetPath));
}

// @ts-ignore: error TS4111: Property 'hostedOpenDrive' comes from an index
// signature, so it must be accessed with ['hostedOpenDrive'].
testcase.hostedOpenDrive = () => {
  // @ts-ignore: error TS4111: Property 'testDocument' comes from an index
  // signature, so it must be accessed with ['testDocument'].
  return webDriveFileOpen(ENTRIES.testDocument, 'document_alternate_link');
};

// @ts-ignore: error TS4111: Property 'encryptedHostedOpenDrive' comes from an
// index signature, so it must be accessed with ['encryptedHostedOpenDrive'].
testcase.encryptedHostedOpenDrive = () => {
  // @ts-ignore: error TS4111: Property 'testCSEDocument' comes from an index
  // signature, so it must be accessed with ['testCSEDocument'].
  return webDriveFileOpen(ENTRIES.testCSEDocument, 'document_alternate_link');
};

// @ts-ignore: error TS4111: Property 'encryptedNonHostedOpenDrive' comes from
// an index signature, so it must be accessed with
// ['encryptedNonHostedOpenDrive'].
testcase.encryptedNonHostedOpenDrive = () => {
  // @ts-ignore: error TS4111: Property 'testCSEFile' comes from an index
  // signature, so it must be accessed with ['testCSEFile'].
  return webDriveFileOpen(ENTRIES.testCSEFile, 'file_alternate_link');
};
