// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getCaller, pending, repeatUntil, RootPath} from '../test_util.js';
import {testcase} from '../testcase.js';

import {remoteCall, setupAndWaitUntilReady} from './background.js';

/**
 * Tests restoring window geometry of the Files app.
 */
testcase.restoreGeometry = async () => {
  // Set up Files app.
  let appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Resize the window to minimal dimensions.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('resizeWindow', appId, [640, 480]));

  // Check the current window's size.
  await remoteCall.waitForWindowGeometry(appId, 640, 480);

  // Enlarge the window by 10 pixels.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('resizeWindow', appId, [650, 490]));

  // Check the current window's size.
  await remoteCall.waitForWindowGeometry(appId, 650, 490);

  // Open another window, where the current view is restored.
  appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Check the next window's size.
  await remoteCall.waitForWindowGeometry(appId, 650, 490);
};

/**
 * Tests restoring a maximized Files app window.
 */
testcase.restoreGeometryMaximized = async () => {
  const caller = getCaller();

  // Set up Files app.
  let appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Maximize the window
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('maximizeWindow', appId, []));

  // Check that the first window is maximized.
  await repeatUntil(async () => {
    if (await remoteCall.callRemoteTestUtil('isWindowMaximized', appId, [])) {
      return true;
    }
    return pending(caller, 'Waiting window maximized...');
  });

  // Close the window.
  await remoteCall.closeWindowAndWait(appId);

  // Open a Files app window again.
  appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Check that the newly opened window is maximized initially.
  await repeatUntil(async () => {
    if (await remoteCall.callRemoteTestUtil('isWindowMaximized', appId, [])) {
      return true;
    }
    return pending(caller, 'Waiting window maximized...');
  });
};
