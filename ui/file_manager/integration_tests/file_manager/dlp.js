// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, RootPath, sendTestMessage} from '../test_util.js';
import {testcase} from '../testcase.js';

import {navigateWithDirectoryTree, remoteCall, setupAndWaitUntilReady} from './background.js';
import {BASIC_LOCAL_ENTRY_SET} from './test_data.js';


/**
 * Tests DLP block toast is shown when a restricted file is copied.
 */
testcase.transferShowDlpToast = async () => {
  // Setup the restrictions.
  await sendTestMessage({name: 'setIsRestrictedDestinationRestriction'});

  const entry = ENTRIES.hello;

  // Open Files app.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Mount a USB volume.
  await sendTestMessage({name: 'mountFakeUsbEmpty'});

  // Wait for the USB volume to mount.
  const usbVolumeQuery = '#directory-tree [volume-type-icon="removable"]';
  await remoteCall.waitForElement(appId, usbVolumeQuery);

  // Select the file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'selectFile', appId, [entry.nameText]));

  // Copy the file.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['copy']));

  // Select USB volume.
  await navigateWithDirectoryTree(appId, '/fake-usb');

  // Paste the file to begin a copy operation.
  chrome.test.assertTrue(
      await remoteCall.callRemoteTestUtil('execCommand', appId, ['paste']));

  // Check that a toast is displayed because copy is disallowed.
  await remoteCall.waitForElement(appId, '#toast');
};

/**
 * Tests that if the file is restricted by DLP, a managed icon is shown in the
 * detail list .
 */
testcase.dlpShowManagedIcon = async () => {
  // Setup the restrictions.
  await sendTestMessage({name: 'setIsRestrictedByAnyRuleRestrictions'});

  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);

  // Check: only three of the five files should have the 'dlp-managed-icon'
  // class, which means that the icon is displayed.
  await remoteCall.waitForElementsCount(
      appId, ['#file-list .dlp-managed-icon'], 3);
};
