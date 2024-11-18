// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, getCaller, pending, repeatUntil, sendTestMessage} from '../test_util.js';

import {remoteCall} from './background.js';
import {DirectoryTreePageObject} from './page_objects/directory_tree.js';


/**
 * Waits for the empty folder element to show and assert the content to match
 * the expected message.
 * @param appId Files app windowId.
 * @param expectedMessage The expected empty folder message
 */
async function waitForEmptyFolderMessage(
    appId: string, expectedMessage: string) {
  const caller = getCaller();
  // Use repeatUntil() here because when we switch between different filters,
  // the message changes but the element itself will always show there.
  await repeatUntil(async () => {
    const emptyMessage = await remoteCall.waitForElement(
        appId, '#empty-folder:not(.hidden) > .label');
    if (emptyMessage && emptyMessage.text?.startsWith(expectedMessage)) {
      return;
    }

    return pending(
        caller,
        `Expected empty folder message: "${expectedMessage}", got "${
            emptyMessage.text}"`);
  });
}

/**
 * Tests that when local files are disabled, we navigate to default set by the
 * policy, e.g. Drive after unmounting a USB.
 */
export async function skyVaultLocalFilesDisabledUnmountRemovable() {
  // Mount Drive and Downloads.
  await sendTestMessage({name: 'mountDrive'});
  await sendTestMessage({name: 'mountDownloads'});
  // Ensure two volumes are mounted.
  await remoteCall.waitForVolumesCount(2);

  // Enable SkyVault.
  await sendTestMessage(
      {name: 'skyvault:setLocalFilesEnabled', enabled: false});
  await sendTestMessage(
      {name: 'skyvault:setDefaultLocation', defaultLocation: 'google_drive'});

  // Open Files app without specifying the initial directory/root.
  const appId = await remoteCall.openNewWindow(null, null);
  chrome.test.assertTrue(!!appId, 'failed to open new window');

  // Confirm that the Files App opened in Google Drive, as set by the policy.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/My Drive');

  // Mount USB volume in the Downloads window.
  await sendTestMessage({name: 'mountFakeUsb'});

  // Wait for the USB mount and click to open the USB volume.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.selectItemByType('removable');
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/fake-usb');

  // Unmount the USB.
  await sendTestMessage({name: 'unmountUsb'});

  // We should navigate to My Drive.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/My Drive');
}

/**
 * Tests that disabling local storage while in a local folder navigates away to
 * the default set by the policy, e.g. Drive.
 */
export async function skyVaultLocalFilesDisableInMyFiles() {
  // Mount Drive and Downloads.
  await sendTestMessage({name: 'mountDrive'});
  await sendTestMessage({name: 'mountDownloads'});

  // Doesn't have effect while local files are enabled.
  await sendTestMessage(
      {name: 'skyvault:setDefaultLocation', defaultLocation: 'google_drive'});

  // Once local files are disabled, prevent local volumes from being added as
  // read-only.
  await sendTestMessage({name: 'skyvault:skipMigration'});

  // Open Files app without specifying the initial directory/root.
  const appId = await remoteCall.openNewWindow(null, null);
  chrome.test.assertTrue(!!appId, 'failed to open new window');

  // Confirm that the Files App opened in MyFiles.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/My files');

  // Disable local storage: should remove MyFiles and use GoogleDrive as
  // default.
  await sendTestMessage(
      {name: 'skyvault:setLocalFilesEnabled', enabled: false});

  // We should navigate to Drive.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/My Drive');
}

/**
 * Tests that disabling local storage removes all local volumes, adds a
 * placeholder OneDrive volume since it's not mounted, and navigates to it as
 * the new default.
 */
export async function skyVaultOneDrivePlaceholder() {
  // Mount Downloads.
  await sendTestMessage({name: 'mountDownloads'});
  await remoteCall.waitForVolumesCount(1);

  // Set OneDrive as the default. Must be done before the dialog is opened, but
  // won't have effect while local files are enabled.
  await sendTestMessage({
    name: 'skyvault:setDefaultLocation',
    defaultLocation: 'microsoft_onedrive',
  });

  // Once local files are disabled, prevent local volumes from being added as
  // read-only.
  await sendTestMessage({name: 'skyvault:skipMigration'});

  // Open Files app without specifying the initial directory/root.
  const appId = await remoteCall.openNewWindow(null, null);
  chrome.test.assertTrue(!!appId, 'failed to open new window');

  // Confirm that the Files App opened in MyFiles.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/My files');

  // Confirm that OneDrive isn't shown yet.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  const oneDriveLabel = 'Microsoft OneDrive';
  directoryTree.waitForItemLostByLabel(oneDriveLabel);

  // Disable local storage.
  await sendTestMessage(
      {name: 'skyvault:setLocalFilesEnabled', enabled: false});

  // Check that the placeholder is added.
  await directoryTree.waitForItemByLabel(oneDriveLabel);

  // Files App should switch to the placeholder as the default.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId, `/${oneDriveLabel}`);

  // Check: the empty folder should be visible.
  await remoteCall.waitForElement(appId, '#empty-folder:not([hidden])');
  await waitForEmptyFolderMessage(appId, 'You\'ve been logged out');
}

/**
 * Tests that having no volumes (other than Recent), like when SkyVault is
 * misconfigured (no local storage, no cloud selected as alternative) shows an
 * error message.
 */
