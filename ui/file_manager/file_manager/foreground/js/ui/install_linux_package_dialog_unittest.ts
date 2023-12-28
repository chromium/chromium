// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {installMockChrome} from '../../../common/js/mock_chrome.js';

import {InstallLinuxPackageDialog} from './install_linux_package_dialog.js';

export function testInstallButtonHiddenUntilInfoReady() {
  let getInfoCallback:
      Parameters<typeof chrome.fileManagerPrivate.getLinuxPackageInfo>[1] =
          () => {};
  installMockChrome({
    fileManagerPrivate: {
      getLinuxPackageInfo: (_, callback) => {
        getInfoCallback = callback;
      },
    },
  });

  const info: chrome.fileManagerPrivate.LinuxPackageInfo =
      {name: 'n', version: 'v', summary: 's', description: 'd'};
  const dialogElement = document.createElement('dialog');
  document.body.append(dialogElement);
  const dialog = new InstallLinuxPackageDialog(dialogElement);

  // Show dialog and verify that the install button is disabled.
  dialog.showInstallLinuxPackageDialog({} as Entry);
  const installButton =
      document.querySelector<HTMLButtonElement>('.cr-dialog-ok')!;
  assertTrue(installButton.disabled);

  // The install button should become enabled once info is ready.
  getInfoCallback(info);
  assertFalse(installButton.disabled);
}
