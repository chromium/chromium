// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Tests that breadcrumbs work.
 */

'use strict';

(() => {
  testcase.breadcrumbsNavigate = async () => {
    const files = [ENTRIES.hello, ENTRIES.photos];
    const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, files, []);

    // Navigate to Downloads/photos.
    await remoteCall.navigateWithDirectoryTree(
        appId, '/Downloads/photos', 'My files');

    // Use the breadcrumbs to navigate back to Downloads.
    await remoteCall.waitAndClickElement(
        appId, '#location-breadcrumbs .breadcrumb-path:nth-of-type(2)');

    // Wait for the contents of Downloads to load again.
    await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows(files));

    // A user action should have been recorded for the breadcrumbs.
    chrome.test.assertEq(
        1, await getUserActionCount('FileBrowser.ClickBreadcrumbs'));
  };

  /**
   * Test that clicking on the current directory in the Breadcrumbs doesn't
   * leave the focus in the breadcrumbs. crbug.com/944022
   */
  testcase.breadcrumbsLeafNoFocus = async () => {
    const appId =
        await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.photos], []);

    // Navigate to Downloads/photos.
    await remoteCall.navigateWithDirectoryTree(
        appId, '/Downloads/photos', 'My files');

    // Focus and click on "photos" in the breadcrumbs.
    const leafBreadCrumb =
        '#location-breadcrumbs .breadcrumb-path.breadcrumb-last';
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil('focus', appId, [leafBreadCrumb]),
        'focus failed: ' + leafBreadCrumb);
    await remoteCall.waitAndClickElement(appId, leafBreadCrumb);

    // Wait focus to not be on breadcrumb clicked.
    await remoteCall.waitForElementLost(appId, leafBreadCrumb + ':focus');

    // Focus should be on file list.
    await remoteCall.waitForElement(appId, '#file-list:focus');
  };

  /**
   * Tests that Downloads is translated in the breadcrumbs.
   */
  testcase.breadcrumbsDownloadsTranslation = async () => {
    // Switch UI to Portuguese (Portugal).
    await sendTestMessage({name: 'switchLanguage', language: 'pt-PT'});

    // Reload Files app to pick up the new language.
    await remoteCall.callRemoteTestUtil('reload', null, []);

    // Open Files app.
    const appId =
        await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.photos], []);

    // Check the breadcrumbs for Downloads:
    // Os meu ficheiros => My files.
    // Transferências => Downloads (as in Transfers).
    let path =
        await remoteCall.callRemoteTestUtil('getBreadcrumbPath', appId, []);
    chrome.test.assertEq('/Os meus ficheiros/Transferências', path);

    // Navigate to Downloads/photos.
    await remoteCall.waitAndClickElement(
        appId, '[full-path-for-testing="/Downloads/photos"]');

    // Wait and check breadcrumb translation.
    await remoteCall.waitUntilCurrentDirectoryIsChanged(
        appId, '/Os meus ficheiros/Transferências/photos');
  };
})();
