// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Query used to find USB removable volume.
 */
const USB_VOLUME_QUERY = '#directory-tree [volume-type-icon="removable"]';

/**
 * Opens two window of given root paths.
 * @param {string} rootPath1 Root path of the first window.
 * @param {string} rootPath2 Root path of the second window.
 * @return {Promise} Promise fulfilled with an array containing two window IDs.
 */
function openTwoWindows(rootPath1, rootPath2) {
  return Promise.all([
    openNewWindow(null, rootPath1),
    openNewWindow(null, rootPath2)
  ]).then(function(windowIds) {
    return Promise.all([
      remoteCall.waitForElement(windowIds[0], '#detail-table'),
      remoteCall.waitForElement(windowIds[1], '#detail-table'),
    ]).then(function() {
      return windowIds;
    });
  });
}

/**
 * Copies a file between two windows.
 * @param {string} window1 ID of the source window.
 * @param {string} window2 ID of the destination window.
 * @param {TestEntryInfo} file Test entry info to be copied.
 * @return {Promise} Promise fulfilled on success.
 */
function copyBetweenWindows(window1, window2, file, alreadyPresentFile = null) {
  if (!file || !file.nameText)
    chrome.test.assertTrue(false, 'copyBetweenWindows invalid file name');

  const flag = {ignoreLastModifiedTime: true};
  const name = file.nameText;

  return remoteCall.waitForFiles(window1, [file.getExpectedRow()])
      .then(function() {
        return remoteCall.callRemoteTestUtil('fakeMouseClick', window1, []);
      })
      .then(function() {
        return remoteCall.callRemoteTestUtil('selectFile', window1, [name]);
      })
      .then(function(result) {
        if (!result)
          chrome.test.assertTrue(false, 'Failed: selectFile ' + name);
        return remoteCall.callRemoteTestUtil('execCommand', window1, ['copy']);
      })
      .then(function() {
        return remoteCall.callRemoteTestUtil('fakeMouseClick', window2, []);
      })
      .then(function() {
        return remoteCall.callRemoteTestUtil('execCommand', window2, ['paste']);
      })
      .then(function() {
        var expectedFiles = [file.getExpectedRow()];
        if (alreadyPresentFile) {
          expectedFiles.push(alreadyPresentFile.getExpectedRow());
        }
        return remoteCall.waitForFiles(window2, expectedFiles, flag);
      });
}

/**
 * Tests file copy+paste from Drive to Downloads.
 */
