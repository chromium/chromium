// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertFalse, assertTrue} from 'chrome://test/chai_assert.js';

import {InstallLinuxPackageDialog} from './install_linux_package_dialog.m.js';

export function testInstallButtonHiddenUntilInfoReady() {
  // Polyfill chrome.app.window.current().
  /** @suppress {checkTypes,const} */
  chrome.app = {window: {current: () => null}};

  loadTimeData.data = {};

  let getInfoCallback;
  /** @suppress {checkTypes,const} */
  chrome.fileManagerPrivate = {
    getLinuxPackageInfo: (entry, callback) => {
      getInfoCallback = callback;
    }
  };
  const container =
      assertInstanceof(document.createElement('div'), HTMLElement);

  const info = {name: 'n', version: 'v', info: 'i', summary: 's'};
  const dialog = new InstallLinuxPackageDialog(container);

  // Show dialog and very that install button is disabled.
  dialog.showInstallLinuxPackageDialog(/** @type {!Entry} */ ({}));
  assertTrue(container.querySelector('.cr-dialog-ok').disabled);

  // Button becomes enabled once info is ready.
  getInfoCallback(info);
  assertFalse(container.querySelector('.cr-dialog-ok').disabled);
}
