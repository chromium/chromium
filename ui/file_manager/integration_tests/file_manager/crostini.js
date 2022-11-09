// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, RootPath, sendTestMessage} from '../test_util.js';
import {testcase} from '../testcase.js';

import {mountCrostini, remoteCall, setupAndWaitUntilReady} from './background.js';

const FAKE_LINUX_FILES = '#directory-tree [root-type-icon="crostini"]';
const REAL_LINUX_FILES = '#directory-tree [volume-type-icon="crostini"]';

testcase.mountCrostini = async () => {

  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  await mountCrostini(appId);

  // Unmount and ensure fake root is shown.
  remoteCall.callRemoteTestUtil('unmount', null, ['crostini']);
  await remoteCall.waitForElement(appId, FAKE_LINUX_FILES);
};

testcase.enableDisableCrostini = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Ensure fake Linux files root is shown.
  await remoteCall.waitForElement(appId, FAKE_LINUX_FILES);

  // Disable Crostini, then ensure fake Linux files is removed.
  await sendTestMessage({name: 'setCrostiniEnabled', enabled: false});
  await remoteCall.waitForElementLost(appId, FAKE_LINUX_FILES);

  // Re-enable Crostini, then ensure fake Linux files is shown again.
  await sendTestMessage({name: 'setCrostiniEnabled', enabled: true});
  await remoteCall.waitForElement(appId, FAKE_LINUX_FILES);
};

testcase.sharePathWithCrostini = async () => {
  const downloads = '#directory-tree [volume-type-icon="downloads"]';
  const photos = '#file-list [file-name="photos"]';
  const menuShareWithLinux = '#file-context-menu:not([hidden]) ' +
      '[command="#share-with-linux"]:not([hidden]):not([disabled])';
  const menuNoShareWithLinux = '#file-context-menu:not([hidden]) ' +
      '[command="#share-with-linux"][hidden][disabled="disabled"]';
  const shareMessageShown =
      '#banners > shared-with-crostini-pluginvm-banner:not([hidden])';

  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.photos], []);

  await remoteCall.isolateBannerForTesting(
      appId, 'shared-with-crostini-pluginvm-banner');

  // Ensure fake Linux files root is shown.
  await remoteCall.waitForElement(appId, FAKE_LINUX_FILES);

  // Mount crostini, and ensure real root is shown.
  remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [FAKE_LINUX_FILES]);
  await remoteCall.waitForElement(appId, REAL_LINUX_FILES);

  // Go back to downloads, wait for photos dir to be shown.
  remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [downloads]);
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
};

testcase.pluginVmDirectoryNotSharedErrorDialog = async () => {
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);
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
  const appOptions = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId, ['#tasks-menu [tabindex]']);
  chrome.test.assertEq(
      1, appOptions.filter(el => el.text == 'App (Windows)').length);

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
  const dialogTitles = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId,
      ['.cr-dialog-frame:not(#default-task-dialog) .cr-dialog-title']);
  const dialogTexts = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId,
      ['.cr-dialog-frame:not(#default-task-dialog) .cr-dialog-text']);

  chrome.test.assertEq([''], dialogTitles.map(el => el.text));
  chrome.test.assertEq(
      ['To open files with App (Windows), ' +
       'first move them to the Windows files folder.'],
      dialogTexts.map(el => el.text));

  // TODO(crbug.com/1049453): Test file is moved. This can only be tested when
  // tests allow creating /MyFiles/PvmDefault.
};

testcase.pluginVmFileOnExternalDriveErrorDialog = async () => {
  // Use files outside of MyFiles to show 'copy' rather than 'move'.
  const appId = await setupAndWaitUntilReady(RootPath.DRIVE);
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
  const appOptions = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId, ['#tasks-menu [tabindex]']);
  chrome.test.assertEq(
      1, appOptions.filter(el => el.text == 'App (Windows)').length);

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
  const dialogTitles = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId,
      ['.cr-dialog-frame:not(#default-task-dialog) .cr-dialog-title']);
  const dialogTexts = await remoteCall.callRemoteTestUtil(
      'queryAllElements', appId,
      ['.cr-dialog-frame:not(#default-task-dialog) .cr-dialog-text']);

  chrome.test.assertEq([''], dialogTitles.map(el => el.text));
  chrome.test.assertEq(
      ['To open files with App (Windows), ' +
       'first copy them to the Windows files folder.'],
      dialogTexts.map(el => el.text));

  // TODO(crbug.com/1049453): Test file is moved. This can only be tested when
  // tests allow creating /MyFiles/PvmDefault.
};

/**
 * Tests that when drag from Files app and dropping in the Plugin VM a
 * dialog is displayed if the containing folder isn't shared with Plugin VM.
 */
testcase.pluginVmFileDropFailErrorDialog = async () => {
  const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS);

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
};