testcase.copyBetweenWindowsDriveToLocal = function() {
  var window1;
  var window2;
  StepsRunner.run([
    // Open two Files app windows.
    function() {
      openTwoWindows(RootPath.DOWNLOADS, RootPath.DRIVE).then(this.next);
    },
    // Add files.
    function(appIdArray) {
      window1 = appIdArray[0];
      window2 = appIdArray[1];
      Promise
          .all([
            addEntries(['drive'], [ENTRIES.hello]),
            addEntries(['local'], [ENTRIES.photos]),
          ])
          .then(this.next);
    },
    // Check: Downloads photos file.
    function() {
      remoteCall.waitForFiles(window1, [ENTRIES.photos.getExpectedRow()])
          .then(this.next);
    },
    // Copy Drive hello file to Downloads.
    function() {
      copyBetweenWindows(window2, window1, ENTRIES.hello, ENTRIES.photos)
          .then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests file copy+paste from Downloads to Drive.
 */
testcase.copyBetweenWindowsLocalToDrive = function() {
  var window1;
  var window2;
  StepsRunner.run([
    // Open two Files app windows.
    function() {
      openTwoWindows(RootPath.DOWNLOADS, RootPath.DRIVE).then(this.next);
    },
    // Add files.
    function(appIdArray) {
      window1 = appIdArray[0];
      window2 = appIdArray[1];
      Promise
          .all([
            addEntries(['local'], [ENTRIES.hello]),
            addEntries(['drive'], [ENTRIES.photos]),
          ])
          .then(this.next);
    },
    // Check: Downloads hello file and Drive photos file.
    function() {
      remoteCall.waitForFiles(window2, [ENTRIES.photos.getExpectedRow()])
          .then(this.next);
    },
    // Copy Downloads hello file to Drive.
    function() {
      copyBetweenWindows(window1, window2, ENTRIES.hello, ENTRIES.photos)
          .then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests file copy+paste from Drive to USB.
 */
testcase.copyBetweenWindowsDriveToUsb = function() {
  var window1;
  var window2;
  StepsRunner.run([
    // Add photos to Downloads.
    function() {
      addEntries(['local'], [ENTRIES.photos], this.next);
    },
    // Check: photos was added.
    function(result) {
      if (!result)
        chrome.test.assertTrue(false, 'Failed: adding Downloads photos');
      // Open two Files app windows.
      openTwoWindows(RootPath.DOWNLOADS, RootPath.DRIVE).then(this.next);
    },
    // Check: Drive window is empty.
    function(appIdArray) {
      window1 = appIdArray[0];
      window2 = appIdArray[1];
      remoteCall.waitForFiles(window2, []).then(this.next);
    },
    // Click to switch back to the Downloads window.
    function() {
      remoteCall.callRemoteTestUtil('fakeMouseClick', window1, [], this.next);
    },
    // Check: Downloads window is showing photos.
    function() {
      remoteCall.waitForFiles(window1, [ENTRIES.photos.getExpectedRow()])
          .then(this.next);
    },
    // Mount an empty USB volume in the Downloads window.
    function() {
      sendTestMessage({name: 'mountFakeUsbEmpty'}).then(this.next);
    },
    // Wait for the USB mount.
    function() {
      remoteCall.waitForElement(window1, USB_VOLUME_QUERY).then(this.next);
    },
    // Click to open the USB volume.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', window1, [USB_VOLUME_QUERY], this.next);
    },
    // Check: Downloads window is showing an empty USB volume.
    function() {
      remoteCall.waitForFiles(window1, []).then(this.next);
    },
    // Add hello file to Drive.
    function() {
      addEntries(['drive'], [ENTRIES.hello], this.next);
    },
    // Check Drive hello file, copy it to USB.
    function() {
      copyBetweenWindows(window2, window1, ENTRIES.hello).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests file copy+paste from Downloads to USB.
 */
testcase.copyBetweenWindowsLocalToUsb = function() {
  var window1;
  var window2;
  StepsRunner.run([
    // Add photos to Drive.
    function() {
      addEntries(['drive'], [ENTRIES.photos], this.next);
    },
    // Check: photos was added.
    function(result) {
      if (!result)
        chrome.test.assertTrue(false, 'Failed: adding Drive photos');
      // Open two Files app windows.
      openTwoWindows(RootPath.DRIVE, RootPath.DOWNLOADS).then(this.next);
    },
    // Check: Downloads window is empty.
    function(appIdArray) {
      window1 = appIdArray[0];
      window2 = appIdArray[1];
      remoteCall.waitForFiles(window2, []).then(this.next);
    },
    // Click to switch back to the Drive window.
    function() {
      remoteCall.callRemoteTestUtil('fakeMouseClick', window1, [], this.next);
    },
    // Check: Drive window is showing photos.
    function() {
      remoteCall.waitForFiles(window1, [ENTRIES.photos.getExpectedRow()])
          .then(this.next);
    },
    // Mount an empty USB volume in the Drive window.
    function() {
      chrome.test.sendMessage(
          JSON.stringify({name: 'mountFakeUsbEmpty'}), this.next);
    },
    // Wait for the USB mount.
    function() {
      remoteCall.waitForElement(window1, USB_VOLUME_QUERY).then(this.next);
    },
    // Click to open the USB volume.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', window1, [USB_VOLUME_QUERY], this.next);
    },
    // Check: Drive window is showing an empty USB volume.
    function() {
      remoteCall.waitForFiles(window1, []).then(this.next);
    },
    // Add hello file to Downloads.
    function() {
      addEntries(['local'], [ENTRIES.hello], this.next);
    },
    // Check Downloads hello file, copy it to USB.
    function() {
      copyBetweenWindows(window2, window1, ENTRIES.hello).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests file copy+paste from USB to Drive.
 */
testcase.copyBetweenWindowsUsbToDrive = function() {
  var window1;
  var window2;
  StepsRunner.run([
    // Add photos to Downloads.
    function() {
      addEntries(['local'], [ENTRIES.photos], this.next);
    },
    // Check: photos was added.
    function(result) {
      if (!result)
        chrome.test.assertTrue(false, 'Failed: adding Downloads photos');
      // Open two Files app windows.
      openTwoWindows(RootPath.DOWNLOADS, RootPath.DRIVE).then(this.next);
    },
    // Check: Drive window is empty.
    function(appIdArray) {
      window1 = appIdArray[0];
      window2 = appIdArray[1];
      remoteCall.waitForFiles(window2, []).then(this.next);
    },
    // Click to switch back to the Downloads window.
    function() {
      remoteCall.callRemoteTestUtil('fakeMouseClick', window1, [], this.next);
    },
    // Check: Downloads window is showing photos.
    function() {
      remoteCall.waitForFiles(window1, [ENTRIES.photos.getExpectedRow()])
          .then(this.next);
    },
    // Mount an empty USB volume in the Downloads window.
    function() {
      chrome.test.sendMessage(
          JSON.stringify({name: 'mountFakeUsbEmpty'}), this.next);
    },
    // Wait for the USB mount.
    function() {
      remoteCall.waitForElement(window1, USB_VOLUME_QUERY).then(this.next);
    },
    // Click to open the USB volume.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', window1, [USB_VOLUME_QUERY], this.next);
    },
    // Check: Downloads window is showing an empty USB volume.
    function() {
      remoteCall.waitForFiles(window1, []).then(this.next);
    },
    // Add hello file to the USB volume.
    function() {
      addEntries(['usb'], [ENTRIES.hello], this.next);
    },
    // Check USB hello file, copy it to Drive.
    function() {
      copyBetweenWindows(window1, window2, ENTRIES.hello).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

/**
 * Tests file copy+paste from USB to Downloads.
 */
testcase.copyBetweenWindowsUsbToLocal = function() {
  var window1;
  var window2;
  StepsRunner.run([
    // Add photos to Drive.
    function() {
      addEntries(['drive'], [ENTRIES.photos], this.next);
    },
    // Check: photos was added.
    function(result) {
      if (!result)
        chrome.test.assertTrue(false, 'Failed: adding Drive photos');
      // Open two Files app windows.
      openTwoWindows(RootPath.DRIVE, RootPath.DOWNLOADS).then(this.next);
    },
    // Check: Downloads window is empty.
    function(appIdArray) {
      window1 = appIdArray[0];
      window2 = appIdArray[1];
      remoteCall.waitForFiles(window2, []).then(this.next);
    },
    // Click to switch back to the Drive window.
    function() {
      remoteCall.callRemoteTestUtil('fakeMouseClick', window1, [], this.next);
    },
    // Check: Drive window is showing photos.
    function() {
      remoteCall.waitForFiles(window1, [ENTRIES.photos.getExpectedRow()])
          .then(this.next);
    },
    // Mount an empty USB volume in the Drive window.
    function() {
      chrome.test.sendMessage(
          JSON.stringify({name: 'mountFakeUsbEmpty'}), this.next);
    },
    // Wait for the USB mount.
    function() {
      remoteCall.waitForElement(window1, USB_VOLUME_QUERY).then(this.next);
    },
    // Click to open the USB volume.
    function() {
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', window1, [USB_VOLUME_QUERY], this.next);
    },
    // Check: Drive window is showing an empty USB volume.
    function() {
      remoteCall.waitForFiles(window1, []).then(this.next);
    },
    // Add hello file to the USB volume.
    function() {
      addEntries(['usb'], [ENTRIES.hello], this.next);
    },
    // Check USB hello file, copy it to Downloads.
    function() {
      copyBetweenWindows(window1, window2, ENTRIES.hello).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};
