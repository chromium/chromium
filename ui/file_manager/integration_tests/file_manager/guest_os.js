// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, RootPath, sendTestMessage} from '../test_util.js';
import {testcase} from '../testcase.js';

import {IGNORE_APP_ERRORS, remoteCall, setupAndWaitUntilReady} from './background.js';

/**
 * Tests that Guest OS entries show up in the sidebar at files app launch.
 */
testcase.fakesListed = async () => {
  // Prepopulate the list with a bunch of guests.
  const names = ['Electra', 'Etcetera', 'Jemima'];
  for (const name of names) {
    await sendTestMessage({name: 'registerMountableGuest', displayName: name});
  }

  // Open the files app.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Check that our guests are listed.
  for (const name of names) {
    await remoteCall.waitForElement(
        appId, `#directory-tree [entry-label="${name}"]`);
  }
};

/**
 * Tests that the list of guests is updated when new guests are added or
 * removed.
 */
testcase.listUpdatedWhenGuestsChanged = async () => {
  // Open the files app.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // We'll use this query a lot to check how many guests we have.
  const query = '#directory-tree [root-type-icon=android_files]';

  const names = ['Etcetera', 'Electra'];
  const ids = [];

  for (const name of names) {
    // Add a guest...
    ids.push(await sendTestMessage(
        {name: 'registerMountableGuest', displayName: name, vmType: 'arcvm'}));

    // ...and it should show up.
    await remoteCall.waitForElement(
        appId, `#directory-tree [entry-label="${name}"]`);
  }

  // Check that we have the right number of entries.
  await remoteCall.waitForElementsCount(appId, [query], ids.length);

  // Remove the guests...
  for (const guestId of ids) {
    await sendTestMessage({name: 'unregisterMountableGuest', guestId: guestId});
  }

  // ...and they should all be gone.
  await remoteCall.waitForElementsCount(appId, [query], 0);

  // Then add them back for good measure.
  for (const name of names) {
    await sendTestMessage(
        {name: 'registerMountableGuest', displayName: name, vmType: 'arcvm'});
  }
  await remoteCall.waitForElementsCount(appId, [query], names.length);
};

/**
 * Tests that clicking on a Guest OS entry in the sidebar mounts the
 * corresponding volume, and that the UI is updated appropriately (volume in
 * sidebar and not fake, contents show up once done loading, etc).
 */
testcase.mountGuestSuccess = async () => {
  const guestName = 'JennyAnyDots';
  // Start off with one guest.
  const guestId = await sendTestMessage({
    name: 'registerMountableGuest',
    displayName: guestName,
    canMount: true,
    vmType: 'bruschetta',
  });
  // Open the files app.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Wait for our guest to appear and click it.
  const placeholderQuery = '#directory-tree [root-type-icon=bruschetta]';
  const volumeQuery =
      `.tree-item[volume-type-for-testing="guest_os"][entry-label="${
          guestName}"]`;
  await remoteCall.waitAndClickElement(appId, placeholderQuery);

  // Wait until it's loaded.
  await remoteCall.waitForElement(
      appId, `#breadcrumbs[path="My files/${guestName}"]`);

  // We should have a volume in the sidebar.
  await remoteCall.waitForElement(appId, volumeQuery);
  await remoteCall.waitForElement(
      appId, '#directory-tree [volume-type-icon=bruschetta]');

  // We should no longer have a fake.
  await remoteCall.waitForElementsCount(appId, [placeholderQuery], 0);

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
  await remoteCall.waitForElementsCount(appId, [placeholderQuery], 1);

  // And no more volume.
  await remoteCall.waitForElementsCount(appId, [volumeQuery], 0);
};

/**
 * Tests that clicking on a Guest OS Android entry in the sidebar mounts the
 * corresponding volume, and that the UI is update appropriately (volume in
 * sidebar and not fake, contents show up once done loading, etc).
 */
testcase.mountAndroidVolumeSuccess = async () => {
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
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Wait for our guest to appear and click it.
  const placeholderQuery = '#directory-tree [root-type-icon=android_files]';
  const volumeQuery =
      `.tree-item[volume-type-for-testing="android_files"][entry-label="${
          guestName}"]`;

  await remoteCall.waitAndClickElement(appId, placeholderQuery);

  // Wait until it's loaded.
  await remoteCall.waitForElement(
      appId, `#breadcrumbs[path="My files/${guestName}"]`);

  // We should have a volume in the sidebar.
  await remoteCall.waitForElement(appId, volumeQuery);
  await remoteCall.waitForElement(
      appId, '#directory-tree [volume-type-icon=android_files]');

  // We should no longer have a fake.
  await remoteCall.waitForElementsCount(appId, [placeholderQuery], 0);

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
  await remoteCall.waitForElementsCount(appId, [placeholderQuery], 1);

  // And no more volume.
  await remoteCall.waitForElementsCount(appId, [volumeQuery], 0);
};
