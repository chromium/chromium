// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Tests opening files in the browser using content sniffing.
 */

'use strict';

(() => {
  /**
   * Tests opening a file with missing filename extension from Files app.
   *
   * @param {string} path Directory path (Downloads or Drive).
   * @param {Object<TestEntryInfo>} entry FileSystem entry to use.
   */
  async function sniffedFileOpen(path, entry) {
    await sendTestMessage({
      name: 'expectFileTask',
      fileNames: [entry.targetPath],
      openType: 'launch'
    });
    // Open Files.App on |path|, add imgpdf to Downloads and Drive.
    const appId = await setupAndWaitUntilReady(path, [entry], [entry]);

    // Open the pdf file from Files app.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'openFile', appId, [entry.targetPath]));
    // Wait for a browser window to appear.
    const browserWindows = await getBrowserWindows();
    // Find the main (normal) browser window.
    let normalWindow = undefined;
    for (let i = 0; i < browserWindows.length; ++i) {
      if (browserWindows[i].type == 'normal') {
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
    chrome.test.assertTrue(tail === entry.targetPath);
  }

  testcase.pdfOpenDownloads = () => {
    return sniffedFileOpen(RootPath.DOWNLOADS, ENTRIES.imgPdf);
  };

  testcase.pdfOpenDrive = () => {
    return sniffedFileOpen(RootPath.DRIVE, ENTRIES.imgPdf);
  };

  testcase.textOpenDownloads = () => {
    return sniffedFileOpen(RootPath.DOWNLOADS, ENTRIES.plainText);
  };

  testcase.textOpenDrive = () => {
    return sniffedFileOpen(RootPath.DRIVE, ENTRIES.plainText);
  };
})();
