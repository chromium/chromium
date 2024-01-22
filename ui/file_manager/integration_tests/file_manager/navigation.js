// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, RootPath} from '../test_util.js';
import {testcase} from '../testcase.js';

import {remoteCall, setupAndWaitUntilReady} from './background.js';

/** Tests that the Backspace key navigates to parent directory.  */
// @ts-ignore: error TS4111: Property 'navigateToParent' comes from an index
// signature, so it must be accessed with ['navigateToParent'].
testcase.navigateToParent = async () => {
  // Open Files app on local Downloads.
  const appId =
      // @ts-ignore: error TS4111: Property 'beautiful' comes from an index
      // signature, so it must be accessed with ['beautiful'].
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.beautiful], []);

  // It should start in Downloads.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(
      appId, '/My files/Downloads');

  // Send the Backspace key to the file list.
  const backspaceKey = ['#file-list', 'Backspace', false, false, false];
  // @ts-ignore: error TS2556: A spread argument must either have a tuple type
  // or be passed to a rest parameter.
  await remoteCall.fakeKeyDown(appId, ...backspaceKey);

  // It should navigate to the parent.
  await remoteCall.waitUntilCurrentDirectoryIsChanged(appId, '/My files');
};
