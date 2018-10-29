// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Tests sharing a file on Drive
 */
testcase.suggestAppDialog = function() {
  var appId;
  StepsRunner.run([
    // Set up File Manager.
    function() {
      sendTestMessage({name: 'getCwsWidgetContainerMockUrl'}).then(this.next);
    },
    // Override the container URL with the mock.
    function(json) {
      var data = JSON.parse(json);

      var appState = {
        suggestAppsDialogState: {
          overrideCwsContainerUrlForTest: data.url,
          overrideCwsContainerOriginForTest: data.origin
        }
      };
      setupAndWaitUntilReady(appState, RootPath.DRIVE, this.next);
    },
    function(results) {
      appId = results.windowId;

      remoteCall.callRemoteTestUtil(
          'selectFile', appId, ['unsupported.foo'], this.next);
    },
    // Double-click the file.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.callRemoteTestUtil(
          'fakeMouseDoubleClick',
          appId,
          ['#file-list li.table-row[selected] .filename-label span'],
          this.next);
    },
    // Wait for the widget is loaded.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#suggest-app-dialog webview[src]').
          then(this.next);
    },
    // Wait for the widget is initialized.
    function(result) {
      chrome.test.assertTrue(!!result);
      remoteCall.waitForElement(
          appId, '.cws-widget-spinner-layer:not(.cws-widget-show-spinner)').
          then(this.next);
    },
    // Override task APIs for test.
    function(result) {
      chrome.test.assertTrue(!!result);
      remoteCall.callRemoteTestUtil(
          'overrideTasks',
          appId,
          [[
            {
              driveApp: false,
              iconUrl: 'chrome://theme/IDR_DEFAULT_FAVICON',  // Dummy icon
              isDefault: true,
              taskId: 'dummytaskid|drive|open-with',
              title: 'The dummy task for test'
            }
          ]],
          this.next);
    },
    // Override installWebstoreItem API for test.
    function(result) {
      chrome.test.assertTrue(!!result);
      remoteCall.callRemoteTestUtil(
          'overrideInstallWebstoreItemApi',
          appId,
          [
            'DUMMY_ITEM_ID_FOR_TEST',  // Same ID in cws_container_mock/main.js.
            null  // Success
          ],
          this.next);
    },
    // Initiate an installation from the widget.
    function(result) {
      chrome.test.assertTrue(!!result);
      remoteCall.callRemoteTestUtil(
          'executeScriptInWebView',
          appId,
          ['#suggest-app-dialog webview',
           'document.querySelector("button").click()'],
          this.next);
    },
    // Wait until the installation is finished and the dialog is closed.
    function(result) {
      chrome.test.assertTrue(!!result);
      remoteCall.waitForElementLost(appId, '#suggest-app-dialog').
          then(this.next);
    },
    // Wait until the task is executed.
    function(result) {
      chrome.test.assertTrue(!!result);
      remoteCall.waitUntilTaskExecutes(appId, 'dummytaskid|drive|open-with').
          then(this.next);
    },
    // Check error
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};
