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
