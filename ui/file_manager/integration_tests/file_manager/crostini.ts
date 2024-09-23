// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {type ElementObject} from '../prod/file_manager/shared_types.js';
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
      '#banners > shared-with-crostini-pluginvm-banner:not([hidden])';

  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.photos], []);

  await remoteCall.isolateBannerForTesting(
      appId, 'shared-with-crostini-pluginvm-banner');

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
      appId, '#banners > shared-with-crostini-pluginvm-banner');
  remoteCall.callRemoteTestUtil('fakeMouseDoubleClick', appId, [photos]);
  await remoteCall.waitForElement(appId, shareMessageShown);
}

export async function pluginVmDirectoryNotSharedErrorDialog() {
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);
  const pluginVmAppDescriptor = {
    appId: 'plugin-vm-app-id',
    taskType: 'pluginvm',
    actionId: 'open-with',
  };

  // Override the tasks so the "Open with Plugin VM App" button becomes a
  // dropdown option.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'overrideTasks', appId, [[
        {
          descriptor: {
            appId: 'text-app-id',
            taskType: 'app',
            actionId: 'text',
          },
          title: 'Text',
        },
        {
          descriptor: pluginVmAppDescriptor,
          title: 'App (Windows)',
        },
      ]]));

  // Right click on 'hello.txt' file, and wait for dialog with 'Open with'.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId,
      ['[id^="listitem-"][file-name="hello.txt"]']);

  // Click 'Open with'.
  await remoteCall.waitAndClickElement(
      appId, 'cr-menu-item[command="#open-with"]:not([hidden])');

  // Wait for app picker.
  await remoteCall.waitForElement(appId, '#tasks-menu:not([hidden])');

  // Ensure app picker shows Plugin VM option.
  const appOptions = await remoteCall.callRemoteTestUtil<ElementObject[]>(
      'queryAllElements', appId, ['#tasks-menu [tabindex]']);
  chrome.test.assertEq(
      1, appOptions.filter(el => el.text === 'App (Windows)').length);

  // Click on the Plugin VM app, and wait for error dialog.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId,
      [`#tasks-menu [tabindex]:nth-of-type(${
          appOptions.map((el: ElementObject) => el.text)
              .indexOf('App (Windows)') +
          1})`]);
  await remoteCall.waitUntilTaskExecutes(
      appId, pluginVmAppDescriptor, ['hello.txt'],
      ['failed_plugin_vm_directory_not_shared']);
  await remoteCall.waitForElement(
      appId, '.cr-dialog-frame:not(#default-task-dialog):not([hidden])');

  // Validate error messages.
  const dialogTitles = await remoteCall.callRemoteTestUtil<ElementObject[]>(
      'queryAllElements', appId,
      ['.cr-dialog-frame:not(#default-task-dialog) .cr-dialog-title']);
  const dialogTexts = await remoteCall.callRemoteTestUtil<ElementObject[]>(
      'queryAllElements', appId,
      ['.cr-dialog-frame:not(#default-task-dialog) .cr-dialog-text']);

  chrome.test.assertEq([''], dialogTitles.map(el => el.text));
  chrome.test.assertEq(
      ['To open files with App (Windows), ' +
       'first move them to the Windows files folder.'],
      dialogTexts.map(el => el.text));

  // TODO(crbug.com/40117686): Test file is moved. This can only be tested when
  // tests allow creating /MyFiles/PvmDefault.
}

export async function pluginVmFileOnExternalDriveErrorDialog() {
  // Use files outside of MyFiles to show 'copy' rather than 'move'.
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DRIVE);
  const pluginVmAppDescriptor = {
    appId: 'plugin-vm-app-id',
    taskType: 'pluginvm',
    actionId: 'open-with',
  };

  // Override the tasks so the "Open with Plugin VM App" button becomes a
  // dropdown option.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'overrideTasks', appId, [[
        {
          descriptor: {
            appId: 'text-app-id',
            taskType: 'app',
            actionId: 'text',
          },
          title: 'Text',
        },
        {
          descriptor: pluginVmAppDescriptor,
          title: 'App (Windows)',
        },
      ]]));

  // Right click on 'hello.txt' file, and wait for dialog with 'Open with'.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId,
      ['[id^="listitem-"][file-name="hello.txt"]']);

  // Click 'Open with'.
  await remoteCall.waitAndClickElement(
      appId, 'cr-menu-item[command="#open-with"]:not([hidden])');

  // Wait for app picker.
  await remoteCall.waitForElement(appId, '#tasks-menu:not([hidden])');

  // Ensure app picker shows Plugin VM option.
  const appOptions = await remoteCall.callRemoteTestUtil<ElementObject[]>(
      'queryAllElements', appId, ['#tasks-menu [tabindex]']);
  chrome.test.assertEq(
      1, appOptions.filter(el => el.text === 'App (Windows)').length);

  // Click on the Plugin VM app, and wait for error dialog.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId,
      [`#tasks-menu [tabindex]:nth-of-type(${
          appOptions.map(el => el.text).indexOf('App (Windows)') + 1})`]);
  await remoteCall.waitUntilTaskExecutes(
      appId, pluginVmAppDescriptor, ['hello.txt'],
      ['failed_plugin_vm_directory_not_shared']);
  await remoteCall.waitForElement(
      appId, '.cr-dialog-frame:not(#default-task-dialog):not([hidden])');

  // Validate error messages.
  const dialogTitles = await remoteCall.callRemoteTestUtil<ElementObject[]>(
      'queryAllElements', appId,
      ['.cr-dialog-frame:not(#default-task-dialog) .cr-dialog-title']);
  const dialogTexts = await remoteCall.callRemoteTestUtil<ElementObject[]>(
      'queryAllElements', appId,
      ['.cr-dialog-frame:not(#default-task-dialog) .cr-dialog-text']);

  chrome.test.assertEq([''], dialogTitles.map(el => el.text));
  chrome.test.assertEq(
      ['To open files with App (Windows), ' +
       'first copy them to the Windows files folder.'],
      dialogTexts.map(el => el.text));

  // TODO(crbug.com/40117686): Test file is moved. This can only be tested when
  // tests allow creating /MyFiles/PvmDefault.
}

/**
 * Tests that when drag from Files app and dropping in the Plugin VM a
 * dialog is displayed if the containing folder isn't shared with Plugin VM.
 */
export async function pluginVmFileDropFailErrorDialog() {
  const appId = await remoteCall.setupAndWaitUntilReady(RootPath.DOWNLOADS);

  // Select 'hello.txt' file.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId, ['[id^="listitem-"][file-name="hello.txt"]']);

  // Send 'dragstart'.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeEvent', appId, ['body', 'dragstart', {bubbles: true}]));

  // Send CrostiniEvent 'drop_failed_plugin_vm_directory_not_shared'.
  await sendTestMessage({name: 'onDropFailedPluginVmDirectoryNotShared'});

  // Wait for error dialog.
  await remoteCall.waitForElement(
      appId, '.cr-dialog-frame:not(#default-task-dialog):not([hidden])');
}
