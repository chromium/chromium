// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, RootPath} from '../test_util.js';
import {testcase} from '../testcase.js';

import {remoteCall, setupAndWaitUntilReady} from './background.js';

/** Tests that the Backspace key navigates to parent directory.  */
testcase.navigateToParent = async () => {
  // Open Files app on local Downloads.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // It should start in Downloads.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId, '/My files/Downloads');

  // Send the Backspace key to the file list.
  const backspaceKey = ['#file-list', 'Backspace', false, false, false];
  await remoteCall.fakeKeyDown(appId, ...backspaceKey);

  // It should navigate to the parent.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/My files');
};
