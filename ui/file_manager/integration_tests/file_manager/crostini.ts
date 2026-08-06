// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, RootPath, sendTestMessage} from '../test_util.js';

import {remoteCall} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';

const LINUX_FILES_TYPE = 'crostini';

export async function mountCrostini() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);

  await remoteCall.mountCrostini(appId);

  // Unmount and ensure fake root is shown.
  remoteCall.callRemoteTestUtil('unmount', null, ['crostini']);
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForPlaceholderItemByType(LINUX_FILES_TYPE);
}

export async function mountCrostiniWithSubFolder() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);
  const directoryTree = await DirectoryTreePageObject.create(appId);
  // Expect the expand icon is hidden for fake Crostini.
  await directoryTree.waitForItemExpandIconToHideByLabel('Linux files');

  // Add a sub folder to Crostini and mount it.
  await remoteCall.mountCrostini(appId, [ENTRIES.photos]);

  // Expect the expand icon shows now.
  await directoryTree.waitForItemExpandIconToShowByLabel('Linux files');
}

export async function enableDisableCrostini() {
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Ensure fake Linux files root is shown.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForPlaceholderItemByType(LINUX_FILES_TYPE);

  // Disable Crostini, then ensure fake Linux files is removed.
  await sendTestMessage({name: 'setCrostiniEnabled', enabled: false});
  await directoryTree.waitForPlaceholderItemLostByType(LINUX_FILES_TYPE);

  // Re-enable Crostini, then ensure fake Linux files is shown again.
  await sendTestMessage({name: 'setCrostiniEnabled', enabled: true});
  await directoryTree.waitForPlaceholderItemByType(LINUX_FILES_TYPE);
}

export async function sharePathWithCrostini() {
  const photos = '#file-list [file-name="photos"]';
  const menuShareWithLinux = '#file-context-menu:not([hidden]) ' +
      '[command="#share-with-linux"]:not([hidden]):not([disabled])';
  const menuNoShareWithLinux = '#file-context-menu:not([hidden]) ' +
      '[command="#share-with-linux"][hidden][disabled="disabled"]';
  const shareMessageShown =
      '#banners > shared-with-crostini-banner:not([hidden])';

  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.photos], []);

  await remoteCall.isolateBannerForTesting(
      appId, 'shared-with-crostini-banner');

  // Ensure fake Linux files root is shown.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForPlaceholderItemByType(LINUX_FILES_TYPE);

  // Mount crostini, and ensure real root is shown.
  await directoryTree.selectPlaceholderItemByType(LINUX_FILES_TYPE);
  await directoryTree.waitForItemByType(LINUX_FILES_TYPE);

  // Go back to downloads, wait for photos dir to be shown.
  await directoryTree.selectItemByLabel('Downloads');
  await remoteCall.waitForElement(appId, photos);

  // Right-click 'photos' directory, ensure 'Share with Linux' is shown.
  remoteCall.callRemoteTestUtil('fakeMouseRightClick', appId, [photos]);
  await remoteCall.waitForElement(appId, menuShareWithLinux);

  // Click on 'Share with Linux', ensure menu is closed.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId,
      ['#file-context-menu [command="#share-with-linux"]']);
  await remoteCall.waitForElement(appId, '#file-context-menu[hidden]');

  // Right-click 'photos' directory, ensure 'Share with Linux' is not shown.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['#file-list [file-name="photos"']);
  await remoteCall.waitForElement(appId, menuNoShareWithLinux);

  // Click 'photos' to go in photos directory, ensure share message is shown.
  await remoteCall.waitForElementLost(
      appId, '#banners > shared-with-crostini-banner');
  remoteCall.callRemoteTestUtil('fakeMouseDoubleClick', appId, [photos]);
  await remoteCall.waitForElement(appId, shareMessageShown);
}
