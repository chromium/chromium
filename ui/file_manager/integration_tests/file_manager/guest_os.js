// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, RootPath, sendTestMessage} from '../test_util.js';
import {testcase} from '../testcase.js';

import {mountCrostini, remoteCall, setupAndWaitUntilReady} from './background.js';

/**
 * Tests that Guest OS entries don't show up if the flag controlling guest os +
 * files app integration is disabled.
 */
testcase.notListedWithoutFlag = async () => {
  // Open the files app.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Check that we have no Guest OS entries.
  const query = '#directory-tree [root-type-icon=guest_os]';
  await remoteCall.waitForElementsCount(appId, [query], 0);
};

/**
 * Tests that Guest OS entries show up in the sidebar at files app launch.
 */
testcase.fakesListed = async () => {
  // Open the files app.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Browsertest base registers two mock Guest OSs, check that they're both
  // listed.
  const query = '#directory-tree [root-type-icon=guest_os]';
  await remoteCall.waitForElementsCount(appId, [query], 2);
};