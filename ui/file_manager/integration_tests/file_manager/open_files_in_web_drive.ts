// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Tests opening hosted files in the browser.
 */

import {ENTRIES, getBrowserWindows, RootPath, sendTestMessage, type TestEntryInfo} from '../test_util.js';

import {remoteCall} from './background.js';


/**
 * Tests context menu default task label for Google Drive items.
 *
 * @param entry FileSystem entry to use.
 * @param expectedLabel Label of the context menu item.
 */
async function checkDefaultTaskLabel(
    entry: TestEntryInfo, expectedLabel: string) {
  // Open Files.App on drive path, add entry to Drive.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE, [], [entry]);

  // Select the file.
  await remoteCall.waitAndClickElement(
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
  chrome.test.assertEq(expectedLabel, element.innerText);
}

export function hostedHasDefaultTask() {
  return checkDefaultTaskLabel(ENTRIES.testDocument, 'Google Docs');
}

export function encryptedHasDefaultTask() {
  return checkDefaultTaskLabel(ENTRIES.testCSEFile, 'Google Drive');
}

/**
 * Tests opening a file from Files app in Web Drive.
 *
 * @param entry FileSystem entry to use.
 * @param expectedHostname Hostname of the resource the browser is
 * supposed to be navigated to.
 */
async function webDriveFileOpen(
    entry: TestEntryInfo, expectedHostname: string) {
  await sendTestMessage({
    name: 'expectFileTask',
    fileNames: [entry.targetPath],
    openType: 'launch',
  });
  // Open Files.App on drive path, add entry to Drive.
  const appId =
      await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE, [], [entry]);

  const path = entry.nameText;
  // Open the file from Files app.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('openFile', appId, [path]));

  // The SWA window itself is detected by getBrowserWindows().
  const initialWindowCount = 1;
  // Wait for a new browser window to appear.
  const browserWindows = await getBrowserWindows(initialWindowCount);

  // Find the main (normal) browser window.
  const normalWindow = browserWindows.find(window => window.type === 'normal');
  // Check we have found a 'normal' browser window.
  chrome.test.assertTrue(normalWindow !== undefined);
  // Check we have only one tab opened from trying to open the file.
  const tabs = normalWindow.tabs;
  chrome.test.assertTrue(tabs !== undefined && tabs.length === 1);
  // Get the url of the tab, which may still be pending.
  const url = new URL(tabs[0].pendingUrl || tabs[0].url);
  // Check the end of the URL matches the file we tried to open.
  chrome.test.assertEq(expectedHostname, url.hostname);
  chrome.test.assertTrue(entry.targetPath !== undefined);
  chrome.test.assertEq(
      url.pathname, '/' + encodeURIComponent(entry.targetPath));
}

export function hostedOpenDrive() {
  return webDriveFileOpen(ENTRIES.testDocument, 'document_alternate_link');
}

export function encryptedHostedOpenDrive() {
  return webDriveFileOpen(ENTRIES.testCSEDocument, 'document_alternate_link');
}

export function encryptedNonHostedOpenDrive() {
  return webDriveFileOpen(ENTRIES.testCSEFile, 'file_alternate_link');
}
