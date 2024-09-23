// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getCaller, openEntryChoosingWindow, pending, repeatUntil, RootPath, sendTestMessage} from '../test_util.js';

import {remoteCall} from './background.js';


/**
 * Tests that the holding space welcome banner appears and that it can be
 * dismissed.
 */
export async function holdingSpaceWelcomeBanner() {
  // Open Files app on Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  await remoteCall.isolateBannerForTesting(
      appId, 'holding-space-welcome-banner');
  const holdingSpaceBannerShown =
      '#banners > holding-space-welcome-banner:not([hidden])';
  const holdingSpaceBannerDismissButton = [
    '#banners > holding-space-welcome-banner',
    'educational-banner',
    await remoteCall.isCrosComponents(appId) ? '#dismiss-button' :
                                               '#dismiss-button-old',
  ];
  const holdingSpaceBannerHidden =
      '#banners > holding-space-welcome-banner[hidden]';

  // Check: the holding space welcome banner should appear. Note that inline
  // styles are removed once dynamic styles have finished loading.
  await remoteCall.waitForElement(appId, holdingSpaceBannerShown);

  // Dismiss the holding space welcome banner.
  await remoteCall.waitAndClickElement(appId, holdingSpaceBannerDismissButton);

  // Check: the holding space welcome banner should be hidden.
  await remoteCall.waitForElement(appId, holdingSpaceBannerHidden);
}

/**
 * Tests that the holding space welcome banner will show for modal dialogs when
 * using the new banners framework.
 */
export async function holdingSpaceWelcomeBannerWillShowForModalDialogs() {
  // Open Save as dialog in the foreground window.
  await openEntryChoosingWindow({type: 'saveFile'});

  const appId = await remoteCall.waitForDialog();

  // Ensure the Holding space welcome banner is the only banner prioritised.
  await remoteCall.isolateBannerForTesting(
      appId, 'holding-space-welcome-banner');

  // Wait to finish initial load.
  await remoteCall.waitFor('isFileManagerLoaded', appId, true);

  // Check: the holding space welcome banner should be visible.
  await remoteCall.waitForElement(
      appId, '#banners > holding-space-welcome-banner:not([hidden])');
}

/**
 * Tests that the holding space welcome banner will update its text depending on
 * whether or not tablet mode is enabled.
 */
export async function holdingSpaceWelcomeBannerOnTabletModeChanged() {
  // Open Files app on Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Check: `document.body` should indicate that tablet mode is disabled.
  chrome.test.assertFalse(
      document.body.classList.contains('tablet-mode-enabled'));

  // Async function which repeats until the element matching the specified
  // `query` has a calculated display matching the specified `displayValue`.
  async function waitForElementWithDisplay(
      query: string[], displayValue: string) {
    await repeatUntil(async () => {
      const caller = getCaller();
      const el =
          await remoteCall.waitForElementStyles(appId, query, ['display']);
      if (el && el.styles && el.styles['display'] === displayValue) {
        return el;
      }
      return pending(
          caller, 'Element `%s` with display `%s` is not found.', query,
          displayValue);
    });
  }

  // Cache queries for `text` and `textInTabletMode`.
  await remoteCall.isolateBannerForTesting(
      appId, 'holding-space-welcome-banner');
  const text = [
    '#banners > holding-space-welcome-banner:not([hidden])',
    'educational-banner > span[slot="subtitle"].tablet-mode-disabled',
  ];
  const textInTabletMode = [
    '#banners > holding-space-welcome-banner:not([hidden])',
    'educational-banner > span[slot="subtitle"].tablet-mode-enabled',
  ];
  const expectedTextDisplayValue = 'inline';
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
}
