// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const crostiniMount = {};

crostiniMount.testCrostiniNotEnabled = async (done) => {
  const fakeRoot = '#directory-tree [root-type-icon="crostini"]';
  await test.setupAndWaitUntilReady();
  chrome.fileManagerPrivate.onCrostiniChanged.dispatchEvent(
      {eventType: 'disable', vmName: 'termina'});
  await test.waitForElementLost(fakeRoot);

  // Reset crostini back to default enabled=true.
  chrome.fileManagerPrivate.onCrostiniChanged.dispatchEvent(
      {eventType: 'enable', vmName: 'termina'});
  await test.waitForElement(fakeRoot);
  done();
};

crostiniMount.testMountCrostiniSuccess = async (done) => {
  const fakeRoot = '#directory-tree [root-type-icon="crostini"]';
  const oldMount = chrome.fileManagerPrivate.mountCrostini;
  let mountCallback = null;
  chrome.fileManagerPrivate.mountCrostini = (callback) => {
    mountCallback = callback;
  };
  await test.setupAndWaitUntilReady();

  // Linux files fake root is shown.
  await test.waitForElement(fakeRoot);

  // Click on Linux files.
  assertTrue(test.fakeMouseClick(fakeRoot, 'click linux files'));
  await test.waitForElement('paper-progress:not([hidden])');

  // Ensure mountCrostini is called.
  await test.repeatUntil(() => {
    if (!mountCallback) {
      return test.pending('Waiting for mountCrostini');
    }
    return mountCallback;
  });

  // Intercept the fileManagerPrivate.mountCrostini call
  // and add crostini disk mount.
  test.mountCrostini();
  // Continue from fileManagerPrivate.mountCrostini callback
  // and ensure expected files are shown.
  mountCallback();
  await test.waitForFiles(
      test.TestEntryInfo.getExpectedRows(test.BASIC_CROSTINI_ENTRY_SET));

  // Reset fileManagerPrivate.mountCrostini and remove mount.
  chrome.fileManagerPrivate.mountCrostini = oldMount;
  chrome.fileManagerPrivate.removeMount('crostini');
  // Linux Files fake root is shown.
  await test.waitForElement(fakeRoot);

  // MyFiles folder should be shown when crostini goes away.
  await test.waitForFiles(test.TestEntryInfo.getExpectedRows(
      test.BASIC_MY_FILES_ENTRY_SET_WITH_LINUX_FILES));

  done();
};

crostiniMount.testMountCrostiniError = async (done) => {
  const fakeRoot = '#directory-tree [root-type-icon="crostini"]';
  const oldMount = chrome.fileManagerPrivate.mountCrostini;
  // Override fileManagerPrivate.mountCrostini to return error.
  chrome.fileManagerPrivate.mountCrostini = (callback) => {
    chrome.runtime.lastError = {message: 'test message'};
    callback();
    delete chrome.runtime.lastError;
  };
  await test.setupAndWaitUntilReady();
  await test.waitForElement(fakeRoot);

  // Click on Linux Files, ensure error dialog is shown.
  assertTrue(test.fakeMouseClick(fakeRoot));
  await test.waitForElement('.cr-dialog-container.shown');

  // Click OK button to close.
  assertTrue(test.fakeMouseClick('button.cr-dialog-ok'));
  await test.waitForElementLost('.cr-dialog-container');

  // Reset chrome.fileManagerPrivate.mountCrostini.
  chrome.fileManagerPrivate.mountCrostini = oldMount;
  done();
};

crostiniMount.testCrostiniMountOnDrag = async (done) => {
  const fakeRoot = '#directory-tree [root-type-icon="crostini"]';
  chrome.fileManagerPrivate.mountCrostiniDelay_ = 0;
  await test.setupAndWaitUntilReady();
  await test.waitForElement(fakeRoot);
  assertTrue(test.sendEvent(fakeRoot, new Event('dragenter', {bubbles: true})));
  assertTrue(test.sendEvent(fakeRoot, new Event('dragleave', {bubbles: true})));
  await test.waitForFiles(
      test.TestEntryInfo.getExpectedRows(test.BASIC_CROSTINI_ENTRY_SET));
  chrome.fileManagerPrivate.removeMount('crostini');
  await test.waitForElement(fakeRoot);
  done();
};
