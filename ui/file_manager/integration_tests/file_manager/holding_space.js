// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

/**
 * Tests that the holding space welcome banner is hidden when the feature is
 * disabled.
 */
testcase.holdingSpaceWelcomeBannerWithFeatureDisabled = async () => {
  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Check: the holding space welcome banner should be hidden.
  await remoteCall.waitForElement(appId, '.holding-space-welcome[hidden]');
};

/**
 * Tests that the holding space welcome banner appears when the feature is
 * enabled and that it can be dismissed.
 */
testcase.holdingSpaceWelcomeBannerWithFeatureEnabled = async () => {
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
  await remoteCall.waitForElement(appId, '.holding-space-welcome[hidden]');
};

/**
 * Tests that the holding space welcome banner will not continue to show after
 * having been shown enough times to reach the limit.
 */
testcase.holdingSpaceWelcomeBannerWontShowAfterReachingLimit = async () => {
  // Open Files app on Downloads.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Check: the holding space welcome banner should appear.
  await remoteCall.waitForElement(
      appId, '.holding-space-welcome:not([hidden])');

  // Open two new windows on Downloads.
  const windowIds = await Promise.all(
      [openNewWindow(RootPath.DOWNLOADS), openNewWindow(RootPath.DOWNLOADS)]);

  // Check: the holding space welcome banner should appear for both windows.
  await Promise.all([
    remoteCall.waitForElement(
        windowIds[0], '.holding-space-welcome:not([hidden])'),
    remoteCall.waitForElement(
        windowIds[1], '.holding-space-welcome:not([hidden])')
  ]);

  // Open a fourth window on Downloads.
  windowIds.push(await openNewWindow(RootPath.DOWNLOADS));

  // Check: the holding space welcome banner should be hidden.
  await remoteCall.waitForElement(
      windowIds[2], '.holding-space-welcome[hidden]');
};

/**
 * Tests that the holding space welcome banner will not show on Drive.
 */
testcase.holdingSpaceWelcomeBannerWontShowOnDrive = async () => {
  // Open Files app on Drive.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE);

  // Check: the holding space welcome banner should be hidden.
  await remoteCall.waitForElement(appId, '.holding-space-welcome[hidden]');
};
