// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(() => {
  const tooltipQueryHidden = 'files-tooltip:not([visible])';
  const tooltipQueryVisible = 'files-tooltip[visible=true]';
  const searchButton = '#search-button[has-tooltip]';
  const viewButton = '#view-button[has-tooltip]';
  const readonlyIndicator =
      '#read-only-indicator[has-tooltip][show-card-tooltip]';
  const fileList = '#file-list';
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

    // Focus an element without tooltip.
    chrome.test.assertTrue(
        await remoteCall.callRemoteTestUtil('focus', appId, [fileList]));

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

    // Go to readonly volume to have readonly indicator.
    await remoteCall.simulateUiClick(
        appId, ['[volume-type-for-testing=android_files]']);

    // Hover another element with card tooltip.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeMouseOver', appId, [readonlyIndicator]));

    // The tooltip should be visible.
    tooltip = await remoteCall.waitForElement(appId, tooltipQueryVisible);
    label =
        await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
    chrome.test.assertEq(
        'The contents of this folder are read-only.' +
        ' Some activities are not supported.',
        label.text);
    chrome.test.assertEq('card-tooltip', tooltip.attributes['class']);
    chrome.test.assertEq('card-label', label.attributes['class']);

    // Send a mouseout event.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeMouseOut', appId, [readonlyIndicator]));

    // The tooltip should be hidden.
    tooltip = await remoteCall.waitForElement(appId, tooltipQueryHidden);

    // Hover a button with ordinary tooltip again.
    tooltip = await remoteCall.waitForElement(appId, tooltipQueryHidden);
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeMouseOver', appId, [searchButton]));

    // The tooltip should be visible.
    tooltip = await remoteCall.waitForElement(appId, tooltipQueryVisible);
    label =
        await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
    chrome.test.assertEq('Search', label.text);

    // Tooltip class should be cleared as ordinary tooltip shown.
    chrome.test.assertEq('', label.attributes['class']);
    chrome.test.assertEq('', tooltip.attributes['class']);
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
    const label =
        await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
    chrome.test.assertEq('Search', label.text);

    // Hover an element with tooltip.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeMouseOver', appId, [searchButton]));

    // The tooltip should be visible.
    tooltip = await remoteCall.waitForElement(appId, tooltipQueryVisible);

    // Click on body to hide tooltip.
    chrome.test.assertTrue(
        await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, ['body']));

    // The tooltip should be hidden.
    tooltip = await remoteCall.waitForElement(appId, tooltipQueryHidden);
  };

  /**
   * Tests that card tooltip is hidden when clicking on body (or anything else).
   */
  testcase.filesCardTooltipClickHides = async () => {
    const appId = await setupAndWaitUntilReady(
        RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

    // Hover a button with tooltip.
    await remoteCall.waitForElement(appId, tooltipQueryHidden);

    // Go to a readonly volume to have the readonly indicator.
    await remoteCall.simulateUiClick(
        appId, ['[volume-type-for-testing=android_files]']);

    // Hover an element with card tooltip.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeMouseOver', appId, [readonlyIndicator]));

    // The tooltip should be visible.
    await remoteCall.waitForElement(appId, tooltipQueryVisible);

    // Click on body to hide tooltip.
    chrome.test.assertTrue(
        await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, ['body']));

    // The tooltip should be hidden.
    await remoteCall.waitForElement(appId, tooltipQueryHidden);
  };

  /**
   * Tests that the tooltip should hide when the window resizes.
   */
  testcase.filesTooltipHidesOnWindowResize = async () => {
    const appId = await setupAndWaitUntilReady(
        RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

    // The tooltip should be hidden.
    await remoteCall.waitForElement(appId, tooltipQueryHidden);

    // Focus a button with tooltip.
    chrome.test.assertTrue(
        await remoteCall.callRemoteTestUtil('focus', appId, [searchButton]));

    // The tooltip should be visible.
    await remoteCall.waitForElement(appId, tooltipQueryVisible);

    // Resize the window.
    await remoteCall.callRemoteTestUtil('resizeWindow', appId, [1200, 1200]);

    // The tooltip should be hidden.
    await remoteCall.waitForElement(appId, tooltipQueryHidden);
  };
})();
