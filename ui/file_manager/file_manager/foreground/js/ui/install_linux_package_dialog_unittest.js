// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {InstallLinuxPackageDialog} from './install_linux_package_dialog.js';

export function testInstallButtonHiddenUntilInfoReady() {
  // Polyfill chrome.app.window.current().
  /** @suppress {checkTypes,const} */
  // @ts-ignore: error TS2339: Property 'app' does not exist on type 'typeof
  // chrome'.
  chrome.app = {window: {current: () => null}};

  let getInfoCallback;
  /** @suppress {checkTypes,const} */
  // @ts-ignore: error TS2740: Type '{ getLinuxPackageInfo: (entry:
  // FileSystemEntry, callback: (arg0: LinuxPackageInfo) => void) => void; }' is
  // missing the following properties from type 'typeof fileManagerPrivate':
  // setPreferences, getDriveConnectionState, PreferencesChange,
  // DriveConnectionStateType, and 185 more.
  chrome.fileManagerPrivate = {
    // @ts-ignore: error TS6133: 'entry' is declared but its value is never
    // read.
    getLinuxPackageInfo: (entry, callback) => {
      getInfoCallback = callback;
    },
  };

  const info = {name: 'n', version: 'v', info: 'i', summary: 's'};
  const dialog = new InstallLinuxPackageDialog(document.body);

  // Show dialog and verify that the install button is disabled.
  dialog.showInstallLinuxPackageDialog(/** @type {!Entry} */ ({}));
  const installButton = document.querySelector('.cr-dialog-ok');
  // @ts-ignore: error TS2339: Property 'disabled' does not exist on type
  // 'Element'.
  assertTrue(installButton.disabled);

  // The install button should become enabled once info is ready.
  // @ts-ignore: error TS2722: Cannot invoke an object which is possibly
  // 'undefined'.
  getInfoCallback(info);
  // @ts-ignore: error TS2339: Property 'disabled' does not exist on type
  // 'Element'.
  assertFalse(installButton.disabled);
}