export async function skyVaultFileSystemDisabled() {
  // Disable local storage.
  await sendTestMessage(
      {name: 'skyvault:setLocalFilesEnabled', enabled: false});
  // Disable drive.
  await sendTestMessage({name: 'setDriveEnabled', enabled: false});
  // Skip SkyVault migration to immediately remove My Files.
  await sendTestMessage({name: 'skyvault:skipMigration'});

  // Open Files app without specifying the initial directory/root.
  const appId = await remoteCall.openNewWindow(null, null);
  chrome.test.assertTrue(!!appId, 'failed to open new window');

  // Check: the empty folder should be visible.
  await remoteCall.waitForElement(appId, '#empty-folder:not([hidden])');
  await waitForEmptyFolderMessage(
      appId, 'The file system has been disabled by your administrator');

  // Mount USB volume.
  await sendTestMessage({name: 'mountFakeUsb'});

  // Check: the empty folder should hide.
  await remoteCall.waitForElement(appId, '#empty-folder[hidden]');

  // Check: MyFiles shouldn't appear.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  await directoryTree.waitForItemLostByLabel('My files');
}

/**
 * Tests that the files migrating to cloud banner appears when MyFiles is opened
 * while SkyVault migration is enabled.
 */
export async function skyVaultMigrationToGoogleDrive() {
  await remoteCall.setLocalFilesMigrationDestination('google_drive');
  // Mount the local folder after setting the policies.
  await sendTestMessage({name: 'skyvault:mountMyFiles'});
  await sendTestMessage({name: 'skyvault:addLocalFiles'});

  // Open Files app without specifying the initial directory/root.
  const appId = await remoteCall.openNewWindow(null, null);
  chrome.test.assertTrue(!!appId, 'failed to open new window');

  await remoteCall.isolateBannerForTesting(
      appId, 'files-migrating-to-cloud-banner');

  // We should navigate to MyFiles.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/My files');

  // Check: the migration banner should be visible.
  await remoteCall.waitForElement(
      appId, '#banners > files-migrating-to-cloud-banner');

  // Check: the read-only indicator on toolbar is visible.
  await remoteCall.waitForElement(appId, '#read-only-indicator:not([hidden])');

  // Check: the delete button should be visible if the file is selected.
  await remoteCall.waitUntilSelected(appId, ENTRIES.hello.targetPath);
  await remoteCall.waitForElement(appId, '#delete-button:not([hidden])');
}

/**
 * Tests that the files migrating to cloud banner appears when MyFiles is opened
 * while SkyVault migration is enabled.
 */
export async function skyVaultMigrationToOneDrive() {
  await remoteCall.setLocalFilesMigrationDestination('microsoft_onedrive');
  // Mount the local folder after setting the policies.
  await sendTestMessage({name: 'skyvault:mountMyFiles'});
  await sendTestMessage({name: 'skyvault:addLocalFiles'});

  // Open Files app without specifying the initial directory/root.
  const appId = await remoteCall.openNewWindow(null, null);
  await remoteCall.isolateBannerForTesting(
      appId, 'files-migrating-to-cloud-banner');

  // We should navigate to MyFiles.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/My files');

  // Check: the migration banner should be visible.
  await remoteCall.waitForElement(
      appId, '#banners > files-migrating-to-cloud-banner');

  // Check: the read-only indicator on toolbar is visible.
  await remoteCall.waitForElement(appId, '#read-only-indicator:not([hidden])');

  // Check: the delete button should be visible if the file is selected.
  await remoteCall.waitUntilSelected(appId, ENTRIES.hello.targetPath);
  await remoteCall.waitForElement(appId, '#delete-button:not([hidden])');
}

/**
 * Tests that MyFiles volume is removed after the migration if it completes
 * while the Files App is opened.
 */
export async function skyVaultMigrationRemovesMyFiles() {
  // Enable migration.
  await remoteCall.setLocalFilesMigrationDestination('google_drive');

  // Open Files app without specifying the initial directory/root.
  const appId = await remoteCall.openNewWindow(null, null);
  chrome.test.assertTrue(!!appId, 'failed to open new window');

  // Confirm that MyFiles is visible.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  const myFilesLabel = 'My files';
  await directoryTree.waitForItemByLabel(myFilesLabel);

  // Skip SkyVault migration to immediately remove My Files.
  await sendTestMessage({name: 'skyvault:skipMigration'});
  await directoryTree.waitForItemLostByLabel(myFilesLabel);
}

/**
 * Tests that MyFiles volume is removed after the migration, even if MyFiles is
 * opened afterwards.
 */
export async function skyVaultMigrationRemovesMyFilesOpenAfter() {
  // Enable migration.
  await remoteCall.setLocalFilesMigrationDestination('google_drive');
  // Skip SkyVault migration to immediately remove My Files.
  await sendTestMessage({name: 'skyvault:skipMigration'});

  // Open Files app without specifying the initial directory/root.
  const appId = await remoteCall.openNewWindow(null, null);
  chrome.test.assertTrue(!!appId, 'failed to open new window');

  // Confirm that MyFiles isn't visible.
  const directoryTree = await DirectoryTreePageObject.create(appId);
  const myFilesLabel = 'My files';
  await directoryTree.waitForItemLostByLabel(myFilesLabel);
}
