// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getCaller, pending, repeatUntil, RootPath, sendTestMessage} from '../test_util.js';
import {testcase} from '../testcase.js';

import {navigateWithDirectoryTree, openNewWindow, remoteCall, setupAndWaitUntilReady} from './background.js';
import {waitForDialog} from './file_dialog.js';

/**
 * Tests that the holding space welcome banner appears and that it can be
 * dismissed.
 */
testcase.holdingSpaceWelcomeBanner = async () => {
  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  let holdingSpaceBannerShown = '.holding-space-welcome:not([hidden][style])';
  let holdingSpaceBannerDismissButton =
      '.holding-space-welcome cr-button.text-button';
  let holdingSpaceBannerHidden = '.holding-space-welcome[hidden]';
  if (await sendTestMessage({name: 'isBannersFrameworkEnabled'}) === 'true') {
    await remoteCall.isolateBannerForTesting(
        appId, 'holding-space-welcome-banner');
    holdingSpaceBannerShown =
        '#banners > holding-space-welcome-banner:not([hidden])';
    holdingSpaceBannerDismissButton = [
      '#banners > holding-space-welcome-banner', 'educational-banner',
      '#dismiss-button'
    ];
    holdingSpaceBannerHidden =
        '#banners > holding-space-welcome-banner[hidden]';
  }

  // Check: the holding space welcome banner should appear. Note that inline
  // styles are removed once dynamic styles have finished loading.
  await remoteCall.waitForElement(appId, holdingSpaceBannerShown);

  // Dismiss the holding space welcome banner.
  await remoteCall.waitAndClickElement(appId, holdingSpaceBannerDismissButton);

  // Check: the holding space welcome banner should be hidden.
  await remoteCall.waitForElement(appId, holdingSpaceBannerHidden);
};

/**
 * Tests that the holding space welcome banner will not continue to show after
 * having been explicitly dismissed by the user.
 */
testcase.holdingSpaceWelcomeBannerWontShowAfterBeingDismissed = async () => {
  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Check: the holding space welcome banner should appear.
  await remoteCall.waitForElement(
      appId, '.holding-space-welcome:not([hidden])');

  // Dismiss the holding space welcome banner.
  await remoteCall.waitAndClickElement(
      appId, '.holding-space-welcome cr-button.text-button');

  // Check: the holding space welcome banner should be hidden.
  await remoteCall.waitForElement(appId, '.holding-space-welcome[hidden]');

  // Change to My Files folder.
  await navigateWithDirectoryTree(appId, '/My files');

  // Check: the holding space welcome banner should still be hidden.
  await remoteCall.waitForElementLost(appId, '.holding-space-welcome[style]');
  await remoteCall.waitForElement(appId, '.holding-space-welcome[hidden]');
};

/**
 * Tests that the holding space welcome banner will not show for modal dialogs.
 */
testcase.holdingSpaceWelcomeBannerWontShowForModalDialogs = async () => {
  // Open Save as dialog.
  chrome.fileSystem.chooseEntry({type: 'saveFile'}, entry => {});
  const appId = await waitForDialog();

  // Wait to finish initial load.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  // Check: the holding space welcome banner should be hidden.
  await remoteCall.waitForElement(appId, '.holding-space-welcome[hidden]');
};

/**
 * Tests that the holding space welcome banner will show for modal dialogs when
 * using the new banners framework.
 */
testcase.holdingSpaceWelcomeBannerWillShowForModalDialogs = async () => {
  // Open Save as dialog.
  chrome.fileSystem.chooseEntry({type: 'saveFile'}, entry => {});
  const appId = await waitForDialog();

  // Ensure the Holding space welcome banner is the only banner prioritised.
  await remoteCall.isolateBannerForTesting(
      appId, 'holding-space-welcome-banner');

  // Wait to finish initial load.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  // Check: the holding space welcome banner should be visible.
  await remoteCall.waitForElement(
      appId, '#banners > holding-space-welcome-banner:not([hidden])');
};

/**
 * Tests that the holding space welcome banner will not show on Drive.
 */
testcase.holdingSpaceWelcomeBannerWontShowOnDrive = async () => {
  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Check: the holding space welcome banner should appear.
  await remoteCall.waitForElement(
      appId, '.holding-space-welcome:not([hidden])');

  // Change to My Drive folder.
  await navigateWithDirectoryTree(appId, '/My Drive');

  // Check: the holding space welcome banner should be hidden.
  await remoteCall.waitForElement(appId, '.holding-space-welcome[hidden]');
};

/**
 * Tests that the holding space welcome banner will update its text depending on
 * whether or not tablet mode is enabled.
 */
testcase.holdingSpaceWelcomeBannerOnTabletModeChanged = async () => {
  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Check: `document.body` should indicate that tablet mode is disabled.
  chrome.test.assertFalse(
      document.body.classList.contains('tablet-mode-enabled'));

  // Async function which repeats until the element matching the specified
  // `query` has a calculated display matching the specified `displayValue`.
  async function waitForElementWithDisplay(query, displayValue) {
    await repeatUntil(async () => {
      const caller = getCaller();
      const el =
          await remoteCall.waitForElementStyles(appId, query, ['display']);
      if (el && el.styles && el.styles.display === displayValue) {
        return el;
      }
      return pending(
          caller, 'Element `%s` with display `%s` is not found.', query,
          displayValue);
    });
  }

  const isBannersFrameworkEnabled =
      await sendTestMessage({name: 'isBannersFrameworkEnabled'}) === 'true';

  // Cache queries for `text` and `textInTabletMode`.
  let text = '.holding-space-welcome-text:not(.tablet-mode-enabled)';
  let textInTabletMode = '.holding-space-welcome-text.tablet-mode-enabled';
  let expectedTextDisplayValue = 'block';
  if (isBannersFrameworkEnabled) {
    await remoteCall.isolateBannerForTesting(
        appId, 'holding-space-welcome-banner');
    text = [
      '#banners > holding-space-welcome-banner:not([hidden])',
      'educational-banner > span[slot="subtitle"].tablet-mode-disabled'
    ];
    textInTabletMode = [
      '#banners > holding-space-welcome-banner:not([hidden])',
      'educational-banner > span[slot="subtitle"].tablet-mode-enabled'
    ];
    expectedTextDisplayValue = 'inline';
  }
  // Check: `text` should be displayed but `textInTabletMode` should not.
  await waitForElementWithDisplay(text, expectedTextDisplayValue);
  await waitForElementWithDisplay(textInTabletMode, 'none');

  // Perform and check: enable tablet mode.
  await sendTestMessage({name: 'enableTabletMode'}).then((result) => {
    chrome.test.assertEq(result, 'tabletModeEnabled');
  });

  // Check: `textInTabletMode` should be displayed but `text` should not.
  await waitForElementWithDisplay(text, 'none');
  await waitForElementWithDisplay(textInTabletMode, 'block');

  // Perform and check: disable tablet mode.
  await sendTestMessage({name: 'disableTabletMode'}).then((result) => {
    chrome.test.assertEq(result, 'tabletModeDisabled');
  });

  // Check: `text` should be displayed but `textInTabletMode` should not.
  await waitForElementWithDisplay(text, expectedTextDisplayValue);
  await waitForElementWithDisplay(textInTabletMode, 'none');
};
