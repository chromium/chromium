// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, RootPath, sendTestMessage} from '../test_util.js';

import {remoteCall} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';

/**
 * Tests that Guest OS entries show up in the sidebar at files app launch.
 */
export async function fakesListed() {
  // Prepopulate the list with a bunch of guests.
  const names = ['Electra', 'Etcetera', 'Jemima'];
  for (const name of names) {
    await sendTestMessage({name: 'registerMountableGuest', displayName: name});
  }

  // Open the files app.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Check that our guests are listed.
  const directoryTree = await DirectoryTreePageObject.create(appId);

  for (const name of names) {
    await directoryTree.waitForItemByLabel(name);
  }
}

/**
 * Tests that the list of guests is updated when new guests are added or
 * removed.
 */
export async function listUpdatedWhenGuestsChanged() {
  // Open the files app.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);

  const names = ['Etcetera', 'Electra'];
  const ids = [];

  const directoryTree = await DirectoryTreePageObject.create(appId);

  for (const name of names) {
    // Add a guest...
    ids.push(await sendTestMessage(
        {name: 'registerMountableGuest', displayName: name, vmType: 'arcvm'}));

    // ...and it should show up.
    await directoryTree.waitForItemByLabel(name);
  }

  // Check that we have the right number of entries.
  await directoryTree.waitForPlaceholderItemsCountByType(
      'android_files', ids.length);

  // Remove the guests...
  for (const guestId of ids) {
    await sendTestMessage({name: 'unregisterMountableGuest', guestId: guestId});
  }

  // ...and they should all be gone.
  await directoryTree.waitForPlaceholderItemsCountByType('android_files', 0);

  // Then add them back for good measure.
  for (const name of names) {
    await sendTestMessage(
        {name: 'registerMountableGuest', displayName: name, vmType: 'arcvm'});
  }
  await directoryTree.waitForPlaceholderItemsCountByType(
      'android_files', names.length);
}

/**
 * Tests that clicking on a Guest OS entry in the sidebar mounts the
 * corresponding volume, and that the UI is updated appropriately (volume in
 * sidebar and not fake, contents show up once done loading, etc).
 */
export async function mountGuestSuccess() {
  const guestName = 'JennyAnyDots';
  // Start off with one guest.
  const guestId = await sendTestMessage({
    name: 'registerMountableGuest',
    displayName: guestName,
    canMount: true,
    vmType: 'bruschetta',
  });
  // Open the files app.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);
  const directoryTree = await DirectoryTreePageObject.create(appId);

  // Wait for our guest to appear and click it.
  await directoryTree.selectPlaceholderItemByType('bruschetta');

  // Wait until it's loaded.
  await remoteCall.waitForElement(
      appId, `#breadcrumbs[path="My files/${guestName}"]`);

  // We should have a volume in the sidebar.
  await directoryTree.waitForItemByLabel(guestName);
  await directoryTree.waitForItemByType('bruschetta');

  // We should no longer have a fake.
  await directoryTree.waitForPlaceholderItemLostByType('bruschetta');

  // And the volume should be focused in the main window.
  await remoteCall.waitForElement(
      appId, `#list-container[scan-completed="${guestName}"]`);

  // It should not be read-only.
  await remoteCall.waitForElement(appId, '#read-only-indicator[hidden]');

  // Unmount the volume.
  await sendTestMessage({
    name: 'unmountGuest',
    guestId: guestId,
  });

  // We should have our fake back.
  await directoryTree.waitForPlaceholderItemByType('bruschetta');

  // And no more volume.
  await directoryTree.waitForItemLostByType('bruschetta');
}

/**
 * Tests that clicking on a Guest OS Android entry in the sidebar mounts the
 * corresponding volume, and that the UI is update appropriately (volume in
 * sidebar and not fake, contents show up once done loading, etc).
 */
export async function mountAndroidVolumeSuccess() {
  await sendTestMessage({name: 'unmountPlayFiles'});
  const guestName = 'Play files';
  // Start off with one guest.
  const guestId = await sendTestMessage({
    name: 'registerMountableGuest',
    displayName: guestName,
    canMount: true,
    vmType: 'arcvm',
  });
  // Open the files app.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Wait for our guest to appear and click it.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectPlaceholderItemByType('android_files');

  // Wait until it's loaded.
  await remoteCall.waitForElement(
      appId, `#breadcrumbs[path="My files/${guestName}"]`);

  // We should have a volume in the sidebar.
  await directoryTree.waitForItemByLabel(guestName);
  await directoryTree.waitForItemByType('android_files');

  // We should no longer have a fake.
  await directoryTree.waitForPlaceholderItemLostByType('android_files');

  // And the volume should be focused in the main window.
  await remoteCall.waitForElement(
      appId, `#list-container[scan-completed="${guestName}"]`);

  // It should not be read-only.
  await remoteCall.waitForElement(appId, '#read-only-indicator');

  // Unmount the volume.
  await sendTestMessage({
    name: 'unmountGuest',
    guestId: guestId,
  });

  // We should have our fake back.
  await directoryTree.waitForPlaceholderItemByType('android_files');

  // And no more volume.
  await directoryTree.waitForItemLostByType('android_files');
}
