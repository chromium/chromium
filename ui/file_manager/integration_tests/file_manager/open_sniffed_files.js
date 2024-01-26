// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Tests opening files in the browser using content sniffing.
 */

import {ENTRIES, getBrowserWindows, RootPath, sendTestMessage, TestEntryInfo} from '../test_util.js';
import {testcase} from '../testcase.js';

import {remoteCall, setupAndWaitUntilReady} from './background.js';

/**
 * Tests opening a file with missing filename extension from Files app.
 *
 * @param {string} path Directory path (Downloads or Drive).
 * @param {Record<string, TestEntryInfo>} entry FileSystem entry to use.
 */
async function sniffedFileOpen(path, entry) {
  await sendTestMessage({
    name: 'expectFileTask',
    // @ts-ignore: error TS4111: Property 'targetPath' comes from an index
    // signature, so it must be accessed with ['targetPath'].
    fileNames: [entry.targetPath],
    openType: 'launch',
  });
  // Open Files.App on |path|, add imgpdf to Downloads and Drive.
  // @ts-ignore: error TS2322: Type 'Record<string, TestEntryInfo>' is not
  // assignable to type 'TestEntryInfo'.
  const appId = await setupAndWaitUntilReady(path, [entry], [entry]);

  // Open the file from Files app.
  // @ts-ignore: error TS4111: Property 'mimeType' comes from an index
  // signature, so it must be accessed with ['mimeType'].
  if (entry.mimeType === 'application/pdf') {
    // When SWA is enabled, Backlight is also enabled and becomes the default
    // handler for PDF files. So we have to use the "open with" option to open
    // in the browser.

    // Select the file.
    // @ts-ignore: error TS4111: Property 'targetPath' comes from an index
    // signature, so it must be accessed with ['targetPath'].
    await remoteCall.waitUntilSelected(appId, entry.targetPath);

    // Right-click the selected file.
    await remoteCall.waitAndRightClick(appId, '.table-row[selected]');

    // Wait for the file context menu to appear.
    await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');
    await remoteCall.waitAndClickElement(
        appId,
        '#file-context-menu:not([hidden]) ' +
            ' [command="#open-with"]:not([hidden])');

    // Wait for the sub-menu to appear.
    await remoteCall.waitForElement(appId, '#tasks-menu:not([hidden])');

    // Wait for the sub-menu item "View".
    await remoteCall.waitAndClickElement(
        appId,
        '#tasks-menu:not([hidden]) [file-type-icon="pdf"]:not([hidden])');
  } else {
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        // @ts-ignore: error TS4111: Property 'targetPath' comes from an index
        // signature, so it must be accessed with ['targetPath'].
        'openFile', appId, [entry.targetPath]));
  }

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
  const url = tabs[0].pendingUrl || tabs[0].url;
  // Check the end of the URL matches the file we tried to open.
  const tail = url.replace(/.*\//, '');
  // @ts-ignore: error TS4111: Property 'targetPath' comes from an index
  // signature, so it must be accessed with ['targetPath'].
  chrome.test.assertTrue(tail === entry.targetPath);
}

// @ts-ignore: error TS4111: Property 'pdfOpenDownloads' comes from an index
// signature, so it must be accessed with ['pdfOpenDownloads'].
testcase.pdfOpenDownloads = () => {
  // @ts-ignore: error TS4111: Property 'imgPdf' comes from an index signature,
  // so it must be accessed with ['imgPdf'].
  return sniffedFileOpen(RootPath.DOWNLOADS, ENTRIES.imgPdf);
};

// @ts-ignore: error TS4111: Property 'pdfOpenDrive' comes from an index
// signature, so it must be accessed with ['pdfOpenDrive'].
testcase.pdfOpenDrive = () => {
  // @ts-ignore: error TS4111: Property 'imgPdf' comes from an index signature,
  // so it must be accessed with ['imgPdf'].
  return sniffedFileOpen(RootPath.DRIVE, ENTRIES.imgPdf);
};

// @ts-ignore: error TS4111: Property 'textOpenDownloads' comes from an index
// signature, so it must be accessed with ['textOpenDownloads'].
testcase.textOpenDownloads = () => {
  // @ts-ignore: error TS4111: Property 'plainText' comes from an index
  // signature, so it must be accessed with ['plainText'].
  return sniffedFileOpen(RootPath.DOWNLOADS, ENTRIES.plainText);
};

// @ts-ignore: error TS4111: Property 'textOpenDrive' comes from an index
// signature, so it must be accessed with ['textOpenDrive'].
testcase.textOpenDrive = () => {
  // @ts-ignore: error TS4111: Property 'plainText' comes from an index
  // signature, so it must be accessed with ['plainText'].
  return sniffedFileOpen(RootPath.DRIVE, ENTRIES.plainText);
};
