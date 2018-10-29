// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

testcase.installLinuxPackageDialog = function() {
  const fake = '#directory-tree .tree-item [root-type-icon="crostini"]';
  const real = '#directory-tree .tree-item [volume-type-icon="crostini"]';
  // The dialog has an INSTALL and OK button, both as .cr-dialog-ok, but only
  // one is visible at a time.
  const dialog = '#install-linux-package-dialog';
  const okButton = dialog + ' .cr-dialog-ok:not([hidden])';

  let appId;

  StepsRunner.run([
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
    // Add entries to crostini volume, but do not mount.
    function(results) {
      appId = results.windowId;
      addEntries(['crostini'], [ENTRIES.debPackage], this.next);
    },
    // Linux files fake root is shown.
    function() {
      remoteCall.waitForElement(appId, fake).then(this.next);
    },
    // Mount crostini, and ensure real root and files are shown.
    function() {
      remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [fake]);
      remoteCall.waitForElement(appId, real).then(this.next);
    },
    function() {
      const files = TestEntryInfo.getExpectedRows([ENTRIES.debPackage]);
      remoteCall.waitForFiles(appId, files).then(this.next);
    },
    // Open the deb package.
    function() {
      remoteCall.callRemoteTestUtil(
          'openFile', appId, [ENTRIES.debPackage.targetPath], this.next);
    },
    // Ensure package install dialog is shown.
    function() {
      remoteCall.waitForElement(appId, dialog).then(this.next);
    },
    function() {
      repeatUntil(function() {
        return remoteCall
            .callRemoteTestUtil(
                'queryAllElements', appId,
                ['.install-linux-package-details-frame'])
            .then(function(elements) {
              // The details are in separate divs on multiple lines, which the
              // test api returns as a single string. These values come from
              // fake_cicerone_client.cc.
              return elements[0] &&
                  elements[0].text ==
                      ('Details' +
                       'Application: Fake Package' +
                       'Version: 1.0' +
                       'Description: A package that is fake') ||
                  pending('Waiting for installation to start.');
            });
      }).then(this.next);
    },
    // Begin installation.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [okButton], this.next);
    },
    // Wait for the installation to start (under test, we use a fake D-Bus
    // client, so it doesn't actually install anything).
    function() {
      repeatUntil(function() {
        return remoteCall
            .callRemoteTestUtil('queryAllElements', appId, ['.cr-dialog-text'])
            .then(function(elements) {
              return elements[0] &&
                  elements[0].text == 'Installation successfully started.' ||
                  pending('Waiting for installation to start.');
            });
      }).then(this.next);
    },
    // Dismiss dialog
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, [okButton], this.next);
    },
    // Ensure dialog closes
    function() {
      remoteCall.waitForElementLost(appId, dialog).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    },
  ]);
};
