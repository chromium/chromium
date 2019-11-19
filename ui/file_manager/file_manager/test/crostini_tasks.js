// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const crostiniTasks = {};

crostiniTasks.testErrorLoadingLinuxPackageInfo = async (done) => {
  const fakeRoot = '#directory-tree [root-type-icon="crostini"]';
  const dialog = '#install-linux-package-dialog';
  const detailsFrame = '.install-linux-package-details-frame';

  // Save old fmp.getFileTasks and replace with version that returns
  // the internal linux package install handler
  let oldGetFileTasks = chrome.fileManagerPrivate.getFileTasks;
  chrome.fileManagerPrivate.getFileTasks = (entries, callback) => {
    setTimeout(
        callback, 0, [{
          taskId: `${test.FILE_MANAGER_EXTENSION_ID}|app|install-linux-package`,
          title: 'Install with Linux (Beta)',
          verb: 'open_with',
          isDefault: true,
        }]);
  };

  // Save old fmp.getLinuxPackageInfo and replace with version that saves the
  // callback to manually call later
  let oldGetLinuxPackageInfo = chrome.fileManagerPrivate.getLinuxPackageInfo;
  let packageInfoCallback = null;
  chrome.fileManagerPrivate.getLinuxPackageInfo = (entry, callback) => {
    packageInfoCallback = callback;
  };

  await test.setupAndWaitUntilReady([], [], [test.ENTRIES.debPackage]);
  await test.waitForElement(fakeRoot);

  // Select 'Linux files' in directory tree.
  assertTrue(test.fakeMouseClick(fakeRoot), 'click Linux files');
  await test.waitForFiles(
      test.TestEntryInfo.getExpectedRows([test.ENTRIES.debPackage]));

  // Double click on 'package.deb' file to open the install dialog.
  assertTrue(test.fakeMouseDoubleClick('[file-name="package.deb"]'));
  await test.waitForElement(dialog);

  // Verify the loading state is shown.
  assertEquals(
      'Details\nLoading information...',
      document.querySelector(detailsFrame).innerText);
  await test.repeatUntil(() => {
    return packageInfoCallback ||
        test.pending('Waiting for package info request');
  });

  // Call the callback with an error.
  chrome.runtime.lastError = {message: 'error message'};
  packageInfoCallback(undefined);
  delete chrome.runtime.lastError;
  assertEquals(
      'Details\nFailed to retrieve app info.',
      document.querySelector(detailsFrame).innerText);

  // Click 'cancel' to close.  Ensure dialog closes.
  assertTrue(test.fakeMouseClick('button.cr-dialog-cancel'));
  await test.waitForElementLost(dialog);

  // Restore fmp.getFileTasks, fmp.getLinuxPackageInfo.
  chrome.fileManagerPrivate.getFileTasks = oldGetFileTasks;
  chrome.fileManagerPrivate.getLinuxPackageInfo = oldGetLinuxPackageInfo;
  chrome.fileManagerPrivate.removeMount('crostini');
  await test.waitForElement(fakeRoot);
  done();
};

crostiniTasks.testImportCrostiniImage = async (done) => {
  const dialog = '.cr-dialog-container.shown';

  // Save old fmp.importCrostiniImage function.
  const oldImportCrostiniImage = chrome.fileManagerPrivate.importCrostiniImage;
  let importCrostiniImageCalled = false;
  chrome.fileManagerPrivate.importCrostiniImage = (entry) => {
    importCrostiniImageCalled = true;
  };

  // Save old fmp.getFileTasks and replace with version that returns the
  // internal import crostini image handler.
  const oldGetFileTasks = chrome.fileManagerPrivate.getFileTasks;
  chrome.fileManagerPrivate.getFileTasks = (entries, callback) => {
    setTimeout(
        callback, 0, [{
          taskId: `${test.FILE_MANAGER_EXTENSION_ID}|app|import-crostini-image`,
        }]);
  };

  await test.setupAndWaitUntilReady([test.ENTRIES.tiniFile]);

  assertTrue(test.fakeMouseDoubleClick('[file-name="test.tini"]'));
  await test.waitForElement(dialog);

  assertTrue(test.fakeMouseClick('button.cr-dialog-ok'));
  await test.waitForElementLost(dialog);

  assertTrue(importCrostiniImageCalled);

  // Restore fmp.getFileTasks, fmp.importCrostiniImage.
  chrome.fileManagerPrivate.getFileTasks = oldGetFileTasks;
  chrome.fileManagerPrivate.importCrostiniImage = oldImportCrostiniImage;
  done();
};
