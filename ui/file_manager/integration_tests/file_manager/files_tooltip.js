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
  const deleteButton = '#delete-button[has-tooltip]';

  const tooltipShowTimeout = 500;  // ms

  /**
   * $i18n{} labels used when template replacement is disabled.
   *
   * @const {!Object<string, string>}
   */
  const i18nLabelReplacements = {
    'SEARCH_TEXT_LABEL': 'Search',
    'READONLY_INDICATOR_TOOLTIP':
        'The contents of this folder are read-only. ' +
        'Some activities are not supported.',
    'CANCEL_SELECTION_BUTTON_LABEL': 'Cancel selection',
  };

  /**
   * Returns $i18n{} label if devtools code coverage is enabled, otherwise the
   * replaced contents.
   *
   * @param {string} key $i18n{} key of replacement text
   * @return {!Promise<string>}
   */
  async function getExpectedLabelText(key) {
    const isDevtoolsCoverageActive =
        await sendTestMessage({name: 'isDevtoolsCoverageActive'});

    if (isDevtoolsCoverageActive === 'true') {
      return '$i18n{' + key + '}';
    }

    // Verify |key| has a $i18n{} replacement in |i18nLabelReplacements|.
    const label = i18nLabelReplacements[key];
    chrome.test.assertEq('string', typeof label, 'Missing: ' + key);

    return label;
  }

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
    let expectedLabelText = await getExpectedLabelText('SEARCH_TEXT_LABEL');
    tooltip = await remoteCall.waitForElement(appId, tooltipQueryVisible);
    let label =
        await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
    chrome.test.assertEq(expectedLabelText, label.text);

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
    expectedLabelText =
        await getExpectedLabelText('CANCEL_SELECTION_BUTTON_LABEL');
    tooltip = await remoteCall.waitForElement(appId, tooltipQueryVisible);
    label =
        await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
    chrome.test.assertEq(expectedLabelText, label.text);
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
    let expectedLabelText = await getExpectedLabelText('SEARCH_TEXT_LABEL');
    tooltip = await remoteCall.waitForElement(appId, tooltipQueryVisible);
    let label =
        await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
    chrome.test.assertEq(expectedLabelText, label.text);

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
    expectedLabelText =
        await getExpectedLabelText('READONLY_INDICATOR_TOOLTIP');
    tooltip = await remoteCall.waitForElement(appId, tooltipQueryVisible);
    label =
        await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
    chrome.test.assertEq(expectedLabelText, label.text);
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
    expectedLabelText = await getExpectedLabelText('SEARCH_TEXT_LABEL');
    tooltip = await remoteCall.waitForElement(appId, tooltipQueryVisible);
    label =
        await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
    chrome.test.assertEq(expectedLabelText, label.text);

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
    const expectedLabelText = await getExpectedLabelText('SEARCH_TEXT_LABEL');
    tooltip = await remoteCall.waitForElement(appId, tooltipQueryVisible);
    const label =
        await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
    chrome.test.assertEq(expectedLabelText, label.text);

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

  /**
   * Tests that the tooltip is hidden after the 'Delete' confirmation dialog is
   * closed.
   */
  testcase.filesTooltipHidesOnDeleteDialogClosed = async () => {
    const appId = await setupAndWaitUntilReady(
        RootPath.DRIVE, [], [ENTRIES.beautiful, ENTRIES.photos]);

    const fileListItemQuery = '#file-list li[file-name="Beautiful Song.ogg"]';
    const okButtonQuery = '.cr-dialog-ok';
    const cancelButtonQuery = '.cr-dialog-cancel';

    // The tooltip should be hidden.
    await remoteCall.waitForElement(appId, tooltipQueryHidden);

    // Select file.
    await remoteCall.waitAndClickElement(appId, [fileListItemQuery]);

    // Focus delete button.
    chrome.test.assertTrue(
        await remoteCall.callRemoteTestUtil('focus', appId, [deleteButton]));

    // Click delete button.
    await remoteCall.waitAndClickElement(appId, [deleteButton]);

    // Cancel deletion by clicking 'Cancel'.
    await remoteCall.waitAndClickElement(appId, [cancelButtonQuery]);

    // Leave time for tooltip to show.
    await wait(tooltipShowTimeout);

    // The tooltip should still be hidden.
    await remoteCall.waitForElement(appId, tooltipQueryHidden);

    // Select file.
    await remoteCall.waitAndClickElement(appId, [fileListItemQuery]);

    // Focus delete button.
    chrome.test.assertTrue(
        await remoteCall.callRemoteTestUtil('focus', appId, [deleteButton]));

    // Click the delete button.
    await remoteCall.waitAndClickElement(appId, [deleteButton]);

    // Confirm deletion by clicking 'Delete'.
    await remoteCall.waitAndClickElement(appId, [okButtonQuery]);

    // Leave time for tooltip to show.
    await wait(tooltipShowTimeout);

    // The tooltip should be hidden.
    await remoteCall.waitForElement(appId, tooltipQueryHidden);
  };
})();
