// Copyright 2022 The Chromium Authors
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
  await remoteCall.waitUntilSelected(appId, entry.nameText);

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
 * detail list and a tooltip is displayed when hovering over that icon.
 */
testcase.dlpShowManagedIcon = async () => {
  // Setup the restrictions.
  await sendTestMessage({name: 'setIsRestrictedByAnyRuleRestrictions'});

  // Open Files app.
  const appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, BASIC_LOCAL_ENTRY_SET, []);
  const dlpManagedIcon = '#file-list .dlp-managed-icon';

  // Check: only three of the five files should have the 'dlp-managed-icon'
  // class, which means that the icon is displayed.
  await remoteCall.waitForElementsCount(appId, [dlpManagedIcon], 3);

  // Hover over an icon: a tooltip should appear.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseOver', appId, [dlpManagedIcon]));

  // Check: the DLP managed icon tooltip should be visible. The full text
  // contains a placeholder for the link so here we only check the first part.
  const labelTextPrefix = 'This file is confidential and subject ' +
      'to restrictions by administrator policy.';
  const label = await remoteCall.waitForElement(
      appId, ['files-tooltip[visible=true]', '#label']);
  chrome.test.assertTrue(label.text.startsWith(labelTextPrefix));
};

/**
 * Tests that if the file is restricted by DLP, the Restriction details context
 * menu item appears and is enabled.
 */
testcase.dlpContextMenuRestrictionDetails = async () => {
  // Setup the restrictions.
  await sendTestMessage({name: 'setIsRestrictedByAnyRuleBlocked'});

  const entry = ENTRIES.hello;

  // Open Files app.
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, [entry], []);

  // Wait for the DLP managed icon to be shown - this also means metadata has
  // been cached and can be used to show the context menu command.
  await remoteCall.waitForElementsCount(
      appId, ['#file-list .dlp-managed-icon'], 1);

  // Select the file.
  await remoteCall.waitUntilSelected(appId, entry.nameText);

  // Right-click on the file.
  await remoteCall.waitAndRightClick(appId, ['.table-row[selected]']);

  // Wait for the context menu to appear.
  await remoteCall.waitForElement(appId, '#file-context-menu:not([hidden])');

  // Wait for the context menu command option to appear.
  await remoteCall.waitForElement(
      appId,
      '#file-context-menu:not([hidden])' +
          ' [command="#dlp-restriction-details"]' +
          ':not([hidden]):not([disabled])');
};
