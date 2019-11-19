// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Tests sharing a file on Drive
 */
testcase.suggestAppDialog = async () => {
  await sendTestMessage({
    name: 'expectFileTask',
    fileNames: ['unsupported.foo'],
    openType: 'launch'
  });

  // Fetch the mock CWS page data.
  const data =
      JSON.parse(await sendTestMessage({name: 'getCwsWidgetContainerMockUrl'}));

  // Override the container URL with the mock.
  const appState = {
    suggestAppsDialogState: {
      overrideCwsContainerUrlForTest: data.url,
      overrideCwsContainerOriginForTest: data.origin
    }
  };

  // Open Files app.
  const appId = await setupAndWaitUntilReady(
      RootPath.DRIVE, [], BASIC_DRIVE_ENTRY_SET, appState);

  // Select a file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'selectFile', appId, ['unsupported.foo']));

  // Double-click the file.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'fakeMouseDoubleClick', appId,
      ['#file-list li.table-row[selected] .filename-label span']));

  // Wait until the widget is loaded.
  chrome.test.assertTrue(!!await remoteCall.waitForElement(
      appId, '#suggest-app-dialog webview[src]'));

  // Wait until the widget is initialized.
  chrome.test.assertTrue(!!await remoteCall.waitForElement(
      appId, '.cws-widget-spinner-layer:not(.cws-widget-show-spinner)'));

  // Override task APIs for test.
  chrome.test.assertTrue(!!await remoteCall.callRemoteTestUtil(
      'overrideTasks', appId, [[{
        driveApp: false,
        iconUrl: 'chrome://theme/IDR_DEFAULT_FAVICON',  // Dummy icon
        isDefault: true,
        taskId: 'dummytaskid|drive|open-with',
        title: 'The dummy task for test'
      }]]));

  // Override installWebstoreItem API for test.
  chrome.test.assertTrue(!!remoteCall.callRemoteTestUtil(
      'overrideInstallWebstoreItemApi', appId,
      [
        'DUMMY_ITEM_ID_FOR_TEST',  // Same ID in cws_container_mock/main.js.
        null                       // Success
      ]));

  // Initiate an installation from the widget.
  chrome.test.assertTrue(
      !!await remoteCall.callRemoteTestUtil('executeScriptInWebView', appId, [
        '#suggest-app-dialog webview',
        'document.querySelector("button").click()'
      ]));

  // Wait until the installation is finished and the dialog is closed.
  chrome.test.assertTrue(
      !!await remoteCall.waitForElementLost(appId, '#suggest-app-dialog'));

  // Wait until the task is executed.
  await remoteCall.waitUntilTaskExecutes(appId, 'dummytaskid|drive|open-with');
};
