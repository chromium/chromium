// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ENTRIES, RootPath} from '../test_util.js';
import {testcase} from '../testcase.js';

import {IGNORE_APP_ERRORS, remoteCall, setupAndWaitUntilReady} from './background.js';

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

/**
 * Tests that clicking on a Guest OS entry in the sidebar triggers a mount
 * event, and an error is returned.
 * TODO(crbug/1293229): Scan errors don't seem to show up in the UI any more.
 * Need to fix that, then update this test to check for the expected error.
 */
testcase.mountGuestError = async () => {
  // Open the files app.
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Browsertest base registers some mock Guest OSs, wait for one to appear and
  // click it.
  const query = '#directory-tree [root-type-icon=guest_os]';
  await remoteCall.waitAndClickElement(appId, query);

  // Check that the Guest is selected. Guests are listed alphabetically so
  // Electra should be first.
  // TODO(crbug/1293229): It looks like scan errors are no longer surfaced in
  // the UI, I remember they used to be? Need to figure out surfacing errors and
  // then we check that instead.
  await remoteCall.waitForElement(appId, '#breadcrumbs[path=Electra]');

  // We expect there to be an error, from the mount failure.
  return IGNORE_APP_ERRORS;
};
