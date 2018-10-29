// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

function generateVerifyRecentsSteps(expectedRecents = RECENT_ENTRY_SET) {
  let appId;
  return [
    // Navigate to Recents.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil(
          'fakeMouseClick', appId, ['span[root-type-icon=\'recent\']'],
          this.next);
    },
    // Verify Recents contains the expected files - those with an mtime in the
    // future.
    function(result) {
      chrome.test.assertTrue(result);
      const files = TestEntryInfo.getExpectedRows(expectedRecents);
      remoteCall.waitForFiles(appId, files).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ];
}

testcase.recentsDownloads = function() {
  StepsRunner.run([
    // Populate downloads.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next, BASIC_LOCAL_ENTRY_SET, []);
    },
  ].concat(generateVerifyRecentsSteps()));
};

testcase.recentsDrive = function() {
  StepsRunner.run([
    // Populate drive.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DRIVE, this.next, [], BASIC_DRIVE_ENTRY_SET);
    },
  ].concat(generateVerifyRecentsSteps()));
};

testcase.recentsDownloadsAndDrive = function() {
  StepsRunner.run([
    // Populate both downloads and drive with disjoint sets of files.
    function() {
      setupAndWaitUntilReady(
          null, RootPath.DOWNLOADS, this.next,
          [ENTRIES.beautiful, ENTRIES.hello, ENTRIES.photos],
          [ENTRIES.desktop, ENTRIES.world, ENTRIES.testDocument]);
    },
  ].concat(generateVerifyRecentsSteps()));
};

testcase.recentsDownloadsAndDriveWithOverlap = function() {
  StepsRunner.run([
    // Populate both downloads and drive with overlapping sets of files.
    function() {
      setupAndWaitUntilReady(null, RootPath.DOWNLOADS, this.next);
    },
  ].concat(generateVerifyRecentsSteps(RECENT_ENTRY_SET
                                          .concat(RECENT_ENTRY_SET))));
};
