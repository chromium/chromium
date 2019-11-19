// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(() => {
  const tooltipQueryHidden = 'files-tooltip:not([visible])';
  const tooltipQueryVisible = 'files-tooltip[visible=true]';
  const searchButton = '#search-button[has-tooltip]';
  const viewButton = '#view-button[has-tooltip]';
  const breadcrumbs = '#breadcrumb-path-0';
  const cancelButton = '#cancel-selection-button[has-tooltip]';

  /**
   * Tests that tooltip is displayed when focusing an element with tooltip.
   */
  testcase.filesTooltipFocus = async () => {
    const appId = await setupAndWaitUntilReady(
        RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

    // Focus a button with tooltip.
    let tooltip = await remoteCall.waitForElement(appId, tooltipQueryHidden);
    chrome.test.assertTrue(
        await remoteCall.callRemoteTestUtil('focus', appId, [searchButton]));

    // The tooltip should be visible.
    tooltip = await remoteCall.waitForElement(appId, tooltipQueryVisible);
    let label =
        await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
    chrome.test.assertEq('Search', label.text);

    // Focus another button with tooltip.
    chrome.test.assertTrue(
        await remoteCall.callRemoteTestUtil('focus', appId, [viewButton]));

    // The tooltip should be visible.
    tooltip = await remoteCall.waitForElement(appId, tooltipQueryVisible);
    label =
        await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
    chrome.test.assertEq('Switch to thumbnail view', label.text);

    // Focus a button without tooltip.
    chrome.test.assertTrue(
        await remoteCall.callRemoteTestUtil('focus', appId, [breadcrumbs]));

    // The tooltip should be hidden.
    tooltip = await remoteCall.waitForElement(appId, tooltipQueryHidden);

    // Select all the files to enable the cancel selection button.
    const ctrlA = ['#file-list', 'a', true, false, false];
    await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, ctrlA);

    // Focus the cancel selection button.
    chrome.test.assertTrue(
        await remoteCall.callRemoteTestUtil('focus', appId, [cancelButton]));

    // The tooltip should be visible.
    tooltip = await remoteCall.waitForElement(appId, tooltipQueryVisible);
    label =
        await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
    chrome.test.assertEq('Cancel selection', label.text);
  };

  /**
   * Tests that tooltip is displayed when hovering an element with tooltip.
   */
  testcase.filesTooltipMouseOver = async () => {
    const appId = await setupAndWaitUntilReady(
        RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

    // Hover a button with tooltip.
    let tooltip = await remoteCall.waitForElement(appId, tooltipQueryHidden);
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeMouseOver', appId, [searchButton]));

    // The tooltip should be visible.
    tooltip = await remoteCall.waitForElement(appId, tooltipQueryVisible);
    let label =
        await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
    chrome.test.assertEq('Search', label.text);

    // Hover another button with tooltip.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeMouseOver', appId, [viewButton]));

    // The tooltip should be visible.
    tooltip = await remoteCall.waitForElement(appId, tooltipQueryVisible);
    label =
        await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
    chrome.test.assertEq('Switch to thumbnail view', label.text);

    // Send a mouseout event.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeMouseOut', appId, [viewButton]));

    // The tooltip should be hidden.
    tooltip = await remoteCall.waitForElement(appId, tooltipQueryHidden);
  };

  /**
   * Tests that tooltip is hidden when clicking on body (or anything else).
   */
  testcase.filesTooltipClickHides = async () => {
    const appId = await setupAndWaitUntilReady(
        RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

    // Hover a button with tooltip.
    let tooltip = await remoteCall.waitForElement(appId, tooltipQueryHidden);
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeMouseOver', appId, [searchButton]));

    // The tooltip should be visible.
    tooltip = await remoteCall.waitForElement(appId, tooltipQueryVisible);
    let label =
        await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
    chrome.test.assertEq('Search', label.text);

    // Hover another button with tooltip.
    chrome.test.assertTrue(
        await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, ['body']));

    // The tooltip should be hidden.
    tooltip = await remoteCall.waitForElement(appId, tooltipQueryHidden);
  };
})();
