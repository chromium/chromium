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
        'fakeEvent', appId, ['#search-box [type="search"]', 'focus']));

    // Input a text.
    await remoteCall.callRemoteTestUtil(
        'inputText', appId, ['#search-box [type="search"]', 'hello']);

    // Notify the element of the input.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeEvent', appId, ['#search-box [type="search"]', 'input']));

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
        'fakeEvent', appId, ['#search-box [type="search"]', 'focus']));

    // Input a text.
    await remoteCall.callRemoteTestUtil(
        'inputText', appId, ['#search-box [type="search"]', 'INVALID TERM']);

    // Notify the element of the input.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeEvent', appId, ['#search-box [type="search"]', 'input']));

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
    await remoteCall.waitAndClickElement(appId, '#search-box .clear');

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

  /**
   * Tests that clearing the search box with keydown crbug.com/910068.
   */
  testcase.searchDownloadsClearSearchKeyDown = async () => {
    // Perform a normal search, to be able to clear the search box.
    const appId = await testcase.searchDownloadsWithResults();

    const clearButton = '#search-box .clear';
    // Wait for clear button.
    await remoteCall.waitForElement(appId, clearButton);

    // Send a enter key to the clear button.
    const enterKey = [clearButton, 'Enter', false, false, false];
    await remoteCall.fakeKeyDown(appId, ...enterKey);

    // Check: Search input field is empty.
    const searchInput =
        await remoteCall.waitForElement(appId, '#search-box [type="search"]');
    chrome.test.assertEq('', searchInput.value);
  };

  /**
   * Tests that the search text entry box stays expanded until the end of user
   * interaction.
   */
  testcase.searchHidingTextEntryField = async () => {
    const entry = ENTRIES.hello;

    // Open Files app on Downloads.
    const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

    // Select an entry in the file list.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'selectFile', appId, [entry.nameText]));

    // Click the toolbar search button.
    await remoteCall.waitAndClickElement(appId, '#search-button');

    // Verify the toolbar search text entry box is enabled.
    let textInputElement =
        await remoteCall.waitForElement(appId, ['#search-box cr-input']);
    chrome.test.assertEq('false', textInputElement.attributes['aria-disabled']);

    // Send a 'mousedown' to the toolbar 'delete' button.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeEvent', appId, ['#delete-button', 'mousedown']));

    // Verify the toolbar search text entry is still enabled.
    textInputElement =
        await remoteCall.waitForElement(appId, ['#search-box cr-input']);
    chrome.test.assertEq('false', textInputElement.attributes['aria-disabled']);

    // Send a 'mouseup' to the toolbar 'delete' button.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeEvent', appId, ['#delete-button', 'mouseup']));

    // Verify the toolbar search text entry is still enabled.
    textInputElement =
        await remoteCall.waitForElement(appId, ['#search-box cr-input']);
    chrome.test.assertEq('false', textInputElement.attributes['aria-disabled']);
  };

  /**
   * Tests that the search box collapses when empty and Tab out of the box.
   */
  testcase.searchHidingViaTab = async () => {
    const entry = ENTRIES.hello;

    // Open Files app on Downloads.
    const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

    // Search box should start collapsed.
    await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');

    // Click the toolbar search button.
    await remoteCall.waitAndClickElement(appId, '#search-button');

    // Wait for the search box to expand.
    await remoteCall.waitForElementLost(appId, '#search-wrapper[collapsed]');

    // Verify the search input has focus.
    const input =
        await remoteCall.callRemoteTestUtil('deepGetActiveElement', appId, []);
    chrome.test.assertEq(input.attributes['id'], 'input');
    chrome.test.assertEq(input.attributes['aria-label'], 'Search');

    // Send Tab key to focus the next element.
    const result = await sendTestMessage({name: 'dispatchTabKey'});
    chrome.test.assertEq(
        result, 'tabKeyDispatched', 'Tab key dispatch failure');

    // Check: the search box should collapse.
    await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');

  };

  /**
   * Tests that clicking the search button expands and collapses the search box.
   */
  testcase.searchButtonToggles = async () => {
    const entry = ENTRIES.hello;

    // Open Files app on Downloads.
    const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

    // Search box should start collapsed.
    await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');

    // Measure the width of the search box when it's collapsed.
    const collapsedSearchBox = await remoteCall.waitForElementStyles(
        appId, '#search-wrapper', ['width']);

    // Click the toolbar search button.
    await remoteCall.waitAndClickElement(appId, '#search-button');

    // Wait for the search box to expand.
    await remoteCall.waitForElementLost(appId, '#search-wrapper[collapsed]');

    // Check: The search box width should have increased.
    const caller = getCaller();
    await repeatUntil(async () => {
      const element = await remoteCall.waitForElementStyles(
          appId, '#search-wrapper', ['width']);
      if (collapsedSearchBox.renderedWidth >= element.renderedWidth) {
        return pending(caller, 'Waiting search box to expand');
      }
    });

    // Click the toolbar search button again.
    await remoteCall.waitAndClickElement(appId, '#search-button');

    // Check: the search box should collapse.
    await remoteCall.waitForElement(appId, '#search-wrapper[collapsed]');

    // Check: the search box width should decrease.
    await repeatUntil(async () => {
      const element = await remoteCall.waitForElementStyles(
          appId, '#search-wrapper', ['width']);
      if (collapsedSearchBox.renderedWidth < element.renderedWidth) {
        return pending(caller, 'Waiting search box to collapse');
      }
    });
  };

  /**
   * Tests that Files app performs a search at app start up when
   * LaunchParam.searchQuery is specified.
   */
  testcase.searchQueryLaunchParam = async () => {
    addEntries(['local'], BASIC_LOCAL_ENTRY_SET);
    addEntries(['drive'], BASIC_DRIVE_ENTRY_SET);

    // Open Files app with LaunchParam.searchQuery='gdoc'.
    const query = 'gdoc';
    const appState = {searchQuery: query};
    const appId =
        await remoteCall.callRemoteTestUtil('openMainWindow', null, [appState]);

    // Check: search box should be filled with the query.
    const caller = getCaller();
    await repeatUntil(async () => {
      const searchBoxInput =
          await remoteCall.waitForElement(appId, '#search-box cr-input');
      if (searchBoxInput.value !== query) {
        return pending(
            caller, 'Waiting search box to be filled with the query.');
      }
    });

    // Check: "My Drive" directory should be selected because it is the sole
    //        directory that contains query-matched files (*.gdoc).
    const selectedTreeRow = await remoteCall.waitForElement(
        appId, '#directory-tree .tree-row[selected][active]');
    chrome.test.assertTrue(selectedTreeRow.text.includes('My Drive'));

    // Check: Query-matched files should be shown in the files list.
    await repeatUntil(async () => {
      const filenameLabel =
          await remoteCall.waitForElement(appId, '#file-list .filename-label');
      if (!filenameLabel.text.includes(query)) {
        // Pre-search results might be shown only for a moment before the search
        // spinner is shown.
        return pending(caller, 'Waiting files list to be updated.');
      }
    });
  };
})();
