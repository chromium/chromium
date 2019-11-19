// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

const FAKE_LINUX_FILES = '#directory-tree [root-type-icon="crostini"]';
const REAL_LINUX_FILES = '#directory-tree [volume-type-icon="crostini"]';

testcase.mountCrostini = async () => {

  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  await mountCrostini(appId);

  // Unmount and ensure fake root is shown.
  remoteCall.callRemoteTestUtil('unmount', null, ['crostini']);
  await remoteCall.waitForElement(appId, FAKE_LINUX_FILES);
};

testcase.enableDisableCrostini = async () => {
  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.hello], []);

  // Ensure fake Linux files root is shown.
  await remoteCall.waitForElement(appId, FAKE_LINUX_FILES);

  // Disable Crostini, then ensure fake Linux files is removed.
  await sendTestMessage({name: 'setCrostiniEnabled', enabled: false});
  await remoteCall.waitForElementLost(appId, FAKE_LINUX_FILES);

  // Re-enable Crostini, then ensure fake Linux files is shown again.
  await sendTestMessage({name: 'setCrostiniEnabled', enabled: true});
  await remoteCall.waitForElement(appId, FAKE_LINUX_FILES);
};

testcase.sharePathWithCrostini = async () => {
  const downloads = '#directory-tree [volume-type-icon="downloads"]';
  const photos = '#file-list [file-name="photos"]';
  const menuShareWithLinux = '#file-context-menu:not([hidden]) ' +
      '[command="#share-with-linux"]:not([hidden]):not([disabled])';
  const menuNoShareWithLinux = '#file-context-menu:not([hidden]) ' +
      '[command="#share-with-linux"][hidden][disabled="disabled"]';

  const appId =
      await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Ensure fake Linux files root is shown.
  await remoteCall.waitForElement(appId, FAKE_LINUX_FILES);

  // Mount crostini, and ensure real root is shown.
  remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [FAKE_LINUX_FILES]);
  await remoteCall.waitForElement(appId, REAL_LINUX_FILES);

  // Go back to downloads, wait for photos dir to be shown.
  remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [downloads]);
  await remoteCall.waitForElement(appId, photos);

  // Right-click 'photos' directory, ensure 'Share with Linux' is shown.
  remoteCall.callRemoteTestUtil('fakeMouseRightClick', appId, [photos]);
  await remoteCall.waitForElement(appId, menuShareWithLinux);

  // Click on 'Share with Linux', ensure menu is closed.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseClick', appId,
      ['#file-context-menu [command="#share-with-linux"]']);
  await remoteCall.waitForElement(appId, '#file-context-menu[hidden]');

  // Right-click 'photos' directory, ensure 'Share with Linux' is not shown.
  await remoteCall.callRemoteTestUtil(
      'fakeMouseRightClick', appId, ['#file-list [file-name="photos"']);
  await remoteCall.waitForElement(appId, menuNoShareWithLinux);
};
