// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, RootPath} from '../test_util.js';

import {remoteCall} from './background.js';


/** Tests that the Backspace key navigates to parent directory.  */
export async function navigateToParent() {
  // Open Files app on local Downloads.
  const appId = await remoteCall.setupAndWaitUntilReady(
      RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // It should start in Downloads.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId, '/My files/Downloads');

  // Send the Backspace key to the file list.
  await remoteCall.fakeKeyDown(
      appId, '#file-list', 'Backspace', false, false, false);

  // It should navigate to the parent.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/My files');
}
