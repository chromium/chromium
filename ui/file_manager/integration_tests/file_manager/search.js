// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';
(() => {
  /**
   * Expected files shown in the search results for 'hello'
   *
   * @type {Array<TestEntryInfo>}
   * @const
   */
  const SEARCH_RESULTS_ENTRY_SET = [
    ENTRIES.hello,
  ];

  /**
   * Tests searching inside Downloads with results.
   */
  testcase.searchDownloadsWithResults = async () => {
    // Open Files app on Downloads.
    const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

    // Click on the search button to display the search box.
    await remoteCall.waitAndClickElement(appId, '#search-button');

    // Focus the search box.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeEvent', appId, ['#search-box cr-input', 'focus']));

    // Input a text.
    await remoteCall.callRemoteTestUtil(
        'inputText', appId, ['#search-box cr-input', 'hello']);

    // Notify the element of the input.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeEvent', appId, ['#search-box cr-input', 'input']));

    // Wait file list to display the search result.
    await remoteCall.waitForFiles(
        appId, TestEntryInfo.getExpectedRows(SEARCH_RESULTS_ENTRY_SET));

    // Check that a11y message for results has been issued.
    const a11yMessages =
        await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);
    chrome.test.assertEq(1, a11yMessages.length, 'Missing a11y message');
    chrome.test.assertEq('Showing results for hello.', a11yMessages[0]);

    return appId;
  };

  /**
   * Tests searching inside Downloads without results.
   */
  testcase.searchDownloadsWithNoResults = async () => {
    // Open Files app on Downloads.
    const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

    // Focus the search box.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeEvent', appId, ['#search-box cr-input', 'focus']));

    // Input a text.
    await remoteCall.callRemoteTestUtil(
        'inputText', appId, ['#search-box cr-input', 'INVALID TERM']);

    // Notify the element of the input.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeEvent', appId, ['#search-box cr-input', 'input']));

    // Wait file list to display no results.
    await remoteCall.waitForFiles(appId, []);

    // Check that a11y message for no results has been issued.
    const a11yMessages =
        await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);
    chrome.test.assertEq(1, a11yMessages.length, 'Missing a11y message');
    chrome.test.assertEq(
        'There are no results for INVALID TERM.', a11yMessages[0]);
  };

  /**
   * Tests that clearing the search box announces the A11y.
   */
  testcase.searchDownloadsClearSearch = async () => {
    // Perform a normal search, to be able to clear the search box.
    const appId = await testcase.searchDownloadsWithResults();

    // Click on the clear search button.
    await remoteCall.waitAndClickElement(appId, '#search-box cr-input .clear');

    // Wait for fil list to display all files.
    await remoteCall.waitForFiles(
        appId, TestEntryInfo.getExpectedRows(BASIC_LOCAL_ENTRY_SET));

    // Check that a11y message for clearing the search term has been issued.
    const a11yMessages =
        await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);
    chrome.test.assertEq(2, a11yMessages.length, 'Missing a11y message');
    chrome.test.assertEq(
        'Search text cleared, showing all files and folders.', a11yMessages[1]);
  };
})();
